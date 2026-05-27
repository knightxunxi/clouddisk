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
    if (!m_activeDownloadFileName.isEmpty()) {
        if (m_downloadCanceledByClient) {
            emit behaviorLog(QStringLiteral("下载"),
                             QStringLiteral("用户 %1 暂停下载：%2")
                             .arg(m_strName, m_activeDownloadFileName));
            m_activeDownloadPath.clear();
            m_activeDownloadFileName.clear();
            m_downloadCanceledByClient = false;
            m_downloadSendCompleted = false;
        } else if (success) {
            emit behaviorLog(QStringLiteral("下载"),
                             QStringLiteral("用户 %1 下载数据发送完毕：%2，等待客户端确认")
                             .arg(m_strName, m_activeDownloadFileName));
            m_downloadSendCompleted = true;
        } else {
            emit behaviorLog(QStringLiteral("异常"),
                             QStringLiteral("用户 %1 下载失败：%2")
                             .arg(m_strName, m_activeDownloadFileName));
            m_activeDownloadPath.clear();
            m_activeDownloadFileName.clear();
            m_downloadCanceledByClient = false;
            m_downloadSendCompleted = false;
        }
    }
    m_fileWorker = nullptr;  // deleteLater 已由 finished 信号触发
}

void MyTcpSocket::handleCancelDownloadRequest(PDU *pdu)
{
    Q_UNUSED(pdu);
    if (!m_fileWorker) {
        if (!m_activeDownloadFileName.isEmpty()) {
            emit behaviorLog(QStringLiteral("下载"),
                             QStringLiteral("用户 %1 暂停下载：%2")
                             .arg(m_strName, m_activeDownloadFileName));
            m_activeDownloadPath.clear();
            m_activeDownloadFileName.clear();
            m_downloadCanceledByClient = false;
            m_downloadSendCompleted = false;
        } else {
            qDebug() << "[MyTcpSocket] 收到暂停下载请求，但当前没有下载任务，用户：" << m_strName;
        }
        return;
    }

    qDebug() << "[MyTcpSocket] 用户暂停下载：" << m_strName;
    m_downloadCanceledByClient = true;
    m_fileWorker->cancel();
    m_fileWorker->wait(2000);
    m_fileWorker = nullptr;
}

void MyTcpSocket::handleDownloadCompleteRequest(PDU *pdu)
{
    QString remotePath;
    if (pdu && pdu->uiMsgLen > 0) {
        QByteArray bytes(pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
        const int terminator = bytes.indexOf('\0');
        if (terminator >= 0) {
            bytes.truncate(terminator);
        }
        remotePath = QString::fromUtf8(bytes);
    }

    const QString fileText = !m_activeDownloadFileName.isEmpty()
            ? m_activeDownloadFileName
            : remotePath;
    emit behaviorLog(QStringLiteral("下载"),
                     QStringLiteral("用户 %1 完成下载：%2")
                     .arg(m_strName, fileText));

    m_activeDownloadPath.clear();
    m_activeDownloadFileName.clear();
    m_downloadCanceledByClient = false;
    m_downloadSendCompleted = false;
}

void MyTcpSocket::handleUploadData(const QByteArray &buffer)
{
    if (buffer.isEmpty()) {
        return;
    }

    const UploadSession::WriteResult result = m_uploadSession.writeBlock(buffer);
    if (result == UploadSession::WriteComplete) {
        emit behaviorLog(QStringLiteral("上传"),
                         QStringLiteral("用户 %1 完成上传：%2")
                         .arg(m_strName, m_activeUploadFileName));
        m_activeUploadPath.clear();
        m_activeUploadFileName.clear();
        sendUploadResponse(this, UPLOAD_FILE_OK);
        return;
    }
    if (result == UploadSession::WriteFailed) {
        emit behaviorLog(QStringLiteral("异常"),
                         QStringLiteral("用户 %1 上传失败：%2")
                         .arg(m_strName, m_activeUploadFileName));
        m_activeUploadPath.clear();
        m_activeUploadFileName.clear();
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
        m_fileWorker = nullptr;
    }
    m_uploadSession.reset();
    if (!m_activeUploadFileName.isEmpty()) {
        emit behaviorLog(QStringLiteral("上传"),
                         QStringLiteral("用户 %1 断开连接，上传中断：%2")
                         .arg(m_strName, m_activeUploadFileName));
    }
    if (!m_activeDownloadFileName.isEmpty()) {
        emit behaviorLog(QStringLiteral("下载"),
                         QStringLiteral("用户 %1 断开连接，下载中断：%2")
                         .arg(m_strName, m_activeDownloadFileName));
    }
    m_activeUploadPath.clear();
    m_activeUploadFileName.clear();
    m_activeDownloadPath.clear();
    m_activeDownloadFileName.clear();
    m_downloadCanceledByClient = false;
    m_downloadSendCompleted = false;
    emit offline(this);
}
