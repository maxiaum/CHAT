#include "json.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>

using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 当前系统登录的用户信息
User g_currentUser;
// 当前登陆系统的用户好友列表
std::vector<User> g_currentUserFriendlist;
// 当前登录系统的用户群组信息
std::vector<Group> g_currentUserGroupList;

std::unordered_set<int> g_currentReqFriendList;

bool isMainmenu = false;
sem_t recvsem;
std::atomic_bool g_isLoginSuccess{false};
// int clientfd;
// int p = 0;

// 显示当前登录用户的基本信息
void showCurrentUserData();

// 接收子线程
void readTaskHandler(int clientfd);
// 获取系统当前时间
std::string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "command invalid! example command: 127.0.0.1 6006" << std::endl;
        exit(-1);
    }
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    // 服务器连接
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cerr << "socket create error!" << std::endl;
        exit(-1);
    }
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if (connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)) == -1)
    {
        std::cerr << "connection failed!" << std::endl;
        close(clientfd);
        exit(-1);
    }

    sem_init(&recvsem, 0, 0);
    // int readThreadNum = 0;
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    for (;;)
    {
        std::cout << "=================================" << std::endl;
        std::cout << "1. login" << std::endl;
        std::cout << "2. register" << std::endl;
        std::cout << "3. quit" << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << "please choice:";
        int choice = 0;
        std::cin >> choice;
        std::cin.get(); // 读缓冲区残留的回车

        switch (choice)
        {
        case 1:
        {
            int id = 0;
            char pwd[50] = {0};
            std::cout << "user id:";
            std::cin >> id;
            std::cin.get();
            std::cout << std::endl;
            std::cout << "password:";
            std::cin.getline(pwd, 50);
            std::cout << std::endl;

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            std::string request = js.dump();
            g_isLoginSuccess = false;
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                std::cerr << "login message error!" << std::endl;
            }
            sem_wait(&recvsem);
            if (g_isLoginSuccess)
            {
                isMainmenu = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2:
        {
            char nickname[50] = {0};
            char password[50] = {0};
            std::cout << "create nickname:";
            std::cin.getline(nickname, 50);
            std::cin.get();
            std::cout << "create password:";
            std::cin.getline(password, 50);
            std::cout << std::endl;

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = nickname;
            js["password"] = password;

            std::string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                std::cerr << "register message error!" << std::endl;
            }
            sem_wait(&recvsem);
        }
        break;
        case 3:
            close(clientfd);
            sem_destroy(&recvsem);
            exit(0);
        default:
            std::cerr << "invalid input!" << std::endl;
            exit(0);
        }
    }
}

void showCurrentUserData()
{
    std::cout << "Successfully Login!!!" << std::endl;
    std::cout << ">----------------------------------------------<" << std::endl;
    std::cout << "Friend List:" << std::endl;
    for (User &user : g_currentUserFriendlist)
    {
        std::cout << "friend:[" << user.getId() << "-" << user.getName() << "]: " << user.getState() << std::endl;
    }
    std::cout << ">----------------------------------------------<" << std::endl;
    std::cout << "Group List:" << std::endl;
    for (auto &group : g_currentUserGroupList)
    {
        std::cout << "group:" << group.getId() << "-" << group.getName()
                  << " " << group.getDesc() << std::endl;
        std::cout << "---" << std::endl;
        std::cout << "  ↓" << std::endl;
        for (auto &groupuser : group.getUsers())
        {
            std::cout << "member:" << groupuser.getId() << " " << groupuser.getName() << " "
                      << groupuser.getState() << " " << groupuser.getRole() << std::endl;
        }
    }
    std::cout << ">----------------------------------------------<" << std::endl;
}

