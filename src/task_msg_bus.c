/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-22     sly_ant      the first version
 */

#include "task_msg_bus.h"

#define DBG_TAG "task.msg.bus"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_bool_t task_msg_bus_init_tag = RT_FALSE;
static struct rt_semaphore msg_sem;
static struct rt_mutex msg_lock;
static struct rt_mutex msg_ref_lock;
static struct rt_mutex cb_lock;
static struct rt_mutex sub_lock;
static struct rt_mutex wt_lock;
static rt_slist_t callback_slist_array[TASK_MSG_COUNT];
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
static struct task_msg_dup_release_hook dup_release_hooks[TASK_MSG_COUNT] = task_msg_dup_release_hooks;
#endif
static rt_slist_t msg_slist = RT_SLIST_OBJECT_INIT(msg_slist);
static rt_slist_t msg_ref_slist = RT_SLIST_OBJECT_INIT(msg_ref_slist);
static rt_slist_t msg_subscriber_slist = RT_SLIST_OBJECT_INIT(msg_subscriber_slist);
static rt_slist_t msg_wait_slist = RT_SLIST_OBJECT_INIT(msg_wait_slist);
static rt_uint32_t subscriber_id = 0;

/**
 * Append a message reference to the slist:msg_ref_slist,
 * each message reference are added only once.
 *
 * @param args: message reference
 */
