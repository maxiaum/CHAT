#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include <mutex>
#include "usermodel.hpp"
#include "offlinemsgmodel.hpp"
#include "addfriendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

#include "json.hpp"
using json = nlohmann::json;

using namespace std;
using namespace muduo;
using namespace muduo::net;

using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

class ChatService
{
public:
    //获取单例模式的接口函数
    static ChatService* instance();
    //处理登录
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理注册
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理用户异常断开
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器/客户端异常，业务重置
    void reset();
    //处理一对一聊天
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //添加好友
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //接收好友请求
    void acceptAddFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //创建群聊
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //加入群聊
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //群聊天
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //注销
    void logOut(const TcpConnectionPtr &conn, json &js, Timestamp time);

    void handleRedisSubsMessage(int id, std::string);

    MsgHandler getHandler(int msgid);
private:
    ChatService();

    unordered_map<int, MsgHandler> _msgHandlerMap;

    //存储在线用户的连接
    unordered_map<int, TcpConnectionPtr> _connectionMap;

    //定义互斥锁保证 _connectionMap的线程安全
    std::mutex _connMapMtx;

    //数据操作类对象
    UserModel _userModel;

    OfflineMsgModel _offMsgModel;

    FriendModel _addFriendModel;

    GroupModel _groupModel;

    Redis _redis;
};

#endif