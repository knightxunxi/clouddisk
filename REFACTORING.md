# TinyDisk 重构流程

本文档记录当前项目从“客户端/服务端各自维护协议代码”向“共享协议模块”演进的重构流程。目标是在不改变业务行为和 Qt Creator 构建方式的前提下，先消除协议定义重复，为后续网络层和业务层拆分打基础。

## 重构目标

1. 在根目录新增 `common/protocol` 共享协议模块。
2. 客户端和服务端通过 qmake `.pri` 文件引用同一份协议源码。
3. 移除 `tcpClient/src/core/protocol.*` 和 `tcpServer/src/core/protocol.*` 的重复维护。
4. 保持 Qt Creator 原有使用方式：分别打开 `tcpClient.pro`、`tcpServer.pro` 构建运行。
5. 不改动现有 PDU 格式、消息枚举值和业务逻辑。

## 目标目录

```text
TinyDisk-main/
├── common/
│   └── protocol/
│       ├── protocol.h
│       ├── protocol.cpp
│       └── protocol.pri
├── tcpClient/
│   └── tcpClient.pro
└── tcpServer/
    └── tcpServer.pro
```

## 实施步骤

1. 从客户端现有协议文件迁移出共享协议源码：
   - `tcpClient/src/core/protocol.h` -> `common/protocol/protocol.h`
   - `tcpClient/src/core/protocol.cpp` -> `common/protocol/protocol.cpp`
2. 删除服务端重复协议文件：
   - `tcpServer/src/core/protocol.h`
   - `tcpServer/src/core/protocol.cpp`
3. 新增 `common/protocol/protocol.pri`：
   - 统一添加协议头文件路径
   - 统一添加协议源文件和头文件
4. 更新 `tcpClient/tcpClient.pro`：
   - 删除 `src/core` 的本地协议路径和源码引用
   - 添加 `include(../common/protocol/protocol.pri)`
5. 更新 `tcpServer/tcpServer.pro`：
   - 删除 `src/core` 的本地协议路径和源码引用
   - 添加 `include(../common/protocol/protocol.pri)`
6. 使用 Qt 匹配的 MinGW 工具链重新执行 qmake 并构建客户端和服务端。

## 验证标准

- 客户端和服务端均能通过 qmake 重新生成 Makefile。
- 客户端和服务端均能成功编译链接。
- 所有 `#include "protocol.h"` 均通过共享模块解析。
- 仓库中只剩一份协议定义。

## 第二阶段：PDU 收包 codec

共享协议抽取后，新增 `PacketCodec` 处理普通 PDU 模式下的 TCP 缓冲区：

- 通过内部 `QByteArray` 累积 socket 收到的数据。
- 只有收到完整 PDU 后才返回可处理的 `PDU*`。
- 一次 `readyRead` 中包含多个 PDU 时，可以逐个取出处理。
- 发现异常 PDU 长度时清空缓冲区，避免继续按错误长度解析。
- 在客户端进入下载模式、服务端进入上传模式时，将 codec 中剩余数据转交给原有文件裸流处理逻辑。

本阶段保持现有文件传输行为不变：文件内容仍然在同一条 TCP 连接上以裸数据流传输，只是普通 PDU 收包不再直接从 socket 中假定一次读完整包。

## 第三阶段：服务端请求处理拆分

`MyTcpSocket::recvMsg()` 原本同时承担 socket 读取、PDU 分发、账号逻辑、好友逻辑和文件操作，函数体过长，后续维护成本较高。本阶段先做低风险拆分：

- `MyTcpSocket::recvMsg()` 保留 socket 收包循环和上传裸流接收。
- 新增 `tcpServer/src/handlers/requestdispatcher.cpp`，集中按 `ENUM_MSG_TYPE_*` 分发请求。
- 新增 `sessionhandler.cpp`，处理注册、登录、在线用户、搜索用户。
- 新增 `friendhandler.cpp`，处理好友申请、好友列表、删除好友、私聊、群聊。
- 新增 `filehandler.cpp`，处理目录、文件、上传、下载、分享、移动。
- 仍然使用 `MyTcpSocket` 私有成员函数承载业务，避免本阶段大范围修改状态访问和信号连接方式。

本阶段的目标不是改变业务行为，而是降低 `MyTcpSocket` 单文件复杂度，为后续将文件系统操作抽成 `storage/service` 做准备。

## 第四阶段：服务端 storage 模块

文件 handler 拆分后，`filehandler.cpp` 仍然直接处理大量路径拼接、目录列表、文件删除、重命名和移动逻辑。本阶段新增 `tcpServer/src/storage/StorageService`：

- 统一从 PDU 消息体解析路径，处理尾部 `\0`。
- 统一拼接父目录与子文件/目录名。
- 封装创建目录、目录列表、文件/目录判断、删除文件、重命名和移动文件。
- 封装上传断点续传校验和下载文件大小/跳过字节计算。
- 分享接收方目标路径由 storage 模块统一生成。

`filehandler.cpp` 现在主要负责解析业务字段、组装响应 PDU、启动异步 `FileWorker`，文件系统细节集中到 `StorageService`。这一步仍然不改变现有通信协议和文件传输模式。

## 第五阶段：后台文件任务模块

