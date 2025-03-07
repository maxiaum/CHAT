#ifndef PUBLIC_H
#define PUBLIC_H

enum EnMsgType
{
    LOGIN_MSG = 1, // 登录消息
    REG_MSG, // 注册消息
    REG_MSG_ACK, // 注册响应消息
    LOGIN_MSG_ACK, // 登录响应消息
    ONE_CHAT_MSG, // 聊天消息
    ADD_FRIEND_MSG, // 添加好友消息
    ADD_FRIEND_MSG_ACK, // 添加好友消息响应
    ACCEPT_MSG, // 同意添加好友消息
    ACCEPT_MSG_ACK, // 同意添加好友消息响应
    CREATE_GROUP_MSG, // 创建群聊消息
    ADD_GROUP_MSG, // 加入群聊
    GROUP_CHAT_MSG, // 群聊天
    LOGOUT_MSG, // 注销
};

#endif