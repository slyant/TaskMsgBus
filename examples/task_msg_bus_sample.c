#include <board.h>
#include "task_msg_bus.h"

#define DBG_TAG "task.msg.bus.sample"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static void msg_wait_thread_entry(void *params)
{
    struct task_msg_args args;
    while(1)
    {
        /*
        //测试 task_msg_wait_until
        task_msg_wait_until(TASK_MSG_NET_REDAY, RT_WAITING_FOREVER, &args);
        LOG_D("task_msg_wait_until => args.msg_name:%d, args.msg_args_json:%s", args.msg_name, args.msg_args_json);
        rt_free(args.msg_args_json);    
        */

        //测试 task_msg_wait_any
        const enum task_msg_name name_list[4] = {TASK_MSG_OS_REDAY, TASK_MSG_NET_REDAY, TASK_MSG_NET_REDAY3, TASK_MSG_NET_REDAY5};
        task_msg_wait_any(name_list, sizeof(name_list)/sizeof(enum task_msg_name), RT_WAITING_FOREVER, &args);
        LOG_D("task_msg_wait_any => args.msg_name:%d, args.msg_args_json:%s", args.msg_name, args.msg_args_json);
        if(args.msg_args_json)
            rt_free(args.msg_args_json);
    }    
}

static void msg_publish_thread_entry(void *params)
{
    static int i = 0;
    while (1)
    {
        if(i % 3 == 0)
        {
            task_msg_publish(TASK_MSG_OS_REDAY, "{\"os_reday\":true}");
        }
        else if(i % 3 == 1)
        {
            task_msg_publish(TASK_MSG_NET_REDAY3, "{\"net_reday\":true,\"ip\":\"10.0.0.20\"}");
        }
        else
        {
            task_msg_publish(TASK_MSG_NET_REDAY5, "{\"net_reday5\":true,\"ip\":\"192.168.0.50\"}");
        }   
        
        rt_thread_mdelay(1000);
        i++;
    }
}

static void net_reday_callback(task_msg_args_t args)
{
    LOG_D("net_reday_callback => args->msg_name:%d, args->msg_args_json:%s", args->msg_name, args->msg_args_json);
}

static void os_reday_callback(task_msg_args_t args)
{
    LOG_D("os_reday_callback => args->msg_name:%d, args->msg_args_json:%s", args->msg_name, args->msg_args_json);
}

static int task_msg_bus_sample(void)
{
    //初始化消息总线(线程栈大小, 优先级, 时间片)
    task_msg_bus_init(512, 25, 10);
    //订阅消息
    task_msg_subscribe(TASK_MSG_NET_REDAY5, net_reday_callback);
    task_msg_subscribe(TASK_MSG_OS_REDAY, os_reday_callback);
    //创建一个等待消息的线程
    rt_thread_t t_wait = rt_thread_create("msg_wait", msg_wait_thread_entry, RT_NULL, 512, 20, 10);
    rt_thread_startup(t_wait);
    //创建一个发布消息的线程
    rt_thread_t t_publish = rt_thread_create("msg_pub", msg_publish_thread_entry, RT_NULL, 512, 15, 10);
    rt_thread_startup(t_publish);

    return RT_EOK;
}
INIT_APP_EXPORT(task_msg_bus_sample);
