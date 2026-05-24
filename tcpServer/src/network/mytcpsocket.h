#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QTcpSocket>
#include "protocol.h"
#include "packetcodec.h"
#include <QByteArray>
#include <QFile>
#include <QString>

class FileWorker;

// ─────────────────────────────────────────────────
// MyTcpSocket：单个客户端连接处理
// ─────────────────────────────────────────────────
class MyTcpSocket : public QTcpSocket
{
    Q_OBJECT
public:
    MyTcpSocket();
    QString getName();

signals:
    void offline(MyTcpSocket *mysocket);
    void userListChanged();   // 注册、登录等用户列表变化后发出，通知 UI 刷新

public slots:
    void recvMsg();
    void clientOffline();

    // 收到下载线程产生的数据块，写入 socket（在主线程执行，不阻塞）
    void onFileDataBlock(const QByteArray &data);
    // 文件工作线程结束
    void onFileWorkerFinished(bool success);

private:
    // 启动文件工作线程（先停止已有的）
    void startFileWorker(FileWorker *worker);
    void handlePdu(PDU *pdu);
    void handleUploadData(const QByteArray &buffer);

    void handleRegistRequest(PDU *pdu);
    void handleLoginRequest(PDU *pdu);
    void handleAllOnlineRequest(PDU *pdu);
    void handleSearchUserRequest(PDU *pdu);

    void handleAddFriendRequest(PDU *pdu);
    void handleAddFriendAgree(PDU *pdu);
    void handleAddFriendRefuse(PDU *pdu);
    void handleFlushFriendRequest(PDU *pdu);
    void handleDeleteFriendRequest(PDU *pdu);
    void handlePrivateChatRequest(PDU *pdu);
    void handleGroupChatRequest(PDU *pdu);

    void handleCreateDirRequest(PDU *pdu);
    void handleFlushFileRequest(PDU *pdu);
    void handleDeleteDirRequest(PDU *pdu);
    void handleRenameFileRequest(PDU *pdu);
    void handleEnterDirRequest(PDU *pdu);
    void handleUploadFileRequest(PDU *pdu);
    void handleDeleteFileRequest(PDU *pdu);
    void handleDownloadFileRequest(PDU *pdu);
    void handleShareFileRequest(PDU *pdu);
    void handleShareFileNoteRespond(PDU *pdu);
    void handleMoveFileRequest(PDU *pdu);

    QString  m_strName;
    QFile    m_file;        // 仅用于接收上传数据时写文件
    qint64   m_iTotal    = 0;
    qint64   m_iReceived = 0;
    bool     m_bUpload   = false;

    PacketCodec m_packetCodec;
    FileWorker *m_fileWorker = nullptr;  // 当前文件工作线程
};

#endif // MYTCPSOCKET_H
