/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-22     sly_ant      the first version
 */
#ifndef TASK_MSG_BUS_DEF_H_
#define TASK_MSG_BUS_DEF_H_
#include <rtthread.h>

#ifdef TASK_MSG_USER_DEF
#include "task_msg_bus_user_def.h"
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
#ifndef task_msg_dup_release_hooks
#error "Please define 'task_msg_dup_release_hooks' in the header file:'task_msg_bus_user_def.h"
#endif
#endif
#else
enum task_msg_name
{
    TASK_MSG_OS_REDAY = 0,
    TASK_MSG_NET_REDAY,
    TASK_MSG_COUNT
};
#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
#define task_msg_dup_release_hooks {                    \
                {TASK_MSG_OS_REDAY, RT_NULL, RT_NULL},   \
                {TASK_MSG_NET_REDAY, RT_NULL, RT_NULL},  \
            }
#endif
#endif

#endif /* TASK_MSG_BUS_DEF_H_ */