void doLoginResponse(json &responsejs)
{
    if (responsejs["errno"].get<int>() == 1)
    {
        std::cerr << responsejs["errmsg"] << std::endl;
        g_isLoginSuccess = false;
    }
    else if (responsejs["errno"].get<int>() == 2)
    {
        std::cerr << "userid:" << responsejs["id"] << " " << "user:"
                  << responsejs["name"] << " " << responsejs["errmsg"] << std::endl;
        g_isLoginSuccess = false;
    }
    else
    {
        g_currentUser.setId(responsejs["id"]);
        g_currentUser.setName(responsejs["name"]);

        if (responsejs.contains("friendlist"))
        {
            // std::cout << "---------------------Friendlist----------------------" << std::endl;
            g_currentUserFriendlist.clear();
            std::vector<std::string> fList = responsejs["friendlist"];
            for (auto &fe : fList)
            {
                json js = json::parse(fe);
                User user;
                user.setId(js["id"]);
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendlist.push_back(user);
                // std::cout << "friend:" << friend["id"] << "-" << friend["name"]
                //<< "is" << friend["state"] << std::endl;
            }
            // std::cout << "-----------------------------------------------------" << std::endl;
            // std::cout << std::endl;
        }

        if (responsejs.contains("grouplist"))
        {
            // std::cout << "---------------------GroupList----------------------" << std::endl;
            g_currentUserGroupList.clear();
            std::vector<std::string> gList = responsejs["grouplist"];
            for (auto &ge : gList)
            {
                json js = json::parse(ge);
                Group group;
                // 群组信息
                group.setId(js["id"]);
                group.setName(js["groupname"]);
                group.setDesc(js["groupdesc"]);
                // 群组成员
                std::vector<std::string> groupuser = js["groupurs"];
                for (auto &gu : groupuser)
                {
                    GroupUser user;
                    json gurjs = json::parse(gu);
                    user.setId(gurjs["id"]);
                    user.setName(gurjs["name"]);
                    user.setState(gurjs["state"]);
                    user.setRole(gurjs["role"]);
                    group.getUsers().push_back(user);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        showCurrentUserData();

        if (responsejs.contains("offlinemsg"))
        {
            std::vector<std::string> offmsg = responsejs["offlinemsg"];
            for (auto &off : offmsg)
            {
                json info = json::parse(off);
                if (info.contains("groupid"))
                {
                    std::cout << "group[" << info["groupid"] << "]-" << "member[" << info["name"] << "]"
                              << "said:" << info["message"] << std::endl;
                }
                else if (info.contains("requestid"))
                {
                    std::cout << info["message"] << std::endl;
                }
                else
                {
                    std::cout << "friend:[" << info["fid"] << "-" << info["name"] << "]"
                              << "said:" << info["message"] << std::endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

void doRegisterResponse(json &responsejs)
{
    if (responsejs["errno"].get<int>() == 1)
    {
        std::cerr << "register failed, try again!" << std::endl;
    }
    else
    {
        std::cout << "register successfully, your id is" << " "
                  << responsejs["id"] << "do not forget it!" << std::endl;
    }
}
// 子线程处理接收业务
void readTaskHandler(int clientfd)
{
    while (true)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (len == 0 || len == -1)
        {
            close(clientfd);
            exit(-1);
        }
        json js = json::parse(buffer);
        int msgId = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgId)
        {
            std::cout << js["time"] << "friend:[" << js["fid"] << "-" << js["name"] << "]"
                      << "said:" << js["message"] << std::endl;
            continue;
        }
        if (GROUP_CHAT_MSG == msgId)
        {
            std::cout << js["time"] << "group[" << js["groupid"] << "]-" << "member[" << js["name"] << "]"
                      << "said:" << js["message"] << std::endl;
            continue;
        }

        if (ADD_FRIEND_MSG_ACK == msgId)
        {
            std::cout << js["message"] << std::endl;
            g_currentReqFriendList.insert(js["requestid"].get<int>());
            continue;
        }

        if (ACCEPT_MSG_ACK == msgId)
        {
            std::cout << js["message"] << std::endl;
            continue;
        }

        if (LOGIN_MSG_ACK == msgId)
        {
            doLoginResponse(js);
            sem_post(&recvsem);
            continue;
        }

        if (REG_MSG_ACK == msgId)
        {
            doRegisterResponse(js);
            sem_post(&recvsem);
            continue;
        }
    }
}

std::string getCurrentTime()
{
    time_t currTime;
    time(&currTime);
    std::string res = ctime(&currTime);
    res.erase(std::remove(res.begin(), res.end(), '\n'), res.end());
    return res;
}

void help(int fd = 0, std::string info = "")
{
    std::cout << "=================================================================" << std::endl;
    std::cout << "help---------->" << "显示所有支持的命令,格式help" << std::endl;
    std::cout << "chat---------->" << "一对一聊天,格式chat:friendid:message" << std::endl;
    std::cout << "addfriend----->" << "添加好友,格式addfriend:friendid" << std::endl;
    std::cout << "acceptaddfriend----->" << "添加好友,格式accept:acceptid:1(reject)/0(accept)" << std::endl;
    std::cout << "creategroup--->" << "创建群聊,格式creategroup:groupname:groupdesc" << std::endl;
    std::cout << "addgroup------>" << "加入群聊,格式addgroup:groupid" << std::endl;
    std::cout << "groupchat----->" << "群聊天,格式groupchat:groupid:message" << std::endl;
    std::cout << "quit---------->" << "退出登录,格式quit" << std::endl;
    std::cout << "=================================================================" << std::endl;
}

void chat(int clientfd, std::string info)
{
    int index = info.find(":");
    if (index == -1)
    {
        std::cerr << "invalid command!" << std::endl;
        return;
    }
    int tid = atoi(info.substr(0, index).c_str());
    std::string msg = info.substr(index + 1, info.size() - index - 1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["fid"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["tid"] = tid;
    js["message"] = msg;
    js["time"] = getCurrentTime();

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send onchat message error--->" << buffer << std::endl;
    }
}

void addFriend(int clientfd, std::string info)
{
    int friendid = atoi(info.c_str());

    json js;
    char message[128] = {0};
    sprintf(message, "[%d-%s] wants to add you as a friend!", g_currentUser.getId(), g_currentUser.getName().c_str()); 
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    js["message"] = message;

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send addfriend message error--->" << buffer << std::endl;
    }
}

void acceptAF(int clientfd, std::string info)
{
    int index = info.find(":");
    if (index == -1)
    {
        std::cerr << "invalid command!" << std::endl;
        return;
    }
    int friendid = atoi(info.substr(0, index).c_str());
    auto it = g_currentReqFriendList.find(friendid);
    if (it == g_currentReqFriendList.end())
    {
        return;
    }
    int reject = atoi(info.substr(index + 1, info.size() - index - 1).c_str());
    std::string message = reject == 1 ? "Your friend request has been rejected!!!" : "Your friend request has been approved, go chat!!!";
    json js;
    js["msgid"] = ACCEPT_MSG;
    js["id"] = g_currentUser.getId();
    js["requestid"] = friendid;
    js["reject"] = reject;
    js["message"] = message;

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    g_currentReqFriendList.erase(it);
    if (len == -1)
    {
        std::cerr << "send acceptaddfriend message error--->" << buffer << std::endl;
    }
}

void createGroup(int clientfd, std::string info)
{
    int index = info.find(":");
    if (index == -1)
    {
        std::cerr << "invalid command!" << std::endl;
        return;
    }
    std::string groupname = info.substr(0, index);
    std::string groupdesc = info.substr(index + 1, info.size() - index - 1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    // js["name"] = g_currentUser.getName();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    js["time"] = getCurrentTime();

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send create group message error--->" << buffer << std::endl;
    }
}

void addGroup(int clientfd, std::string info)
{
    int groupid = atoi(info.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["userid"] = g_currentUser.getId();
    js["groupid"] = groupid;

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send addgroup message error--->" << buffer << std::endl;
    }
}

void groupChat(int clientfd, std::string info)
{
    int index = info.find(":");
    if (index == -1)
    {
        std::cerr << "invalid command!" << std::endl;
        return;
    }
    int groupid = atoi(info.substr(0, index).c_str());
    std::string message = info.substr(index + 1, info.size() - index - 1);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["userid"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["message"] = message;
    js["time"] = getCurrentTime();

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send groupchat message error--->" << buffer << std::endl;
    }
}

void logOut(int clientfd, std::string info)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();

    std::string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        std::cerr << "send logout message error--->" << buffer << std::endl;
    }
    isMainmenu = false;
}

std::unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"accept", "接收好友请求, 格式accept:acceptid:1(reject)/0(accept)"},
    {"creategroup", "创建群聊,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群聊,格式addgroup:groupid"},
    {"groupchat", "群聊天,格式groupchat:groupid:message"},
    {"logout", "退出登录,格式quit"}};

std::unordered_map<std::string, std::function<void(int, std::string)>>
    commHandlerMap = {
        {"help", help}, {"chat", chat}, {"addfriend", addFriend}, {"accept", acceptAF}, {"creategroup", createGroup}, {"addgroup", addGroup}, {"groupchat", groupChat}, {"quit", logOut}};

void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainmenu)
    {
        std::cout << "#############" << std::endl;
        std::cin.getline(buffer, 1024);
        std::string commandBuf(buffer);
        std::string cmd;

        int index = commandBuf.find(":");

        if (index == -1)
        {
            cmd = commandBuf;
        }
        else
        {
            cmd = commandBuf.substr(0, index);
        }

        auto it = commHandlerMap.find(cmd);

        if (it != commHandlerMap.end())
        {
            it->second(clientfd, commandBuf.substr(index + 1, commandBuf.size() - index - 1));
            // p++;
            continue;
        }
        else
        {
            std::cerr << "invalid command!" << std::endl;
            continue;
        }
        // std::cout<<p<<"***********************"<<std::endl
    }
}