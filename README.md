# TaskMsgBus

## 1、介绍

这个软件包实现了基于RT-Thread的消息总线，可以轻松的实现线程间的同步和消息收发，支持文本、数字、结构体等任意复杂的消息类型的发送和接收。当有多个线程订阅消费消息时，不会增加内存的使用，如果消息对象使用了动态内存地址引用，通过设置消息释放的钩子函数，可实现内存的自动回收。

### 1.1 目录结构

| 名称          | 说明                   |
| ------------- | ---------------------- |
| examples      | 示例                   |
| inc           | 头文件目录             |
| src           | 源代码目录             |
### 1.2 许可证

TaskMsgBus package 遵循 Apache license v2.0 许可，详见 `LICENSE` 文件。

### 1.3 依赖

无。

## 2、如何打开 TaskMsgBus package

使用 TaskMsgBus package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    system packages --->
        [*]TaskMsgBus: For sending and receiving json/text/object messages between threads based on RT-Thread
        TaskMsgBus --->
            task message thread stack size [384]
            task message thread priority [5]
            [*]task msg name define in user file 'task_msg_bus_user_def.h'
            [*]task msg object using dynamic memory
            [*]Enable TaskMsgBus Sample

```
或者直接下载源码，添加到项目中编译即可
## 3、使用 TaskMsgBus package

按上述方法打开TaskMsgBus package，启用示例，编译工程，即可在控制台看到示例运行结果:
![avatar](/image/PuTTY.png)

### 3.1 API 列表

| API        | 功能                     |
| -------------- | ------------------------ |
| rt_err_t task_msg_bus_init(rt_uint32_t stack_size, rt_uint8_t  priority, rt_uint32_t tick); | 初始化消息总线 |
| rt_err_t task_msg_subscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args)); | 订阅消息 |
| rt_err_t task_msg_unsubscribe(enum task_msg_name msg_name, void(*callback)(task_msg_args_t msg_args)); | 取消订阅消息 |
| rt_err_t task_msg_publish(enum task_msg_name msg_name, const char *msg_text);  | 发布text/json消息 |
| rt_err_t task_msg_publish_obj(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size); | 发布任意数据类型消息 |
| rt_err_t task_msg_scheduled_append(enum task_msg_name msg_name, void *msg_obj, rt_size_t msg_size); | 添加一个计划消息，但不发送 |
| rt_err_t task_msg_scheduled_start(enum task_msg_name msg_name, int delay_ms, rt_uint32_t repeat, int interval_ms); | 启动一个计划消息（如果之前没有添加过，将自动添加一个无消息体的计划消息）：当repeat=0时，先延时delay_ms毫秒发送1次消息后，再按interval_ms毫秒间隔周期性循环发送消息；当repeat=1时，interval_ms参数无效，将延时delay_ms毫秒发送1次消息；当repeat>1时，先延时delay_ms毫秒发送1次消息后，再按interval_ms毫秒间隔周期性循环发送(repeat-1)次消息|
| rt_err_t task_msg_scheduled_restart(enum task_msg_name msg_name); | 重新启动一个计划消息（将重置定时器） |
| rt_err_t task_msg_scheduled_stop(enum task_msg_name msg_name); | 停止一个计划消息 |
| void task_msg_scheduled_delete(enum task_msg_name msg_name); | 删除一个计划消息 |
| int task_msg_subscriber_create(enum task_msg_name msg_name); | 创建一个消息订阅者，返回订阅者ID |
| int task_msg_subscriber_create2(const enum task_msg_name *msg_name_list, rt_uint8_t msg_name_list_len); | 创建一个可以订阅多个主题的消息订阅者，返回订阅者ID |
| rt_err_t task_msg_wait_until(int subscriber_id, rt_int32_t timeout_ms, struct task_msg_args **out_args); | 阻塞等待指定订阅者订阅的消息 |
| void task_msg_release(task_msg_args_t args); | 释放已经消费的消息 |
| void task_msg_subscriber_delete(int subscriber_id); | 删除一个消息订阅者 |

### 3.2 使用方法
* 在包管理器中取消Enable TaskMsgBus Sample选项
* 参照示例文件夹中的“task_msg_bus_user_def.h”,创建头文件“task_msg_bus_user_def.h”

定义消息名称的枚举类型enum task_msg_name,例如：

```
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
```

如果要使用结构体类型的消息，需要定义消息结构体，例如：

```
struct msg_2_def
{
    int id;
    char name[8];
};