`FileWorker` 原本定义在 `MyTcpSocket` 头文件中、实现放在 `mytcpsocket.cpp` 顶部，使 socket 连接类暴露了文件复制、目录递归删除、下载分块发送等后台任务细节。本阶段将其移入独立模块：

- 新增 `tcpServer/src/workers/fileworker.h`。
- 新增 `tcpServer/src/workers/fileworker.cpp`。
- `MyTcpSocket` 头文件只保留 `FileWorker` 前向声明。
- `mytcpsocket.cpp` 负责启动 worker、连接信号、接收数据块写入 socket。
- `filehandler.cpp` 在需要删除目录、下载文件、接受分享复制时创建对应 worker。

这一步让网络连接类、业务 handler、文件系统 service、后台任务 worker 四类职责更清楚，也降低了 `mytcpsocket.h` 被其它文件包含时的编译耦合。

## 第六阶段：StorageService 路径安全校验

服务端文件操作原先直接信任客户端传来的当前路径和文件名，例如 `./用户名/目录`。如果客户端伪造 `../`、绝对路径或其它用户目录，服务端缺少统一拦截。本阶段将路径安全规则集中到 `StorageService`：

- 新增用户网盘根目录解析，统一以服务端当前工作目录下的 `./用户名` 为用户根目录。
- 拒绝空路径、绝对路径、包含 `../` 的路径，以及包含路径分隔符的文件名。
- 文件刷新、创建目录、删除、重命名、进入目录、上传、下载、分享、移动等入口都先校验路径，再执行文件系统操作。
- 普通文件操作限制在当前登录用户自己的网盘根目录内，避免跨用户目录访问。
- 分享发送方会校验被分享路径确实属于发送者；接受分享时，目标路径使用当前 socket 登录用户，避免客户端伪造接收者目录。
- 注册时新增用户名合法性检查，并通过 `StorageService::userRootPath()` 创建用户根目录。

## 后续建议

共享协议和普通 PDU codec 完成后，下一阶段可以继续拆分网络与业务逻辑：

- 将文件流也封装为明确长度的数据帧，逐步移除“PDU 模式/裸流模式”切换。
- 为 `StorageService` 增加路径越界校验，避免客户端传入非预期目录。
- 将上传/下载状态抽成独立传输会话对象，减少 `MyTcpSocket` 中的传输状态字段。

## 待优化清单

以下优化暂不立即实施，先保留为后续重构列表。建议先运行当前版本，验证注册、登录、好友、聊天、上传、下载、删除、分享、移动等功能，再结合实际 bug 选择下一步。

1. `StorageService` 路径安全校验
   - 拒绝 `../`、绝对路径、空路径、跨用户目录等非预期路径。
   - 将客户端传入路径限制在服务端允许的用户网盘根目录内。
   - 优先级：高，涉及数据安全。
   - 状态：已完成。见“第六阶段：StorageService 路径安全校验”。

2. 文件传输协议帧化
   - 将上传/下载文件流封装为明确长度的数据帧。
   - 逐步移除同一 TCP 连接中“PDU 模式/裸流模式”的切换。
   - 优先级：高，涉及半包、粘包和大文件稳定性。

3. 上传/下载传输状态对象
   - 新增类似 `TransferSession` 的对象保存当前文件、总大小、已传大小、方向和状态。
   - 减少 `MyTcpSocket` 中的 `m_file`、`m_iTotal`、`m_iReceived`、`m_bUpload` 等传输字段。
   - 优先级：中高，便于后续支持多任务或更清晰的断点续传。

4. PDU 字段解析工具
   - 抽出用户名、密码、文件名、路径、长度字段等解析逻辑。
   - 减少散落的 `strncpy`、`sscanf`、`memcpy`。
   - 优先级：中，主要降低重复代码和字符串截断风险。

5. 客户端响应 handler 拆分
   - 将 `tcpClient::recvMsg()` 中的大型响应 switch 按登录、好友、文件、分享等功能拆分。
   - 对齐服务端已经拆出的 `handlers` 结构。
   - 优先级：中，主要改善客户端可维护性。

## 功能验证与 bug 记录

当前建议先用 Qt Creator 或命令行构建后的程序做一轮人工验证。发现问题后，可以按下面格式补充：

```text
Bug 标题：服务端界面上有 在线用户与非在线用户的查看，但是打开服务端时在线的不会刷新，离线的不显示
复现步骤：重复打开服务端
预期结果：应该刷新与能够查看
实际结果：不会刷新
相关功能：登录 /数据库读取？
严重程度： 中
备注：应该是逻辑缺失，可以在合适的模块新增逻辑。顺便检查并修复一下其他可能的逻辑确实(对比ui上应该需要的功能)
修复状态：已修复。服务端启动时重置数据库残留在线状态；注册、登录、下线都会触发用户列表刷新；去除了刷新按钮重复连接。

Bug 标题：客户端登陆界面，账号与密码颜色太浅 看不到
相关功能：ui
严重程度：低
备注：可以把服务的与客户端的所有ui都统一为浅色ui，字体为中文(全部设置为黑色)
修复状态：已修复。客户端与服务端设置中文字体，登录框、列表、按钮、聊天窗口、管理界面改为浅色 UI，并显式设置文字为黑色。
```
