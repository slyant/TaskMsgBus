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

#include "task_msg_name_def.h"

struct task_msg_args
{
    enum task_msg_name msg_name;
    char *msg_args_json;
};
typedef struct task_msg_args *task_msg_args_t;

struct task_msg_args_node
{
    task_msg_args_t args;
    rt_slist_t slist;
};
typedef struct task_msg_args_node *task_msg_args_node_t;

struct task_msg_callback_node
{
    void(*callback)(const task_msg_args_t msg_args);
    rt_slist_t slist;
};
typedef struct task_msg_callback_node *task_msg_callback_node_t;

struct task_msg_wait_node
{
    enum task_msg_name msg_name;
    struct rt_semaphore msg_sem;
    char *msg_args_json;
    rt_slist_t slist;
};
typedef struct task_msg_wait_node *task_msg_wait_node_t;

struct task_msg_wait_any_node
{
    enum task_msg_name *msg_name_list;
    rt_uint8_t msg_name_list_len;
    struct rt_semaphore msg_sem;
    enum task_msg_name msg_name;
    char *msg_args_json;
    rt_slist_t slist;
};
typedef struct task_msg_wait_any_node *task_msg_wait_any_node_t;

rt_err_t task_msg_bus_init(rt_uint32_t stack_size, rt_uint8_t  priority, rt_uint32_t tick);
rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args));
rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *args_json);
rt_err_t task_msg_wait_until(enum task_msg_name msg_name, rt_uint32_t timeout, task_msg_args_t out_args);
rt_err_t task_msg_wait_any(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len, rt_uint32_t timeout, task_msg_args_t out_args);

#endif /* TASK_MSG_BUS_H_ */