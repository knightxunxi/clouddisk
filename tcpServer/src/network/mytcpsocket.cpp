#include "mytcpsocket.h"
#include "fileworker.h"
#include "opedb.h"
#include <QDebug>
#include <mytcpserver.h>
#include <cstring>

// ═══════════════════════════════════════════════
//  MyTcpSocket 实现
// ═══════════════════════════════════════════════

MyTcpSocket::MyTcpSocket()
{
    connect(this, SIGNAL(readyRead()),    this, SLOT(recvMsg()));
    connect(this, SIGNAL(disconnected()), this, SLOT(clientOffline()));
    m_bUpload    = false;
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
    write(data);
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

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND;
    qint64 bytesRead = buffer.size();
    m_file.write(buffer);
    m_iReceived += bytesRead;

    if (m_iTotal == m_iReceived) {
        m_file.close();
        m_bUpload = false;
        strcpy(respdu->caData, UPLOAD_FILE_OK);
        write((char*)respdu, respdu->uiPDULen);
    } else if (m_iTotal < m_iReceived) {
        m_file.close();
        m_bUpload = false;
        strcpy(respdu->caData, UPLOAD_FILE_FAILED);
        write((char*)respdu, respdu->uiPDULen);
    }
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::recvMsg()
{
    if (!m_bUpload)
    {
        m_packetCodec.append(readAll());
        PDU *pdu = nullptr;
        while (!m_bUpload && (pdu = m_packetCodec.takePacket()) != nullptr)
        {
            handlePdu(pdu);
            free(pdu); pdu = nullptr;
        }

        if (m_bUpload) {
            handleUploadData(m_packetCodec.takeBufferedData());
        }
    }
    else
    {
        // 上传数据接收：文件内容仍按裸流模式处理。
        handleUploadData(readAll());
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
    emit offline(this);
}
