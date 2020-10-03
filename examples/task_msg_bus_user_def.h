#ifndef TASK_MSG_BUS_USER_DEF_H_
#define TASK_MSG_BUS_USER_DEF_H_

struct msg_2_def
{
    int id;
    char name[8];
};

enum task_msg_name{
    TASK_MSG_OS_REDAY = 0,  //RT_NULL
    TASK_MSG_NET_REDAY,     //json: net_reday:int,ip:string,id:int
    TASK_MSG_1,
    TASK_MSG_2,             //struct msg_2_def
    TASK_MSG_3,             //struct msg_3_def
    TASK_MSG_4,
    TASK_MSG_5,
    TASK_MSG_COUNT
};

#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
    struct msg_3_def
    {
        int id;
        char name[8];
        rt_uint8_t *buffer;
        rt_size_t buffer_size;
    };
    extern void *msg_3_dup_hook(void *args);
    extern void msg_3_release_hook(void *args);
    #define task_msg_dup_release_hooks {\
            {TASK_MSG_OS_REDAY,     RT_NULL, RT_NULL},          \
            {TASK_MSG_NET_REDAY,    RT_NULL, RT_NULL},          \
            {TASK_MSG_1,            RT_NULL, RT_NULL},          \
            {TASK_MSG_2,            RT_NULL, RT_NULL},          \
            {TASK_MSG_3,            msg_3_dup_hook, msg_3_release_hook},   \
            {TASK_MSG_4,            RT_NULL, RT_NULL},          \
            {TASK_MSG_5,            RT_NULL, RT_NULL},          \
        }
#endif

#endif
