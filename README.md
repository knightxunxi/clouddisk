# TinyDisk

TinyDisk 是一个基于 Qt 的简易网盘系统，包含客户端和服务端两个独立 qmake 项目。系统通过 TCP 自定义协议通信，服务端使用 SQLite 保存用户与好友关系，文件数据存储在服务端本地目录中。

## 功能概览

客户端功能：

- 用户注册、登录
- 查看在线用户
- 查找用户、添加好友、删除好友
- 好友私聊、好友群发消息
- 文件夹创建、进入、返回、删除
- 文件刷新、上传、下载、删除、重命名
- 文件分享、移动
- 上传/下载进度显示
- 下载暂停与重新登录后的断点续传

服务端功能：

- 监听客户端 TCP 连接
- 分发注册、登录、好友、聊天和文件操作请求
- 维护用户在线状态
- 使用 SQLite 管理用户信息和好友关系
- 展示在线/离线用户列表
- 查看指定用户的网盘文件树
- 输出上传/下载开始、暂停、完成、失败等运行日志
- 运行日志支持按协议行为类别和关键词实时筛选

## 技术栈

- C++ / Qt Widgets
- Qt Network
- Qt SQL
- SQLite
- qmake

当前本地验证环境：

- Qt 6.10.2 MinGW 64-bit
- Qt 自带 MinGW 13.1 工具链

如果使用 Qt Creator，建议选择与 Qt 安装版本匹配的 Kit。不要混用其他 MinGW 工具链，否则可能出现链接错误。

## 项目结构

```text
TinyDisk-main/
├── common/                    # 客户端/服务端共享代码
│   └── protocol/              # PDU 协议定义与 qmake 引入文件
│       ├── protocol.h
│       ├── protocol.cpp
│       ├── packetcodec.h
│       ├── packetcodec.cpp
│       ├── pdufieldcodec.h
│       ├── pdufieldcodec.cpp
│       └── protocol.pri
│
├── tcpClient/                 # 客户端项目
│   ├── tcpClient.pro
│   ├── forms/                 # Qt Designer UI 文件
│   ├── resources/             # 客户端配置与图标资源
│   │   ├── client.config
│   │   ├── config.qrc
│   │   ├── FileType.qrc
│   │   └── icons/
│   └── src/
│       ├── main.cpp
│       ├── handlers/          # 客户端响应分发与处理
│       ├── network/           # TCP 客户端与登录窗口
│       ├── ui/                # 主界面容器
│       ├── transfer/          # 下载传输状态
│       └── features/
│           ├── files/         # 文件管理与分享
│           └── friends/       # 好友、在线用户、私聊
│
└── tcpServer/                 # 服务端项目
    ├── tcpServer.pro
    ├── forms/                 # 服务端 UI 文件
    ├── resources/             # 服务端配置资源
    │   ├── server.config
    │   └── config.qrc
    └── src/
        ├── main.cpp
        ├── handlers/          # 服务端请求分发与业务处理
        ├── database/          # SQLite 访问层
        ├── storage/           # 服务端文件系统操作
        ├── transfer/          # 上传传输状态
        ├── workers/           # 文件传输、复制、删除等后台任务
        ├── network/           # TCP Server 与客户端 Socket
        └── ui/                # 服务端管理界面
```

## 配置说明

客户端配置文件：

```text
tcpClient/resources/client.config
```

默认内容：

```text
127.0.0.1
8888
```

服务端配置文件：

```text
tcpServer/resources/server.config
```

默认内容：

```text
127.0.0.1
8888
cloud.db
```

其中：

- 第一行是监听 IP
- 第二行是监听端口
- 第三行是 SQLite 数据库文件名

数据库文件和构建产物已在 `.gitignore` 中忽略，不建议提交到仓库。

## 构建与运行

方式一：使用 Qt Creator

1. 打开 `tcpServer/tcpServer.pro`
2. 选择合适的 Qt Kit，构建并运行服务端
3. 打开 `tcpClient/tcpClient.pro`
4. 构建并运行客户端
5. 先启动服务端，再启动客户端

方式二：命令行构建

请先确保 Qt 的 `qmake` 和匹配的 MinGW 工具链在 `PATH` 中。

客户端：

```powershell
cd tcpClient
mkdir build_check
cd build_check
qmake ..\tcpClient.pro
mingw32-make -j4
```

服务端：

```powershell
cd tcpServer
mkdir build_check
cd build_check
qmake ..\tcpServer.pro
mingw32-make -j4
```

## 使用说明

1. 启动服务端。
2. 启动一个或多个客户端。
3. 在客户端注册用户并登录。
4. 每个用户注册成功后，服务端会在当前运行目录下创建同名网盘根目录。
5. 好友聊天、文件分享等交互功能需要同时登录多个客户端账号进行测试。

## 数据存储

- 用户、密码哈希、在线状态、好友关系存储在 SQLite 数据库中。
- 用户文件存储在服务端运行目录下的用户同名文件夹中。
- 服务端启动时会自动创建必要的数据表。

## 开发备注

- 客户端和服务端通过 `common/protocol/protocol.pri` 共享同一份协议代码。
- 普通 PDU 收包通过 `common/protocol/PacketCodec` 缓冲解析，避免直接假定一次 socket 读取就是完整包。
- PDU 定长字段和传输字段通过 `common/protocol/PduFieldCodec` 统一解析和写入。
- 上传/下载文件数据也已封装为 PDU 数据帧，客户端和服务端不再在同一 TCP 连接中切换裸文件流模式。
- 下载暂停通过 `ENUM_MSG_TYPE_CANCEL_DOWNLOAD_REQUEST` 通知服务端停止当前下载 worker。
- 客户端本地文件写入完成后通过 `ENUM_MSG_TYPE_DOWNLOAD_COMPLETE_REQUEST` 通知服务端，服务端据此记录真正的下载完成。
- 客户端会记录远程文件和本地保存路径的续传关系，重新登录后再次下载同一文件时可继续写入未完成文件。
- 当前传输模型是一条文件任务一个后台线程：上传由客户端 `UploadThread` 读取文件，下载由服务端 `FileWorker` 读取文件、客户端 `DownloadThread` 写文件；暂未做同一文件的多线程分片并发传输。
- 服务端运行日志通过 `MyTcpSocket -> MyTcpServer -> tcpServer UI` 信号链路上报，支持按系统、连接、文件、上传、下载、异常等协议行为筛选。
- 服务端 `MyTcpSocket::recvMsg()` 只负责收包循环，具体注册/好友/文件请求已拆到 `tcpServer/src/handlers`。
- 服务端文件路径解析、目录列表、删除、重命名、移动等文件系统操作已集中到 `tcpServer/src/storage/StorageService`。
- 上传/下载进度状态已分别收敛到服务端 `UploadSession` 和客户端 `DownloadSession`。
- 客户端响应处理已拆到 `tcpClient/src/handlers/responsehandler.cpp`，`tcpClient::recvMsg()` 只保留收包循环。
- 服务端后台文件任务已拆到 `tcpServer/src/workers/FileWorker`，`MyTcpSocket` 只保留线程启动和信号连接。
- 后续建议优先做完整功能回归和协议/路径安全测试补充。
