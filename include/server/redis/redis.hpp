#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>

class Redis
{
public:
    Redis();
    ~Redis();
    // 连接redis服务器
    bool connect();

    bool publish(int channel, std::string message);

    bool subscribe(int channel);

    bool unsubscribe(int channel);
    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();
    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(std::function<void(int, std::string)> fun);

private:
    redisContext* _publish_context; // hiredis同步上下文对象，负责publish消息

    redisContext* _subscribe_context; // hiredis同步上下文对象，负责subscribe消息

    std::function<void(int, std::string)> _notify_message_handler; // 回调函数，收到订阅消息，给service层上报
};

#endif