struct msg_3_def
{
    int id;
    char name[8];
    rt_uint8_t *buffer;
    rt_size_t buffer_size;
};
```


如果要在结构体的指针类型的字段中动态分配内存，需要在前面的包管理器中启用[task msg object using dynamic memory]，同时，需要定义复制和释放该消息的钩子函数，例如：

```
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
```

在用户的 *.c 文件中实现此钩子函数,例如:
```
void *msg_3_dup_hook(void *args)
{
    struct msg_3_def *msg_3 = (struct msg_3_def *) args;
    struct msg_3_def *r_msg_3 = rt_calloc(1, sizeof(struct msg_3_def));
    if (r_msg_3 == RT_NULL)
    {
        return RT_NULL;
    }

    rt_memcpy((rt_uint8_t *) r_msg_3, (rt_uint8_t *) msg_3, sizeof(struct msg_3_def));
    if (msg_3->buffer && msg_3->buffer_size > 0)
    {
        r_msg_3->buffer = rt_calloc(1, msg_3->buffer_size);
        if (r_msg_3->buffer == RT_NULL)
        {
            rt_free(r_msg_3);
            return RT_NULL;
        }
        rt_memcpy(r_msg_3->buffer, msg_3->buffer, msg_3->buffer_size);
        r_msg_3->buffer_size = msg_3->buffer_size;
    }

    return r_msg_3;
}
void msg_3_release_hook(void *args)
{
    struct msg_3_def *msg_3 = (struct msg_3_def *) args;
    if (msg_3->buffer)
        rt_free(msg_3->buffer);
}
```


* 初始化

```
task_msg_bus_init(); //初始化消息总线，已经导入到组件自动初始化函数INIT_COMPONENT_EXPORT(task_msg_bus_init)
```

调用此函数将动态创建1个消息总线的消息分发线程

* 订阅消息（在回调函数中处理不耗资源的任务时，可以使用此方式）

```
static void net_reday_callback(task_msg_args_t args)
{
    LOG_D("[net_reday_callback]:TASK_MSG_NET_REDAY => args->msg_name:%d, args->msg_obj:%s", args->msg_name, args->msg_obj);
}

task_msg_subscribe(TASK_MSG_NET_REDAY, net_reday_callback);
```

* 取消订阅消息

```
task_msg_unsubscribe(TASK_MSG_NET_REDAY, net_reday_callback);
```

* 发布消息

不带消息内容：

```
task_msg_publish(TASK_MSG_OS_REDAY, RT_NULL);
```

text/json消息：

```
char msg_text[50];
rt_snprintf(msg_text, 50, "{\"net_reday\":%d,\"ip\":\"%s\",\"id\":%ld}", 1, "10.0.0.20", i);
            task_msg_publish(TASK_MSG_NET_REDAY, msg_text);
```

结构体类型的消息(内部字段无动态内存分配)：

```
struct msg_2_def msg_2;
msg_2.id = i;
rt_snprintf(msg_2.name, 8, "%s\0", "hello");
task_msg_publish_obj(TASK_MSG_2, &msg_2, sizeof(struct msg_2_def));
```

结构体类型的消息(内部字段有动态内存分配)：

```
const char buffer_test[32] = {
    0x0F, 0x51, 0xEE, 0x89, 0x9D, 0x40, 0x80, 0x22, 0x63, 0x44, 0x43, 0x39, 0x55, 0x2D, 0x12, 0xA1,
    0x1C, 0x91, 0xE5, 0x2C, 0xC4, 0x6A, 0x62, 0x5B, 0xB6, 0x41, 0xF0, 0xF7, 0x75, 0x48, 0x05, 0xE9
};