static void msg_ref_append(task_msg_args_t args)
{
    rt_bool_t is_first_append = RT_TRUE;
    task_msg_ref_node_t item;
    rt_mutex_take(&msg_ref_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(item, &msg_ref_slist, slist)
    {
        if (item->args == args)
        {
            is_first_append = RT_FALSE;
            item->ref_count++;
            break;
        }
    }
    if (is_first_append)
    {
        task_msg_ref_node_t node = rt_calloc(1, sizeof(struct task_msg_ref_node));
        if (node == RT_NULL)
        {
            rt_mutex_release(&msg_ref_lock);
            LOG_E("there is no memory available!");
            return;
        }
        node->args = args;
        node->ref_count = 1;
        rt_slist_init(&(node->slist));
        rt_slist_append(&msg_ref_slist, &(node->slist));
    }
    rt_mutex_release(&msg_ref_lock);
}

/**
 * Release a message reference from the slist:msg_ref_slist,
 * only when the subscribers of all messages have consumed,
 * can they really free from memory.
 *
 * @param args: message reference
 */
void task_msg_release(task_msg_args_t args)
{
    task_msg_ref_node_t item;
    rt_mutex_take(&msg_ref_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(item, &msg_ref_slist, slist)
    {
        if (item->args == args)
        {
            item->ref_count--;
            if (item->ref_count <= 0)
            {
                rt_slist_remove(&msg_ref_slist, &(item->slist));
                if (item->args->msg_obj)
                {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
                    if (dup_release_hooks[item->args->msg_name].release)
                    {
                        RT_ASSERT(dup_release_hooks[item->args->msg_name].msg_name == item->args->msg_name);
                        dup_release_hooks[item->args->msg_name].release(item->args->msg_obj);
                    }
#endif
                    rt_free(item->args->msg_obj);
                }
                rt_free(item->args);
                rt_free(item);
            }
            break;
        }
    }
    rt_mutex_release(&msg_ref_lock);
}

/**
 * Create a subscriber.
 * @param msg_name: message name
 * @return create failed return -1,otherwise return >=0
 */
int task_msg_subscriber_create(enum task_msg_name msg_name)
{
    if (task_msg_bus_init_tag == RT_FALSE)
        return -1;

    task_msg_subscriber_node_t subscriber = (task_msg_subscriber_node_t) rt_calloc(1,
            sizeof(struct task_msg_subscriber_node));
    if (subscriber == RT_NULL)
        return -1;

    int id = subscriber_id++;
    char name[RT_NAME_MAX];
    rt_snprintf(name, RT_NAME_MAX, "sub_%d", id);
    subscriber->sem = rt_sem_create(name, 0, RT_IPC_FLAG_PRIO);
    if (subscriber->sem == RT_NULL)
    {
        rt_free(subscriber);
        return -1;
    }

    subscriber->msg_name = msg_name;
    subscriber->subscriber_id = id;
    rt_slist_init(&(subscriber->slist));
    rt_mutex_take(&sub_lock, RT_WAITING_FOREVER);
    rt_slist_append(&msg_subscriber_slist, &(subscriber->slist));
    rt_mutex_release(&sub_lock);

    return id;
}

/**
 * Create a subscriber and allow multiple topics to be subscribed.
 * @param msg_name_list: message name array
 * @param msg_name_list_len: message name array length
 * @return create failed return -1,otherwise return >=0
 */
int task_msg_subscriber_create2(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len)
{
    if (task_msg_bus_init_tag == RT_FALSE)
        return -1;

    int id = subscriber_id++;
    char name[RT_NAME_MAX];
    rt_snprintf(name, RT_NAME_MAX, "sub_%d", id);
    rt_sem_t sem = rt_sem_create(name, 0, RT_IPC_FLAG_PRIO);
    if (sem == RT_NULL)
        return -1;

    int count = 0;
    rt_mutex_take(&sub_lock, RT_WAITING_FOREVER);
    for (int i = 0; i < msg_name_list_len; i++)
    {
        task_msg_subscriber_node_t subscriber = rt_calloc(1, sizeof(struct task_msg_subscriber_node));
        if (subscriber == RT_NULL)
        {
            goto ERROR;
        }

        subscriber->sem = sem;
        subscriber->msg_name = msg_name_list[i];
        subscriber->subscriber_id = id;
        rt_slist_init(&(subscriber->slist));
        rt_slist_append(&msg_subscriber_slist, &(subscriber->slist));
        count++;
    }

    if (count > 0)
    {
        rt_mutex_release(&sub_lock);
        return id;
    }

    ERROR: rt_sem_delete(sem);
    if (count > 0)
    {
        task_msg_subscriber_node_t subscriber;
        rt_slist_for_each_entry(subscriber, &msg_subscriber_slist, slist)
        {
            if (subscriber->subscriber_id == id)
            {
                rt_slist_remove(&msg_subscriber_slist, &(subscriber->slist));
                rt_free(subscriber);
            }
        }

    }
    rt_mutex_release(&sub_lock);
    return -1;
}

/**
 * Delete a subscriber.
 * @param subscriber_id: subscriber id
 */
void task_msg_subscriber_delete(int subscriber_id)
{
    task_msg_wait_node_t wait_node;
    task_msg_subscriber_node_t subscriber;
    rt_bool_t first_tag = RT_TRUE;

    rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(wait_node, &msg_wait_slist, slist)
    {
        if (wait_node->subscriber->subscriber_id == subscriber_id)
        {
            rt_slist_remove(&msg_wait_slist, &(wait_node->slist));
            task_msg_release(wait_node->args);
            rt_free(wait_node);
        }
    }

    rt_mutex_take(&sub_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(subscriber, &msg_subscriber_slist, slist)
    {
        if (subscriber->subscriber_id == subscriber_id)
        {
            rt_slist_remove(&msg_subscriber_slist, &(subscriber->slist));
            if (first_tag)
            {
                rt_sem_delete(subscriber->sem);
                first_tag = RT_FALSE;
            }
            rt_free(subscriber);
        }
    }
    rt_mutex_release(&sub_lock);
    rt_mutex_release(&wt_lock);
}

/**
 * Blocks the current thread until a message of the specified message name is received.
 *
 * @param subscriber_id: subscriber id
 * @param timeout_ms: the waiting millisecond (-1:waiting forever until get resource)
 * @param out_args: output parameter, return the received message reference address
 * @return error code
 */
rt_err_t task_msg_wait_until(int subscriber_id, rt_int32_t timeout_ms, struct task_msg_args **out_args)
{
    if (task_msg_bus_init_tag == RT_FALSE)
        return -RT_EINVAL;

    rt_err_t rst = -RT_ERROR;
    task_msg_subscriber_node_t subscriber;
    rt_sem_t sem = RT_NULL;

    rt_mutex_take(&sub_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(subscriber, &msg_subscriber_slist, slist)
    {
        if (subscriber->subscriber_id == subscriber_id)
        {
            sem = subscriber->sem;
            break;
        }
    }
    rt_mutex_release(&sub_lock);

    if (sem == RT_NULL)
    {
        rt_thread_mdelay(timeout_ms);
        return -RT_EINVAL;
    }

    rst = rt_sem_take(sem, timeout_ms);
    if (rst == RT_EOK)
    {
        rst = -RT_EINVAL;
        task_msg_wait_node_t wait_node;
        rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
        rt_slist_for_each_entry(wait_node, &msg_wait_slist, slist)
        {
            if (wait_node->subscriber->subscriber_id == subscriber_id)
            {
                *out_args = wait_node->args;
                rt_slist_remove(&msg_wait_slist, &(wait_node->slist));
                rt_free(wait_node);
                rst = RT_EOK;
                break;
            }
        }
        rt_mutex_release(&wt_lock);
    }

    return rst;
}

/**
 * Subscribe the message with the specified name and set the callback function.
 *
 * @param msg_name: message name
 * @param callback: callback function name
 * @return error code
 */
rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args))
{
    if (task_msg_bus_init_tag == RT_FALSE || callback == RT_NULL)
        return -RT_EINVAL;

    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    rt_bool_t find_tag = RT_FALSE;
    task_msg_callback_node_t node;
    rt_slist_for_each_entry(node, &callback_slist_array[msg_name], slist)
    {
        if (node->callback == callback)
        {
            find_tag = RT_TRUE;
            break;
        }
    }
    if (find_tag)
    {
        LOG_W("this task msg callback with msg_name[%d] is exist!", msg_name);
    }
    else
    {
        task_msg_callback_node_t callback_node = rt_calloc(1, sizeof(struct task_msg_callback_node));
        if (callback_node == RT_NULL)
        {
            rt_mutex_release(&cb_lock);
            LOG_E("there is no memory available!");
            return RT_ENOMEM;
        }
        callback_node->callback = callback;
        rt_slist_init(&(callback_node->slist));
        rt_slist_append(&callback_slist_array[msg_name], &(callback_node->slist));
    }
    rt_mutex_release(&cb_lock);

    return RT_EOK;
}

/**
 * Unsubscribe the message with the specified name and cancle the callback function.
 *
 * @param msg_name: message name
 * @param callback: callback function name
 * @return error code
 */
rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args))
{
    if (task_msg_bus_init_tag == RT_FALSE || callback == RT_NULL)
        return -RT_EINVAL;

    task_msg_callback_node_t node;
    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(node, &callback_slist_array[msg_name], slist)
    {
        if (node->callback == callback)
        {
            rt_slist_remove(&callback_slist_array[msg_name], &(node->slist));
            rt_free(node);
            break;
        }
    }
    rt_mutex_release(&cb_lock);

    return RT_EOK;
}

