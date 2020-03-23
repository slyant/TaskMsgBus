#include <board.h>
#include "task_msg_bus.h"
#ifdef TASK_MSG_USING_JSON
#include "cJSON_util.h"
#endif

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
        const enum task_msg_name name_list[4] = {TASK_MSG_OS_REDAY, TASK_MSG_NET_REDAY, TASK_MSG_3, TASK_MSG_5};
        task_msg_wait_any(name_list, sizeof(name_list)/sizeof(enum task_msg_name), RT_WAITING_FOREVER, &args);
        LOG_D("task_msg_wait_any => args.msg_name:%d, args.msg_args_json:%s", args.msg_name, args.msg_args_json);
    #ifdef TASK_MSG_USING_JSON
        cJSON *root = cJSON_Parse(args.msg_args_json);
        if(root)
        {
            if(args.msg_name==TASK_MSG_OS_REDAY)
            {
                int os_reday, id;
                if(cJSON_item_get_number(root, "os_reday", &os_reday)==0)
                    LOG_D("TASK_MSG_OS_REDAY=>os_reday:%d", os_reday);
                if(cJSON_item_get_number(root, "id", &id)==0)
                    LOG_D("TASK_MSG_OS_REDAY=>id:%d", id);
            }
            else if(args.msg_name==TASK_MSG_NET_REDAY)
            {
                int os_reday, id;
                if(cJSON_item_get_number(root, "net_reday", &os_reday)==0)
                    LOG_D("TASK_MSG_NET_REDAY=>net_reday:%d", os_reday);
                const char *ip = cJSON_item_get_string(root, "ip");
                if(ip)
                    LOG_D("TASK_MSG_NET_REDAY=>ip:%s", ip);
                if(cJSON_item_get_number(root, "id", &id)==0)
                    LOG_D("TASK_MSG_NET_REDAY=>id:%d", id);
            }
            else if(args.msg_name==TASK_MSG_3)
            {
                int id;
                const char *msg_3 = cJSON_item_get_string(root, "msg_3");
                if(msg_3)
                    LOG_D("TASK_MSG_3=>msg_3:%s", msg_3);
                const char *name = cJSON_item_get_string(root, "name");
                if(name)
                    LOG_D("TASK_MSG_3=>name:%s", name);
                if(cJSON_item_get_number(root, "id", &id)==0)
                    LOG_D("TASK_MSG_3=>id:%d", id);
            }
            cJSON_free(root);
        }
    #endif
        if(args.msg_args_json)
            rt_free(args.msg_args_json);
    }    
}

static void msg_publish_thread_entry(void *params)
{
    static int i = 0;
    char arg_json[50];
    while (1)
    {
        if(i % 3 == 0)
        {
            rt_snprintf(arg_json, 50, "{\"os_reday\":%d,\"id\":%ld}", 1, i);
            task_msg_publish(TASK_MSG_OS_REDAY, arg_json);
        }
        else if(i % 3 == 1)
        {
            rt_snprintf(arg_json, 50, "{\"net_reday\":%d,\"ip\":\"%s\",\"id\":%ld}", 1, "10.0.0.20", i);
            task_msg_publish(TASK_MSG_NET_REDAY, arg_json);
        }
        else
        {
            rt_snprintf(arg_json, 50, "{\"msg_3\":\"%s\",\"name\":\"%s\",\"id\":%ld}", "msg3", "slyant", i);
            task_msg_publish(TASK_MSG_3, arg_json);
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
    task_msg_subscribe(TASK_MSG_NET_REDAY, net_reday_callback);
    task_msg_subscribe(TASK_MSG_OS_REDAY, os_reday_callback);
    //创建一个等待消息的线程
    rt_thread_t t_wait = rt_thread_create("msg_wait", msg_wait_thread_entry, RT_NULL, 1024*2, 20, 10);
    rt_thread_startup(t_wait);
    //创建一个发布消息的线程
    rt_thread_t t_publish = rt_thread_create("msg_pub", msg_publish_thread_entry, RT_NULL, 1024*2, 15, 10);
    rt_thread_startup(t_publish);

    return RT_EOK;
}
INIT_APP_EXPORT(task_msg_bus_sample);
