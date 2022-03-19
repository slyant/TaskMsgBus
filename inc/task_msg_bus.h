/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-22     sly_ant      the first version
 */
#ifndef TASK_MSG_BUS_H_
#define TASK_MSG_BUS_H_

#include <rtthread.h>
#include <rtdevice.h>

#include "task_msg_bus_def.h"

struct task_msg_args
{
    enum task_msg_name msg_name;
    void *msg_obj;
    rt_uint32_t msg_size;
};
typedef struct task_msg_args *task_msg_args_t;

struct task_msg_ref_node
{
    task_msg_args_t args;
    int ref_count;
    rt_slist_t slist;
};
typedef struct task_msg_ref_node *task_msg_ref_node_t;

struct task_msg_args_node
{
    task_msg_args_t args;
    rt_slist_t slist;
};
typedef struct task_msg_args_node *task_msg_args_node_t;

struct task_msg_callback_node
{
    void (*callback)(const task_msg_args_t msg_args);
    rt_slist_t slist;
};
typedef struct task_msg_callback_node *task_msg_callback_node_t;

struct task_msg_subscriber_node
{
    int subscriber_id;
    enum task_msg_name msg_name;
    rt_sem_t sem;
    rt_slist_t slist;
};
typedef struct task_msg_subscriber_node *task_msg_subscriber_node_t;

struct task_msg_wait_node
{
    task_msg_subscriber_node_t subscriber;
    task_msg_args_t args;
    rt_slist_t slist;
};
typedef struct task_msg_wait_node *task_msg_wait_node_t;

struct task_msg_dup_release_hook
{
    enum task_msg_name msg_name;
    void *(*dup)(void *args);
    void (*release)(void *args);
};

struct task_msg_timer_node
{
    task_msg_args_t args;
    struct rt_timer timer;
    rt_uint32_t repeat;
    rt_uint32_t do_count;
    rt_bool_t stop;
    rt_tick_t interval;
    rt_slist_t slist;
};
typedef struct task_msg_timer_node *task_msg_timer_node_t;

int task_msg_bus_init(void);
rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *msg_text);
rt_err_t task_msg_publish_obj(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size);

rt_err_t task_msg_scheduled_append(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size);
rt_err_t task_msg_scheduled_start(enum task_msg_name msg_name, int delay_ms, rt_uint32_t repeat, int interval_ms);
rt_err_t task_msg_scheduled_restart(enum task_msg_name msg_name);
rt_err_t task_msg_scheduled_stop(enum task_msg_name msg_name);
void task_msg_scheduled_delete(enum task_msg_name msg_name);

int task_msg_subscriber_create(enum task_msg_name msg_name);
int task_msg_subscriber_create2(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len);
void task_msg_subscriber_delete(int subscriber_id);
rt_err_t task_msg_wait_until(int subscriber_id, rt_int32_t timeout_ms, struct task_msg_args **out_args);
void task_msg_release(task_msg_args_t args);

#endif /* TASK_MSG_BUS_H_ */