/**
 * Publish a message object.
 *
 * @param msg_name: message name
 * @param msg_obj: message object
 * @param msg_size: message size
 * @return error code
 */
rt_err_t task_msg_publish_obj(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size)
{
    if (task_msg_bus_init_tag == RT_FALSE)
        return -RT_EINVAL;

    task_msg_args_node_t node = rt_calloc(1, sizeof(struct task_msg_args_node));
    if (node == RT_NULL)
    {
        LOG_E("task msg publish failed! args_node create failed!");
        return -RT_ENOMEM;
    }

    task_msg_args_t msg_args = rt_calloc(1, sizeof(struct task_msg_args));
    if (msg_args == RT_NULL)
    {
        rt_free(node);
        LOG_E("task msg publish failed! msg_args create failed!");
        return -RT_ENOMEM;
    }

    msg_args->msg_name = msg_name;
    msg_args->msg_size = msg_size;
    msg_args->msg_obj = RT_NULL;
    if (msg_obj && msg_size > 0)
    {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
        if (dup_release_hooks[msg_name].dup)
        {
            RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
            msg_args->msg_obj = dup_release_hooks[msg_name].dup(msg_obj);
            if (msg_args->msg_obj == RT_NULL)
            {
                rt_free(node);
                rt_free(msg_args);
                LOG_E("task msg publish failed! msg_args create failed!");
                return -RT_ENOMEM;
            }
        }
        else
        {
            msg_args->msg_obj = rt_calloc(1, msg_size);
            if (msg_args->msg_obj)
            {
                rt_memcpy(msg_args->msg_obj, msg_obj, msg_size);
            }
            else
            {
                rt_free(node);
                rt_free(msg_args);
                LOG_E("task msg publish failed! msg_args create failed!");
                return -RT_ENOMEM;
            }
        }
#else
        msg_args->msg_obj = rt_calloc(1, msg_size);
        if (msg_args->msg_obj)
        {
            rt_memcpy(msg_args->msg_obj, msg_obj, msg_size);
        }
        else
        {
            rt_free(node);
            rt_free(msg_args);
            LOG_E("task msg publish failed! msg_args create failed!");
            return -RT_ENOMEM;
        }
#endif
    }
    node->args = msg_args;
    rt_slist_init(&(node->slist));
    rt_mutex_take(&msg_lock, RT_WAITING_FOREVER);
    rt_slist_append(&msg_slist, &(node->slist));
    rt_mutex_release(&msg_lock);

    rt_sem_release(&msg_sem);

    return RT_EOK;
}
/**
 * Publish a text message.
 *
 * @param msg_name: message name
 * @param msg_text: message text
 * @return error code
 */
rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *msg_text)
{
    void *msg_obj = (void *) msg_text;
    rt_size_t args_size = 0;
    if (msg_obj)
    {
        args_size = rt_strlen(msg_text) + 1;
    }
    return task_msg_publish_obj(msg_name, msg_obj, args_size);
}

static void msg_timing_timeout(void *params)
{
    task_msg_loop_t msg_timing = (task_msg_loop_t) params;
    task_msg_publish_obj(msg_timing->msg_name, msg_timing->msg_obj, msg_timing->msg_size);
    if (msg_timing->msg_obj)
    {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
        if (dup_release_hooks[msg_timing->msg_name].release)
        {
            RT_ASSERT(dup_release_hooks[msg_timing->msg_name].msg_name == msg_timing->msg_name)
            dup_release_hooks[msg_timing->msg_name].release(msg_timing->msg_obj);
        }
#endif
        rt_free(msg_timing->msg_obj);
        msg_timing->msg_obj = RT_NULL;
    }
    rt_timer_delete(msg_timing->timer);
    msg_timing->timer = RT_NULL;
    rt_free(msg_timing);
}
/**
 * Publish a delay message object.
 * @param delay_ms: delay ms
 * @param msg_name: message name
 * @param msg_obj: message object
 * @param msg_size: message size
 * @return error code
 */
