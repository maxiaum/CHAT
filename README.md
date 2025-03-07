# CHAT
基于muduo库的集群聊天服务器项目

### 项目介绍
在Linux开发环境下基于muduo网络库实现聊天服务器及本地客户端开发。包括新用户的注册、登录、添加好友、创建群组、加入群聊、单聊、群聊、离线消息等功能；采用Json格式数据进行消息传输，实现跨服务器聊天。
### 项目要点
1. 根据需求设计用户信息、好友信息、群组信息、离线消息等表用于项目有关数据存储；
2. 使用muduo网络库作为网络核心模块提供高并发网络I/O服务，实现网络模块与业务模块解耦；
3. 使用Json的序列化和反序列化消息作为私有通信协议；
4. 使用Nginx的TCP负载均衡实现聊天服务器集群功能，提高后端服务的并发能力；
5. 使用Redis的发布-订阅功能实现用户跨服务器聊天通信。
### 编译方式

            cd ./build
            rm -rf *
            cmake ..
            make
#### 或者
            sh autobuild.sh
