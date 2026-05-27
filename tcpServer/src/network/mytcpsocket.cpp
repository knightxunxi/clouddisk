#include "mytcpsocket.h"
#include "fileworker.h"
#include "opedb.h"
#include <QDebug>
#include <mytcpserver.h>
#include <cstring>

namespace {

void sendUploadResponse(QTcpSocket *socket, const char *message)
{
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND;
    strcpy(respdu->caData, message);
    socket->write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

} // namespace

// ═══════════════════════════════════════════════
//  MyTcpSocket 实现
// ═══════════════════════════════════════════════

MyTcpSocket::MyTcpSocket()
{
    connect(this, SIGNAL(readyRead()),    this, SLOT(recvMsg()));
    connect(this, SIGNAL(disconnected()), this, SLOT(clientOffline()));
    m_fileWorker = nullptr;
}

QString MyTcpSocket::getName()
{
    return m_strName;
}

// 启动文件工作线程（先优雅停止已有的）
void MyTcpSocket::startFileWorker(FileWorker *worker)
{
    // 先停止旧的（如果有）
    if (m_fileWorker) {
        m_fileWorker->cancel();
        m_fileWorker->wait(3000);  // 最多等 3 秒
        // 线程已 deleteLater 或已结束则安全
    }
    m_fileWorker = worker;
    connect(m_fileWorker, &FileWorker::dataBlock,
            this,          &MyTcpSocket::onFileDataBlock,
            Qt::QueuedConnection);
    connect(m_fileWorker, &FileWorker::taskFinished,
            this,          &MyTcpSocket::onFileWorkerFinished,
            Qt::QueuedConnection);
    connect(m_fileWorker, &FileWorker::finished,
            m_fileWorker,  &QObject::deleteLater);
    m_fileWorker->start();
}

// 收到子线程发来的数据块 → 写入 socket（主线程，非阻塞）
void MyTcpSocket::onFileDataBlock(const QByteArray &data)
{
    PDU *pdu = mkPDU(static_cast<unit>(data.size()));
    pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND;
    if (!data.isEmpty()) {
        memcpy(pdu->caMsg, data.constData(), data.size());
    }
    write((char*)pdu, pdu->uiPDULen);
    free(pdu); pdu = nullptr;
}

// 工作线程结束通知
void MyTcpSocket::onFileWorkerFinished(bool success)
{
    qDebug() << "[MyTcpSocket] FileWorker 结束，success=" << success;
    m_fileWorker = nullptr;  // deleteLater 已由 finished 信号触发
}

void MyTcpSocket::handleUploadData(const QByteArray &buffer)
{
    if (buffer.isEmpty()) {
        return;
    }

    const UploadSession::WriteResult result = m_uploadSession.writeBlock(buffer);
    if (result == UploadSession::WriteComplete) {
        sendUploadResponse(this, UPLOAD_FILE_OK);
        return;
    }
    if (result == UploadSession::WriteFailed) {
        sendUploadResponse(this, UPLOAD_FILE_FAILED);
    }
}

void MyTcpSocket::recvMsg()
{
    m_packetCodec.append(readAll());
    PDU *pdu = nullptr;
    while ((pdu = m_packetCodec.takePacket()) != nullptr)
    {
        handlePdu(pdu);
        free(pdu); pdu = nullptr;
    }
}
void MyTcpSocket::clientOffline()
{
    OpeDB::getInstance().handleOffline(m_strName.toStdString().c_str());
    // 停止正在进行的文件传输
    if (m_fileWorker) {
        m_fileWorker->cancel();
        m_fileWorker->wait(2000);
    }
    m_uploadSession.reset();
    emit offline(this);
}
