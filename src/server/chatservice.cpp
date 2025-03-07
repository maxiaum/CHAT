#include "chatservice.hpp"
#include "offlinemsgmodel.hpp"
#include "public.hpp"
#include "user.hpp"
#include "group.hpp"
#include "redis.hpp"
#include <muduo/base/Logging.h>
#include <iostream>

// #include <string>
// using namespace std;
// using namespace placeholders;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({ACCEPT_MSG, std::bind(&ChatService::acceptAddFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGOUT_MSG, std::bind(&ChatService::logOut, this, _1, _2, _3)});

    if (_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubsMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}
// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do login service!!!";
    int id = js["id"].get<int>();
    std::string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPassword() == pwd)
    {
        if (user.getState() == "online")
        {
            // 用户已在线，无法重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["id"] = user.getId();
            response["name"] = user.getName();
            response["errno"] = 2;
            response["errmsg"] = "is already logged in and cannot be logged in repeatedly!";
            conn->send(response.dump());
        }
        else
        {
            // 登陆成功
            {
                // 加锁
                lock_guard<mutex> lock(_connMapMtx);
                _connectionMap.insert({id, conn});
            }

            _redis.subscribe(id);

            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["id"] = user.getId();
            response["name"] = user.getName();
            response["errno"] = 0;
            vector<string> vec = _offMsgModel.query(id);
            if (!vec.empty())
            {
                // std::cout << "#################################"<< std::endl;
                response["offlinemsg"] = vec;
                _offMsgModel.remove(id);
            }
            // 登录成功自动返回好友
            vector<User> uvec = _addFriendModel.query(id);
            if (!uvec.empty())
            {
                vector<std::string> vec2;
                for (auto &u : uvec)
                {
                    json js;
                    js["id"] = u.getId();
                    js["name"] = u.getName();
                    js["state"] = u.getState();
                    vec2.push_back(js.dump());
                }
                response["friendlist"] = vec2;
            }

            vector<Group> gVec = _groupModel.queryGroups(id);
            if (!gVec.empty())
            {
                vector<std::string> vec2;
                for (auto &group : gVec)
                {
                    json js;
                    js["id"] = group.getId();
                    js["groupname"] = group.getName();
                    js["groupdesc"] = group.getDesc();
                    // vector<GroupUser> groupuser = group.getUsers();
                    vector<string> guVec;
                    for (auto &gu : group.getUsers())
                    {
                        json guserjs;
                        guserjs["id"] = gu.getId();
                        guserjs["name"] = gu.getName();
                        guserjs["state"] = gu.getState();
                        guserjs["role"] = gu.getRole();
                        guVec.push_back(guserjs.dump());
                    }
                    js["groupurs"] = guVec;
                    vec2.push_back(js.dump());
                }
                response["grouplist"] = vec2;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 用户名或者密码不正确，登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        // response["id"] = user.getId();
        // response["name"] = user.getName();
        response["errno"] = 1;
        response["errmsg"] = "The user id or password is incorrect!";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    std::string name = js["name"];
    std::string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = _userModel.insert(user);
    if (state)
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["id"] = user.getId();
        response["errno"] = 0;
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["id"] = user.getId();
        response["errno"] = 1;
        conn->send(response.dump());
    }
    // LOG_INFO << "do reg service!!!";
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMapMtx);
        auto it = _connectionMap.begin();
        for (; it != _connectionMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // user = _userModel.query(it->first);
                user.setId(it->first);
                _connectionMap.erase(it);
                _redis.unsubscribe(it->first);
                break;
            }
        }
    }

    user.setState("offline");
    _userModel.updateState(user);
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int tid = js["tid"].get<int>();
    {
        lock_guard<mutex> lock(_connMapMtx);
        auto it = _connectionMap.find(tid);
        // 在线消息发送
        if (it != _connectionMap.end())
        {
            it->second->send(js.dump());
            return;
        }
    }

    User user = _userModel.query(tid);
    if (user.getState() == "online")
    {
        _redis.publish(tid, js.dump());
        return;
    }

    // 离线消息存储
    _offMsgModel.insert(tid, js.dump());

}

void ChatService::reset()
{
    _userModel.resetState();
}

// 添加好友
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int fid = js["friendid"].get<int>();

    json response;
    response["msgid"] = ADD_FRIEND_MSG_ACK;
    response["requestid"] = userid;
    response["message"] = js["message"];
    {
        lock_guard<mutex> lock(_connMapMtx);
        auto it = _connectionMap.find(fid);
        // 在线消息发送
        if (it != _connectionMap.end())
        {
            it->second->send(response.dump());
            return;
        }
    }
    User user = _userModel.query(fid);
    if (user.getState() == "online")
    {
        _redis.publish(fid, response.dump());
        return;
    }
    // 离线消息存储
    _offMsgModel.insert(fid, response.dump());
}

void ChatService::acceptAddFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int rid = js["requestid"].get<int>();
    int reject = js["reject"].get<int>();

    json response;
    response["msgid"] = ACCEPT_MSG_ACK;
    response["requestid"] = rid;
    response["message"] = js["message"];

    if (!reject)
    {
        _addFriendModel.insert(userid, rid);
    }
    {
        lock_guard<mutex> lock(_connMapMtx);
        auto it = _connectionMap.find(rid);
        // 在线消息发送
        if (it != _connectionMap.end())
        {
            it->second->send(response.dump());
            return;
        }
    }
    User user = _userModel.query(rid);
    if (user.getState() == "online")
    {
        _redis.publish(rid, response.dump());
        return;
    }
    // 离线消息存储
    _offMsgModel.insert(rid, response.dump());
}

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    std::string groupname = js["groupname"];
    std::string groupdesc = js["groupdesc"];
    // 还不知道群id
    Group group(-1, groupname, groupdesc);
    // group.setId(id);
    // group.setName(groupname);
    // group.setDesc(groupdesc);

    if (_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"].get<int>();
    int groupid = js["groupid"].get<int>();
    std::string grouprole = "normal";

    _groupModel.addGroup(userid, groupid, grouprole);
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"].get<int>();
    int groupid = js["groupid"].get<int>();

    std::vector<int> ursVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMapMtx);
    for (auto &uid : ursVec)
    {
        User user = _userModel.query(uid);
        auto it = _connectionMap.find(uid);
        if (it != _connectionMap.end())
        {
            it->second->send(js.dump());
        }
        else if(user.getState() == "online")
        {
            _redis.publish(uid, js.dump());
        }
        else
        {
            _offMsgModel.insert(uid, js.dump());
        }
    }
}

void ChatService::logOut(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMapMtx);
        auto it = _connectionMap.find(userid);
        if (it != _connectionMap.end())
        {
            _connectionMap.erase(it);
        }
    }

    User user(userid, "", "", "offline");
    _userModel.updateState(user);
    _redis.unsubscribe(userid);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubsMessage(int userid, string message)
{
    lock_guard<mutex> lock(_connMapMtx);
    auto it = _connectionMap.find(userid);
    if(it != _connectionMap.end())
    {
        it->second->send(message);
        return;
    }

    _offMsgModel.insert(userid, message);
}