struct msg_3_def msg_3;
msg_3.id = i;
rt_snprintf(msg_3.name, 8, "%s\0", "slyant");
msg_3.buffer = rt_calloc(1, 32);
rt_memcpy(msg_3.buffer, buffer_test, 32);
msg_3.buffer_size = 32;
task_msg_publish_obj(TASK_MSG_3, &msg_3, sizeof(struct msg_3_def));
rt_free(msg_3.buffer);
```

* 以线程阻塞的方式接收消息

接收某个指定的消息：
```
static void msg_wait_thread_entry(void *params)
{
    rt_err_t rst;
    task_msg_args_t args;
    //创建消息订阅者
    int subscriber_id = task_msg_subscriber_create(TASK_MSG_NET_REDAY);
    if(subscriber_id < 0) return;

    while(1)
    {
        rst = task_msg_wait_until(subscriber_id, 50, &args);
        if(rst==RT_EOK)
        {
            LOG_D("[task_msg_wait_until]:TASK_MSG_NET_REDAY => args.msg_name:%d, args.msg_obj:%s", args->msg_name, args->msg_obj);
            rt_thread_mdelay(200);//模拟耗时操作，在此期间发布的消息不会丢失
            //释放消息
            task_msg_release(args);
        }
        else
        {
            //可以做其它操作，在此期间发布的消息不会丢失
        }
    }
}

rt_thread_t t_wait = rt_thread_create("msg_wt", msg_wait_thread_entry, RT_NULL, 512, 17, 10);
rt_thread_startup(t_wait);
```

同时接收多个指定的消息：

```
static void msg_wait_any_thread_entry(void *params)
{
    rt_err_t rst;
    task_msg_args_t args = RT_NULL;
    const enum task_msg_name name_list[4] = {TASK_MSG_OS_REDAY, TASK_MSG_NET_REDAY, TASK_MSG_2, TASK_MSG_3};
    //创建 多消息订阅者
    int subscriber_id = task_msg_subscriber_create2(name_list, sizeof(name_list)/sizeof(enum task_msg_name));
    if(subscriber_id < 0) return;

    while(1)
    {
        rst = task_msg_wait_until(subscriber_id, 50, &args);
        if(rst==RT_EOK)
        {
            if(args->msg_name==TASK_MSG_OS_REDAY)
            {
                LOG_D("[task_msg_wait_any]:TASK_MSG_OS_REDAY => msg_obj is null:%s", args->msg_obj==RT_NULL ? "true" : "false");
            }
            else if(args->msg_name==TASK_MSG_NET_REDAY)
            {
                LOG_D("[task_msg_wait_any]:TASK_MSG_NET_REDAY => args.msg_name:%d, args.msg_obj:%s", args->msg_name, args->msg_obj);
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
            rt_thread_mdelay(200);//模拟耗时操作，在此期间发布的消息不会丢失
            //释放消息
            task_msg_release(args);
        }
        else
        {
            //可以做其它操作，在此期间发布的消息不会丢失
        }
    }
}

rt_thread_t t_wait_any = rt_thread_create("msg_wa", msg_wait_any_thread_entry, RT_NULL, 1024, 16, 10);
rt_thread_startup(t_wait_any);
```


## 4、注意事项

* 不要在订阅消息的回调函数中执行消耗资源的操作，否则，请在单独的线程中，使用task_msg_wait_until来处理需要关注的消息。

* 如果使用了结构体数据类型的消息，同时在结构体中定义了指针，且动态分配了内存，一定要设置释放内存的钩子函数，否则会造成内存泄露。

* 在使用task_msg_wait_until函数接收消息时，仅当函数返回了RT_EOK时，记得使用task_msg_release函数释放该消息（在其它任何情况下都不要使用task_msg_release函数）。当所有关注该消息的订阅者全部释放了该消息时，该消息才真正从物理内存中释放。

## 5、联系方式 & 感谢

* 维护：[slyant](https://gitee.com/slyant)
* 主页：https://gitee.com/slyant/TaskMsgBus
