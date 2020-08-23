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

struct task_msg_dump_release_hook
{
    enum task_msg_name msg_name;
    void *(*dump)(void *args);
    void (*release)(void *args);
};

struct task_msg_loop
{
    enum task_msg_name msg_name;
    void *msg_obj;
    rt_uint32_t msg_size;
    rt_timer_t timer;
};
typedef struct task_msg_loop *task_msg_loop_t;

rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void (*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *msg_text);
rt_err_t task_msg_publish_obj(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size);
rt_err_t task_msg_delay_publish(rt_uint32_t delay_ms, enum task_msg_name msg_name, const char *msg_text);
rt_err_t task_msg_delay_publish_obj(rt_uint32_t delay_ms, enum task_msg_name msg_name, void *msg_obj,
        rt_size_t msg_size);
task_msg_loop_t task_msg_loop_create(void);
rt_err_t task_msg_loop_delete(task_msg_loop_t msg_loop);
rt_err_t task_msg_loop_start(task_msg_loop_t msg_loop, rt_uint32_t delay_ms, enum task_msg_name msg_name, void *msg_obj,
        rt_size_t msg_size);
rt_err_t task_msg_loop_stop(task_msg_loop_t msg_loop);
int task_msg_subscriber_create(enum task_msg_name msg_name);
int task_msg_subscriber_create2(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len);
void task_msg_subscriber_delete(int subscriber_id);
rt_err_t task_msg_wait_until(int subscriber_id, rt_int32_t timeout_ms, struct task_msg_args **out_args);
void task_msg_release(task_msg_args_t args);

#endif /* TASK_MSG_BUS_H_ */
