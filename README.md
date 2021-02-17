## 介绍
- ChatRoom是基于TCP的网络聊天室，采用c语言编写，实现了用户注册登录，好友管理，群管理，聊天通讯，消息管理，文件传输等功能。
## 运行环境
- 系统：ubuntu19.10
-  处理器：Intel® Core™ i5-7200U CPU @ 2.50GHz × 4
-  内存：7.7 GiB
-  编译器：gcc version 9.2.1
## 特性
- 基于C/S模型设计，服务端采用I/O多路复用及线程池技术，客户端分为收发线程以实现逻辑上的清晰处理。
- 线程池创建固定线程数，当有新任务到来时发出请求，随机空闲线程进行处理。
- 使用MySQL数据库储存数据，采用结构体传输消息。
- 服务器初始化时创建3个链表读取数据库表信息，服务器退出时更新数据库信息。
- 实现优雅关闭连接。
## 目录说明
| 名称 | 作用 |
|--|--|
| client | 客户端 |
| datastructure | 相关数据结构 |
| image | 运行截图 |
| server | 服务端 |
| threadpool | 线程池 |
## 安装及运行
- 服务端
```
cd server
make
```
- 客户端
```
cd client
make
```
## 运行截图
- 服务端

![image --server](https://raw.githubusercontent.com/yyyupeng/chatroom/master/image/server.png)
- 客户端

![image --client-login](https://raw.githubusercontent.com/yyyupeng/chatroom/master/image/login.png)
![image --client-menu](https://raw.githubusercontent.com/yyyupeng/chatroom/master/image/menu.png)