rt_err_t task_msg_delay_publish_obj(rt_uint32_t delay_ms, enum task_msg_name msg_name, void *msg_obj,
        rt_size_t msg_size)
{
    task_msg_loop_t msg_loop = rt_calloc(1, sizeof(struct task_msg_loop));
    if (msg_loop == RT_NULL)
        return -RT_ENOMEM;

    msg_loop->msg_name = msg_name;
    msg_loop->msg_obj = RT_NULL;
    msg_loop->msg_size = 0;
    if (msg_obj && msg_size > 0)
    {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
        if (dup_release_hooks[msg_name].dup)
        {
            RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
            msg_loop->msg_obj = dup_release_hooks[msg_name].dup(msg_obj);
            if (msg_loop->msg_obj == RT_NULL)
            {
                rt_free(msg_loop);
                return -RT_ENOMEM;
            }
        }
        else
        {
            msg_loop->msg_obj = rt_calloc(1, msg_size);
            if (msg_loop->msg_obj == RT_NULL)
            {
                rt_free(msg_loop);
                return -RT_ENOMEM;
            }
            rt_memcpy(msg_loop->msg_obj, msg_obj, msg_size);
        }
#else
        msg_loop->msg_obj = rt_calloc(1, msg_size);
        if (msg_loop->msg_obj == RT_NULL)
        {
            rt_free(msg_loop);
            return -RT_ENOMEM;
        }
        rt_memcpy(msg_loop->msg_obj, msg_obj, msg_size);
#endif
        msg_loop->msg_size = msg_size;
    }
    char name[RT_NAME_MAX];
    rt_snprintf(name, RT_NAME_MAX, "delay%d", msg_name);
    msg_loop->timer = rt_timer_create(name, msg_timing_timeout, msg_loop, rt_tick_from_millisecond(delay_ms),
    RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_ONE_SHOT);
    if (msg_loop->timer == RT_NULL)
    {
        if (msg_loop->msg_obj)
        {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            if (dup_release_hooks[msg_name].release)
            {
                RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
                dup_release_hooks[msg_name].release(msg_obj);
            }
#endif
            rt_free(msg_loop->msg_obj);
        }
        rt_free(msg_loop);
        return -RT_ENOMEM;
    }

    rt_err_t rst = rt_timer_start(msg_loop->timer);
    if (rst != RT_EOK)
    {
        if (msg_loop->msg_obj)
        {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            if (dup_release_hooks[msg_name].release)
            {
                RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
                dup_release_hooks[msg_name].release(msg_obj);
            }
#endif
            rt_free(msg_loop->msg_obj);
        }
        rt_timer_delete(msg_loop->timer);
        rt_free(msg_loop);
    }

    return rst;
}
/**
 * Publish a delay text message.
 * @param delay_ms: delay ms
 * @param msg_name: message name
 * @param msg_text: message text
 * @return error code
 */
rt_err_t task_msg_delay_publish(rt_uint32_t delay_ms, enum task_msg_name msg_name, const char *msg_text)
{
    void *msg_obj = (void *) msg_text;
    rt_size_t args_size = 0;
    if (msg_obj)
    {
        args_size = rt_strlen(msg_text) + 1;
    }
    return task_msg_delay_publish_obj(delay_ms, msg_name, msg_obj, args_size);
}

static void msg_loop_timeout(void *params)
{
    task_msg_loop_t msg_loop = (task_msg_loop_t) params;
    task_msg_publish_obj(msg_loop->msg_name, msg_loop->msg_obj, msg_loop->msg_size);
}
/**
 * create a loop message
 * @return error code
 */
task_msg_loop_t task_msg_loop_create(void)
{
    task_msg_loop_t msg_loop = rt_calloc(1, sizeof(struct task_msg_loop));
    if (msg_loop == RT_NULL)
        return RT_NULL;

    msg_loop->timer = RT_NULL;
    msg_loop->msg_obj = RT_NULL;
    msg_loop->msg_size = 0;

    return msg_loop;
}
/**
 * start a loop message
 * @param msg_loop
 * @param delay_ms
 * @param msg_name
 * @param msg_obj
 * @param msg_size
 * @return error code
 */
