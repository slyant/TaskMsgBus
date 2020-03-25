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
static struct rt_mutex wt_lock;
static struct rt_mutex wta_lock;
static rt_slist_t callback_slist_array[TASK_MSG_COUNT];
static rt_slist_t msg_slist = RT_SLIST_OBJECT_INIT(msg_slist);
static rt_slist_t msg_ref_slist = RT_SLIST_OBJECT_INIT(msg_ref_slist);
static rt_slist_t wait_slist = RT_SLIST_OBJECT_INIT(wait_slist);
static rt_slist_t wait_any_slist = RT_SLIST_OBJECT_INIT(wait_any_slist);

static void msg_ref_append(task_msg_args_t args)
{
    rt_bool_t is_first_append = RT_TRUE;
    task_msg_ref_node_t item;
    rt_mutex_take(&msg_ref_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(item, &msg_ref_slist, slist)
    {
        if(item->args==args)
        {
            is_first_append = RT_FALSE;
            item->ref_count++;
            break;
        }
    }
    if(is_first_append)
    {
        char name[RT_NAME_MAX];
        task_msg_ref_node_t node = rt_calloc(1, sizeof(struct task_msg_ref_node));
        if(node == RT_NULL)
        {
            rt_mutex_release(&msg_ref_lock);
            LOG_E("there is no memory available!");
            return;
        }
        rt_uint32_t count = rt_slist_len(&msg_ref_slist);
        rt_snprintf(name, RT_NAME_MAX, "ref_%d", count);
        node->args = args;
        node->ref_count = 1;
        rt_slist_init(&(node->slist));
        rt_slist_append(&msg_ref_slist, &(node->slist));
    }
    rt_mutex_release(&msg_ref_lock);
}

void task_msg_delete(task_msg_args_t args)
{
    task_msg_ref_node_t item;
    rt_mutex_take(&msg_ref_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(item, &msg_ref_slist, slist)
    {
        if(item->args==args)
        {
            item->ref_count--;
            if(item->ref_count <= 0)
            {
                rt_slist_remove(&msg_ref_slist, &(item->slist));
                if(item->args->msg_args_json)
                    rt_free(item->args->msg_args_json);
                rt_free(item->args);
                rt_free(item);
            }
            break;
        }
    }
    rt_mutex_release(&msg_ref_lock);
}

rt_err_t task_msg_wait_any(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len, rt_uint32_t timeout, struct task_msg_args **out_args)
{
    if(task_msg_bus_init_tag==RT_FALSE) return -RT_EINVAL;

    char name[RT_NAME_MAX];
    task_msg_wait_any_node_t node = rt_calloc(1, sizeof(struct task_msg_wait_any_node));
    if(node == RT_NULL)
    {
        LOG_E("there is no memory available!");
        return RT_ENOMEM;
    }
    rt_uint32_t count = rt_slist_len(&wait_any_slist);
    rt_snprintf(name, RT_NAME_MAX, "wta_%d", count);
    rt_sem_init(&(node->msg_sem), name, 0, RT_IPC_FLAG_FIFO);
    node->msg_name_list = (enum task_msg_name *)msg_name_list;
    node->msg_name_list_len = msg_name_list_len;
    rt_slist_init(&(node->slist));
    rt_mutex_take(&wta_lock, RT_WAITING_FOREVER);
    rt_slist_append(&wait_any_slist, &(node->slist));
    rt_mutex_release(&wta_lock);

    rt_err_t rst = rt_sem_take(&(node->msg_sem), timeout);
    if(rst==RT_EOK && out_args)
    {
        (*out_args)= node->args;
    }
    rt_mutex_take(&wta_lock, RT_WAITING_FOREVER);
    rt_slist_remove(&wait_any_slist, &(node->slist));
    rt_mutex_release(&wta_lock);
    rt_sem_detach(&(node->msg_sem));
    rt_free(node);

    return rst;
}

rt_err_t task_msg_wait_until(enum task_msg_name msg_name, rt_uint32_t timeout, struct task_msg_args **out_args)
{
    if(task_msg_bus_init_tag==RT_FALSE) return -RT_EINVAL;

    char name[RT_NAME_MAX];
    task_msg_wait_node_t node = rt_calloc(1, sizeof(struct task_msg_wait_node));
    if(node == RT_NULL)
    {
        LOG_E("there is no memory available!");
        return RT_ENOMEM;
    }
    rt_uint32_t count = rt_slist_len(&wait_slist);
    rt_snprintf(name, RT_NAME_MAX, "wt_%d", count);
    rt_sem_init(&(node->msg_sem), name, 0, RT_IPC_FLAG_FIFO);
    node->msg_name = msg_name;
    rt_slist_init(&(node->slist));
    rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
    rt_slist_append(&wait_slist, &(node->slist));
    rt_mutex_release(&wt_lock);

    rt_err_t rst = rt_sem_take(&(node->msg_sem), timeout);
    if(rst==RT_EOK && out_args)
    {
        (*out_args) = node->args;
    }
    rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
    rt_slist_remove(&wait_slist, &(node->slist));
    rt_mutex_release(&wt_lock);
    rt_sem_detach(&(node->msg_sem));
    rt_free(node);

    return rst;
}

rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args))
{
    if(task_msg_bus_init_tag==RT_FALSE) return -RT_EINVAL;

    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    rt_bool_t find_tag = RT_FALSE;
    task_msg_callback_node_t node;
    rt_slist_for_each_entry(node, &callback_slist_array[msg_name], slist)
    {
        if(node->callback == callback)
        {
            find_tag = RT_TRUE;
            break;
        }
    }
    if(find_tag)
    {
        LOG_W("this task msg callback with msg_name[%d] is exist!", msg_name);
    }
    else
    {
        task_msg_callback_node_t callback_node = rt_calloc(1, sizeof(struct task_msg_callback_node));
        if(callback_node == RT_NULL)
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

rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args))
{
    if(task_msg_bus_init_tag==RT_FALSE) return -RT_EINVAL;

    task_msg_callback_node_t node;
    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    rt_slist_for_each_entry(node, &callback_slist_array[msg_name], slist)
    {
        if(node->callback == callback)
        {
            rt_slist_remove(&callback_slist_array[msg_name], &(node->slist));
            rt_free(node);
            break;
        }
    }
    rt_mutex_release(&cb_lock);

    return RT_EOK;
}

rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *args_json)
{
    if(task_msg_bus_init_tag==RT_FALSE) return -RT_EINVAL;

    task_msg_args_node_t node = rt_calloc(1, sizeof(struct task_msg_args_node));
    if(node == RT_NULL)
    {
        LOG_E("task msg publish failed! args_node create failed!");
        return -RT_ENOMEM;
    }

    task_msg_args_t msg_args = rt_calloc(1, sizeof(struct task_msg_args));
    if(msg_args == RT_NULL)
    {
        rt_free(node);
        LOG_E("task msg publish failed! msg_args create failed!");
        return -RT_ENOMEM;
    }

    msg_args->msg_name = msg_name;
    msg_args->msg_args_json = RT_NULL;
    if(args_json) msg_args->msg_args_json = rt_strdup(args_json);
    node->args = msg_args;
    rt_slist_init(&(node->slist));
    rt_mutex_take(&msg_lock, RT_WAITING_FOREVER);
    rt_slist_append(&msg_slist, &(node->slist));
    rt_mutex_release(&msg_lock);

    rt_sem_release(&msg_sem);

    return RT_EOK;
}

static void task_msg_callback_init(void)
{
    rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
    for(int i=0; i<TASK_MSG_COUNT; i++)
    {
        callback_slist_array[i].next = RT_NULL;
    }
    rt_mutex_release(&cb_lock);
}

