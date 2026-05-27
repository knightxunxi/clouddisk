#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QTcpServer>
#include <QList>
#include "mytcpsocket.h"

class MyTcpServer : public QTcpServer
{
    Q_OBJECT
private:

public:
    MyTcpServer();
    static MyTcpServer &getInstance();
    void incomingConnection(qintptr socketDescriptor);

    void resend(const char *pername, PDU *pdu);

signals:
    // 任意用户注册、登录或下线时发出此信号，UI 收到后刷新用户列表
    void userStatusChanged();
    // 服务端运行日志：category 用于 UI 实时筛选，message 是展示文本
    void runtimeLog(const QString &category, const QString &message);

public slots:
    void deleteSocket(MyTcpSocket *mysocket);

private:
    QList<MyTcpSocket*> m_tcpSocketList;
};

#endif // MYTCPSERVER_H

