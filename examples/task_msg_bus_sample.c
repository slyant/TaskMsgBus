#include <board.h>
#include "task_msg_bus.h"
#ifdef TASK_MSG_USING_JSON
#include "cJSON_util.h"
#endif

#define LOG_TAG              "task.msg.bus.sample"
#define LOG_LVL              LOG_LVL_DBG
#include <ulog.h>

#ifdef TASK_MSG_USING_DYNAMIC_MEMORY
    void msg_3_release_hook(void *args)
    {
        struct msg_3_def *msg_3 = (struct msg_3_def *)args;
        if(msg_3->buffer)
            rt_free(msg_3->buffer);
    }
#endif

static void net_reday_callback(task_msg_args_t args)
{
    LOG_D("[net_reday_callback]:TASK_MSG_NET_REDAY => args->msg_name:%d, args->msg_obj:%s", args->msg_name, args->msg_obj);
}

static void os_reday_callback(task_msg_args_t args)
{
    LOG_D("[os_reday_callback]:TASK_MSG_OS_REDAY => msg_obj is null:%s", args->msg_obj==RT_NULL ? "true" : "false");
}

static void msg_wait_thread_entry(void *params)
{
    rt_err_t rst;
    task_msg_args_t args;
    while(1)
    {
        //测试 task_msg_wait_until
        rst = task_msg_wait_until(TASK_MSG_NET_REDAY, RT_WAITING_FOREVER, &args);
        if(rst==RT_EOK)
        {
            LOG_D("[task_msg_wait_until]:TASK_MSG_NET_REDAY => args.msg_name:%d, args.msg_obj:%s", args->msg_name, args->msg_obj);
            //释放消息
            task_msg_release(args);
        }
    }
}
static void msg_wait_any_thread_entry(void *params)
{
    rt_err_t rst;
    task_msg_args_t args = RT_NULL;
    while(1)
    {
        //测试 task_msg_wait_any
        const enum task_msg_name name_list[4] = {TASK_MSG_OS_REDAY, TASK_MSG_NET_REDAY, TASK_MSG_2, TASK_MSG_3};
        rst = task_msg_wait_any(name_list, sizeof(name_list)/sizeof(enum task_msg_name), RT_WAITING_FOREVER, &args);
        if(rst==RT_EOK)
        {
            if(args->msg_name==TASK_MSG_OS_REDAY)
            {
                LOG_D("[task_msg_wait_any]:TASK_MSG_OS_REDAY => msg_obj is null:%s", args->msg_obj==RT_NULL ? "true" : "false");
            }
            else if(args->msg_name==TASK_MSG_NET_REDAY)
            {
            #ifdef TASK_MSG_USING_JSON
                cJSON *root = cJSON_Parse(args->msg_obj);
                if(root)
                {
                    int net_reday, id;
                    cJSON_item_get_number(root, "net_reday", &net_reday);
                    cJSON_item_get_number(root, "id", &id);
                    const char *ip = cJSON_item_get_string(root, "ip");
                    LOG_D("[task_msg_wait_any]:TASK_MSG_NET_REDAY => net_reday:%s, ip:%s, id:%d", (net_reday==0 ? "false" : "true"), ip, id);
                    cJSON_Delete(root);
                }
            #else
                LOG_D("[task_msg_wait_any]:TASK_MSG_NET_REDAY => args.msg_name:%d, args.msg_obj:%s", args->msg_name, args->msg_obj);
            #endif
            }
            else if(args->msg_name==TASK_MSG_2)
            {
                struct msg_2_def *msg_2 = (struct msg_2_def *)args->msg_obj;
                LOG_D("[task_msg_wait_any]:TASK_MSG_2 => msg_2.id:%d, msg_2.name:%s", msg_2->id, msg_2->name);
            }
        #ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            else if(args->msg_name==TASK_MSG_3)
            {
                struct msg_3_def *msg_3 = (struct msg_3_def *)args->msg_obj;
                LOG_D("[task_msg_wait_any]:TASK_MSG_3 => msg_3.id:%d, msg_3.name:%s", msg_3->id, msg_3->name);
                LOG_HEX("[task_msg_wait_any]:TASK_MSG_3 => msg_3.buffer", 16, msg_3->buffer, msg_3->buffer_size);
            }
        #endif
            //释放消息
            task_msg_release(args);
        }
    }    
}

static void msg_publish_thread_entry(void *params)
{
    static int i = 0;
    char msg_text[50];
    while (1)
    {
        if(i % 4 == 0)
        {
            //不带消息内容
            task_msg_publish(TASK_MSG_OS_REDAY, RT_NULL);
        }
        else if(i % 4 == 1)
        {
            //json/text消息
            rt_snprintf(msg_text, 50, "{\"net_reday\":%d,\"ip\":\"%s\",\"id\":%ld}", 1, "10.0.0.20", i);
            task_msg_publish(TASK_MSG_NET_REDAY, msg_text);
        }
        else if(i % 4 == 2)
        {
            //结构体类型的消息(内部字段无动态内存分配)
            struct msg_2_def msg_2;
            msg_2.id = i;
            rt_snprintf(msg_2.name, 8, "%s\0", "hello");
            task_msg_publish_obj(TASK_MSG_2, &msg_2, sizeof(struct msg_2_def));
        }
        else
        {
        #ifdef TASK_MSG_USING_DYNAMIC_MEMORY
            const char buffer_test[32] = {
                0x0F, 0x51, 0xEE, 0x89, 0x9D, 0x40, 0x80, 0x22, 0x63, 0x44, 0x43, 0x39, 0x55, 0x2D, 0x12, 0xA1,
                0x1C, 0x91, 0xE5, 0x2C, 0xC4, 0x6A, 0x62, 0x5B, 0xB6, 0x41, 0xF0, 0xF7, 0x75, 0x48, 0x05, 0xE9
            };
            //结构体类型的消息(内部字段有动态内存分配)
            struct msg_3_def msg_3;
            msg_3.id = i;
            rt_snprintf(msg_3.name, 8, "%s\0", "slyant");
            msg_3.buffer = rt_calloc(1, 32);
            rt_memcpy(msg_3.buffer, buffer_test, 32);
            msg_3.buffer_size = 32;
            task_msg_publish_obj(TASK_MSG_3, &msg_3, sizeof(struct msg_3_def));
        #endif
        }

        rt_thread_mdelay(2000);
        i++;
    }
}

static int task_msg_bus_sample(void)
{
    //初始化消息总线(线程栈大小, 优先级, 时间片)
    task_msg_bus_init(512, 11, 10);
    //订阅消息
    task_msg_subscribe(TASK_MSG_NET_REDAY, net_reday_callback);
    task_msg_subscribe(TASK_MSG_OS_REDAY, os_reday_callback);
    //创建一个等待消息的线程
    rt_thread_t t_wait = rt_thread_create("msg_wt", msg_wait_thread_entry, RT_NULL, 512, 17, 10);
    rt_thread_startup(t_wait);
    //创建一个同时等待多个消息的线程
    rt_thread_t t_wait_any = rt_thread_create("msg_wa", msg_wait_any_thread_entry, RT_NULL, 1024, 16, 10);
    rt_thread_startup(t_wait_any);
    //创建一个发布消息的线程
    rt_thread_t t_publish = rt_thread_create("msg_pub", msg_publish_thread_entry, RT_NULL, 512, 15, 10);
    rt_thread_startup(t_publish);

    return RT_EOK;
}
INIT_APP_EXPORT(task_msg_bus_sample);