static void task_msg_bus_thread_entry(void *params)
{
    while(1)
    {
        if(rt_sem_take(&msg_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            task_msg_args_node_t msg_args_node;
            task_msg_callback_node_t msg_callback_node;
            task_msg_wait_node_t msg_wait_node;
            task_msg_wait_any_node_t msg_wait_any_node;
            while(rt_slist_len(&msg_slist) > 0)
            {
                //get msg
                msg_args_node = rt_slist_first_entry(&msg_slist, struct task_msg_args_node, slist);
                msg_ref_append(msg_args_node->args);
                //check wait msg until
                rt_mutex_take(&wt_lock, RT_WAITING_FOREVER);
                rt_slist_for_each_entry(msg_wait_node, &wait_slist, slist)
                {
                    if(msg_wait_node->msg_name==msg_args_node->args->msg_name)
                    {
                        msg_wait_node->args = msg_args_node->args;
                        msg_ref_append(msg_args_node->args);
                        rt_sem_release(&(msg_wait_node->msg_sem));
                    }
                }
                rt_mutex_release(&wt_lock);
                //check wait any msg in array until
                rt_mutex_take(&wta_lock, RT_WAITING_FOREVER);
                rt_slist_for_each_entry(msg_wait_any_node, &wait_any_slist, slist)
                {
                    for(int i=0; i<msg_wait_any_node->msg_name_list_len; i++)
                    {
                        if(msg_wait_any_node->msg_name_list[i]==msg_args_node->args->msg_name)
                        {
                            msg_wait_any_node->args = msg_args_node->args;
                            msg_ref_append(msg_args_node->args);
                            rt_sem_release(&(msg_wait_any_node->msg_sem));
                            break;
                        }
                    }
                }
                rt_mutex_release(&wta_lock);
                //msg callback
                rt_mutex_take(&cb_lock, RT_WAITING_FOREVER);
                rt_slist_for_each_entry(msg_callback_node, &callback_slist_array[msg_args_node->args->msg_name], slist)
                {
                    if(msg_callback_node->callback)
                    {
                        msg_callback_node->callback(msg_args_node->args);
                    }
                }
                rt_mutex_release(&cb_lock);
                //remove msg
                rt_mutex_take(&msg_lock, RT_WAITING_FOREVER);
                rt_slist_remove(&msg_slist, &(msg_args_node->slist));
                rt_mutex_release(&msg_lock);
                //free msg
                task_msg_delete(msg_args_node->args);
                rt_free(msg_args_node);
            }
        }
    }
}

rt_err_t task_msg_bus_init(rt_uint32_t stack_size, rt_uint8_t  priority, rt_uint32_t tick)
{
    if(task_msg_bus_init_tag==RT_FALSE)
    {
        rt_sem_init(&msg_sem, "msg_sem", 0, RT_IPC_FLAG_FIFO);
        rt_mutex_init(&msg_lock, "msg_lock", RT_IPC_FLAG_FIFO);
        rt_mutex_init(&msg_ref_lock, "ref_lock", RT_IPC_FLAG_FIFO);
        rt_mutex_init(&cb_lock, "cb_lock", RT_IPC_FLAG_FIFO);
        rt_mutex_init(&wt_lock, "wt_lock", RT_IPC_FLAG_FIFO);
        rt_mutex_init(&wta_lock, "wta_lock", RT_IPC_FLAG_FIFO);
        task_msg_callback_init();
        task_msg_bus_init_tag = RT_TRUE;
    }

    rt_thread_t t = rt_thread_create("msg_bus",
            task_msg_bus_thread_entry,
            RT_NULL,
            stack_size,
            priority,
            tick);
    if(t==RT_NULL)
    {
        LOG_E("task msg bus initialize failed! msg_bus_thread create failed!");
        return -RT_ENOMEM;
    }

    rt_err_t rst = rt_thread_startup(t);
    if(rst == RT_EOK)
    {
        LOG_I("task msg bus initialize success!");
    }
    else
    {
        LOG_E("task msg bus initialize failed! msg_bus thread startup failed(%d)", rst);
    }
    return rst;
}