rt_err_t task_msg_loop_start(task_msg_loop_t msg_loop, rt_uint32_t delay_ms, enum task_msg_name msg_name, void *msg_obj,
        rt_size_t msg_size)
{
    if (msg_loop == RT_NULL)
        return -RT_EEMPTY;

    if (msg_loop->timer == RT_NULL)
    {
        char name[RT_NAME_MAX];
        rt_snprintf(name, RT_NAME_MAX, "loop%d", msg_name);
        msg_loop->timer = rt_timer_create(name, msg_loop_timeout, msg_loop, rt_tick_from_millisecond(delay_ms),
        RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
        if (msg_loop->timer == RT_NULL)
        {
            return -RT_ENOMEM;
        }
    }
    else
    {
        rt_timer_stop(msg_loop->timer);
        rt_tick_t delay_tick = rt_tick_from_millisecond(delay_ms);
        rt_timer_control(msg_loop->timer, RT_TIMER_CTRL_SET_TIME, &delay_tick);
        if (msg_loop->msg_obj)
        {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            if (dup_release_hooks[msg_name].release)
            {
                RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
                dup_release_hooks[msg_name].release(msg_obj);
            }
#endif
            rt_free(msg_loop->msg_obj);
        }
    }
    msg_loop->msg_name = msg_name;
    msg_loop->msg_obj = RT_NULL;
    msg_loop->msg_size = 0;

    if (msg_obj && msg_size > 0)
    {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
        if (dup_release_hooks[msg_name].dup)
        {
            RT_ASSERT(dup_release_hooks[msg_name].msg_name == msg_name);
            msg_loop->msg_obj = dup_release_hooks[msg_name].dup(msg_obj);
            if (msg_loop->msg_obj == RT_NULL)
            {
                return -RT_ENOMEM;
            }
        }
        else
        {
            msg_loop->msg_obj = rt_calloc(1, msg_size);
            if (msg_loop->msg_obj == RT_NULL)
            {
                return -RT_ENOMEM;
            }
            rt_memcpy(msg_loop->msg_obj, msg_obj, msg_size);
        }
#else
        msg_loop->msg_obj = rt_calloc(1, msg_size);
        if (msg_loop->msg_obj == RT_NULL)
        {
            return -RT_ENOMEM;
        }
        rt_memcpy(msg_loop->msg_obj, msg_obj, msg_size);
#endif
        msg_loop->msg_size = msg_size;
    }

    return rt_timer_start(msg_loop->timer);
}

/**
 * stop a loop message
 * @param msg_loop
 * @return error code
 */
rt_err_t task_msg_loop_stop(task_msg_loop_t msg_loop)
{
    if (msg_loop == RT_NULL || msg_loop->timer == RT_NULL)
        return -RT_EEMPTY;

    return rt_timer_stop(msg_loop->timer);
}
/**
 * delete a loop message
 * @param msg_loop
 * @return error code
 */
rt_err_t task_msg_loop_delete(task_msg_loop_t msg_loop)
{
    rt_err_t rst;

    if (msg_loop == RT_NULL)
        return -RT_EEMPTY;

    if (msg_loop->timer)
    {
        rt_timer_stop(msg_loop->timer);
        rst = rt_timer_delete(msg_loop->timer);
        if (rst == RT_EOK)
            msg_loop->timer = RT_NULL;
    }
    if (rst == RT_EOK)
    {
        if (msg_loop->msg_obj)
        {
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            if (dup_release_hooks[msg_loop->msg_name].release)
            {
                RT_ASSERT(dup_release_hooks[msg_loop->msg_name].msg_name == msg_loop->msg_name);
                dup_release_hooks[msg_loop->msg_name].release(msg_loop->msg_obj);
            }
#endif
            rt_free(msg_loop->msg_obj);
        }
        msg_loop->msg_obj = RT_NULL;
        msg_loop->msg_size = 0;
        rt_free(msg_loop);
    }

    return rst;
}
/**
 * Initialize the callback slist array.
 */
static void task_msg_callback_init(void)
{
    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    for (int i = 0; i < TASK_MSG_COUNT; i++)
    {
        callback_slist_array[i].next = RT_NULL;
    }
    rt_mutex_release(&cb_lock);
}

/**
 * Task message bus thread entry.
 * @param params
 */
static void task_msg_bus_thread_entry(void *params)
{
    while (1)
    {
        if (rt_sem_take(&msg_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            task_msg_args_node_t msg_args_node;
            task_msg_callback_node_t msg_callback_node;
            task_msg_subscriber_node_t subscriber;
            task_msg_wait_node_t msg_wait_node;
            if (rt_slist_len(&msg_slist) > 0)
            {
                //get msg
                msg_args_node = rt_slist_first_entry(&msg_slist, struct task_msg_args_node, slist);
                msg_ref_append(msg_args_node->args);

                rt_mutex_take(&sub_lock, RT_WAITING_FOREVER);
                rt_slist_for_each_entry(subscriber, &msg_subscriber_slist, slist)
                {
                    if (subscriber->msg_name == msg_args_node->args->msg_name)
                    {
                        msg_wait_node = rt_calloc(1, sizeof(struct task_msg_wait_node));
                        if (msg_wait_node == RT_NULL)
                        {
                            LOG_W("no memory to create msg_wait_node!");
                            break;
                        }

                        msg_ref_append(msg_args_node->args);
                        msg_wait_node->subscriber = subscriber;
                        msg_wait_node->args = msg_args_node->args;
                        rt_slist_init(&(msg_wait_node->slist));
                        rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
                        rt_slist_append(&msg_wait_slist, &(msg_wait_node->slist));
                        rt_mutex_release(&wt_lock);

                        rt_sem_release(subscriber->sem);
                    }
                }
                rt_mutex_release(&sub_lock);

                //msg callback
                rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
                rt_slist_for_each_entry(msg_callback_node, &callback_slist_array[msg_args_node->args->msg_name], slist)
                {
                    if (msg_callback_node->callback)
                    {
                        msg_callback_node->callback(msg_args_node->args);
                    }
                }
                rt_mutex_release(&cb_lock);
                //remove msg
                rt_mutex_take(&msg_lock, RT_WAITING_FOREVER);
                rt_slist_remove(&msg_slist, &(msg_args_node->slist));
                rt_mutex_release(&msg_lock);
                //release msg
                task_msg_release(msg_args_node->args);
                rt_free(msg_args_node);
            }
        }
    }
}

/**
 * Initialize message bus components.
 *
 * @param stack_size: message bus thread stack size
 * @param priority: thread priority
 * @param tick: thread tick
 * @return error code
 */
int task_msg_bus_init(void)
{
    if (task_msg_bus_init_tag)
        return -RT_EBUSY;

    rt_sem_init(&msg_sem, "msg_sem", 0, RT_IPC_FLAG_FIFO);
    rt_mutex_init(&msg_lock, "msg_lock", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&msg_ref_lock, "ref_lock", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&cb_lock, "cb_lock", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&wt_lock, "wt_lock", RT_IPC_FLAG_FIFO);
    rt_mutex_init(&sub_lock, "sub_lock", RT_IPC_FLAG_FIFO);
    task_msg_callback_init();
    task_msg_bus_init_tag = RT_TRUE;

    rt_thread_t t = rt_thread_create("msg_bus", task_msg_bus_thread_entry,
    RT_NULL, TASK_MSG_THREAD_STACK_SIZE, TASK_MSG_THREAD_PRIORITY, 20);
    if (t == RT_NULL)
    {
        LOG_E("task msg bus initialize failed! msg_bus_thread create failed!");
        return -RT_ENOMEM;
    }

    rt_err_t rst = rt_thread_startup(t);
    if (rst == RT_EOK)
    {
        LOG_I("task msg bus initialize success!");
    }
    else
    {
        LOG_E("task msg bus initialize failed! msg_bus thread startup failed(%d)", rst);
    }
    return rst;
}
INIT_COMPONENT_EXPORT(task_msg_bus_init);
