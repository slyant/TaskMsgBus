/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-22     sly_ant      the first version
 */
#ifndef TASK_MSG_NAME_DEF_H_
#define TASK_MSG_NAME_DEF_H_
#include <rtthread.h>

#ifdef TASK_MSG_NAME_USER_DEF
    #include "task_msg_name_user_def.h"
#else
    enum task_msg_name{
        TASK_MSG_OS_REDAY = 0,
        TASK_MSG_NET_REDAY,
        TASK_MSG_COUNT
    };
#endif

#endif /* TASK_MSG_NAME_DEF_H_ */
