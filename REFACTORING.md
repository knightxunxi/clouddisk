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

## 后续建议

共享协议抽取完成后，下一阶段可以继续拆分 TCP 收包/发包逻辑：

- 新增 `PacketCodec` 处理 PDU 编解码。
- 新增 socket 缓冲区，统一处理粘包和半包。
- 将服务端 `MyTcpSocket::recvMsg()` 中的业务分发拆到 handler/service。
- 将文件路径校验和文件系统操作抽到服务端 `storage` 模块。

