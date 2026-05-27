#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QByteArray>
#include <QDebug>
#include <QMessageBox>
#include <QHostAddress>
#include <QRegularExpression>
#include "pdufieldcodec.h"

/* ========== DownloadThread 实现 ========== */

void DownloadThread::setup(const QString &filePath, qint64 offset)
{
    m_filePath = filePath;
    m_offset = offset;
    m_canceled = false;
    {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
    }
}

void DownloadThread::cancel()
{
    {
        QMutexLocker locker(&m_mutex);
        m_canceled = true;
        m_cond.wakeAll();
    }
}

void DownloadThread::enqueueData(const QByteArray &data)
{
    {
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue(data);
    }
    m_cond.wakeAll();
}

void DownloadThread::run()
{
    QFile file(m_filePath);
    bool openSuccess = false;

    if (m_offset == 0) {
        openSuccess = file.open(QIODevice::WriteOnly);
    } else {
        if (file.exists() && file.size() == m_offset) {
            openSuccess = file.open(QIODevice::WriteOnly | QIODevice::Append);
        } else {
            // 文件不存在或大小不匹配，从头开始
            openSuccess = file.open(QIODevice::WriteOnly);
            m_offset = 0;
        }
    }

    if (!openSuccess) {
        emit finished(false, QString("无法打开文件: %1").arg(m_filePath));
        return;
    }

    if (m_offset > 0) {
        file.seek(m_offset);
    }

    qint64 totalWritten = 0;
    forever {
        QByteArray data;
        {
            QMutexLocker locker(&m_mutex);
            while (m_queue.isEmpty() && !m_canceled) {
                m_cond.wait(&m_mutex, 1000);
            }
            if (m_canceled && m_queue.isEmpty()) {
                break;
            }
            if (!m_queue.isEmpty()) {
                data = m_queue.dequeue();
            }
        }

        if (!data.isEmpty()) {
            qint64 written = file.write(data);
            if (written < 0) {
                file.close();
                emit finished(false, QString("写入文件失败"));
                return;
            }
            totalWritten += written;
            emit bytesWritten(written);
        }
    }

    file.close();
    emit finished(true, QString());
}

/* ========== tcpClient 实现 ========== */

tcpClient::tcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpClient)
    , m_downloadThread(nullptr)
{
    ui->setupUi(this);

    resize(480, 400);
    setWindowTitle(QStringLiteral("TinyDisk - 局域网云盘"));
    loadConfig();
    connect(&m_tcpSocket, SIGNAL(connected()), this, SLOT(showConnect()));
    connect(&m_tcpSocket, SIGNAL(readyRead()), this, SLOT(recvMsg()));
    //连接服务器
    m_tcpSocket.connectToHost(QHostAddress(m_strIP), m_usPort);
}

tcpClient::~tcpClient()
{
    stopDownload();
    delete ui;
}

void tcpClient::loadConfig()
{
    QFile file(":/client.config");
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray baData = file.readAll();
        file.close();
        QString strData = QString::fromUtf8(baData);
        QStringList strList = strData.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (strList.size() < 2) {
            QMessageBox::critical(this, "open config", "client config format error");
            return;
        }
        m_strIP = strList.at(0);
        m_usPort = strList.at(1).toUShort();
       // qDebug() << "IP:" << m_strIP << "port:" << m_usPort;
    }
    else
    {
        QMessageBox::critical(this, "open config", "open config failed");
    }
}

tcpClient &tcpClient::getInstance()
{
    static tcpClient instance;
    return instance;
}

QTcpSocket &tcpClient::gettcpSocket()
{
    return m_tcpSocket;
}

QString tcpClient::loginName()
{
    return m_strLoginName;
}

QString tcpClient::currentPath()
{
    return m_strCurPath;
}

void tcpClient::setCurrentPath(QString preContentPath)
{
    m_strCurPath = preContentPath;
}

void tcpClient::startDownload(const QString &filePath, qint64 offset)
{
    stopDownload();
    m_downloadThread = new DownloadThread(this);
    m_downloadThread->setup(filePath, offset);
    connect(m_downloadThread, &DownloadThread::bytesWritten,
            this, &tcpClient::onDownloadBytesWritten, Qt::QueuedConnection);
    connect(m_downloadThread, &DownloadThread::finished,
            this, &tcpClient::onDownloadFinished, Qt::QueuedConnection);
    connect(m_downloadThread, &QThread::finished,
            m_downloadThread, &QObject::deleteLater);
    m_downloadThread->start();
}

void tcpClient::stopDownload()
{
    if (m_downloadThread) {
        m_downloadThread->cancel();
        m_downloadThread->wait(3000);
        // deleteLater 已连接，线程结束后自动释放
        m_downloadThread = nullptr;
    }
}

void tcpClient::onDownloadBytesWritten(qint64 bytes)
{
    if (!m_downloadSession.addBytes(bytes)) {
        qDebug() << "下载失败：收到超出会话范围的数据块" << bytes;
        m_downloadSession.reset();
        QMessageBox::critical(this, "下载文件", "下载文件失败");
        stopDownload();
        return;
    }

    qDebug() << "下载写入数据块大小：" << bytes
             << "，已接收：" << m_downloadSession.receivedBytes()
             << "，总共：" << m_downloadSession.totalBytes();

    if (m_downloadSession.isComplete())
    {
        m_downloadSession.reset();
        QMessageBox::information(this, "下载文件", "下载文件成功");
        qDebug() << "下载完成";
        stopDownload();
    }
}

void tcpClient::onDownloadFinished(bool success, const QString &msg)
{
    if (!success && m_downloadSession.isActive()) {
        m_downloadSession.reset();
        QMessageBox::warning(this, "下载文件", msg);
        qDebug() << "下载线程异常结束：" << msg;
    }
    m_downloadThread = nullptr;
}


void tcpClient::showConnect()
{
    QMessageBox::information(this, "连接服务器", "连接服务器成功");
}

void tcpClient::handleDownloadData(const QByteArray &buffer)
{
    qint64 bytesRead = buffer.size();
    if (bytesRead > 0 && m_downloadThread && m_downloadSession.isActive()) {
        m_downloadThread->enqueueData(buffer);
    }
}

void tcpClient::recvMsg()
{
    m_packetCodec.append(m_tcpSocket.readAll());
    PDU *pdu = nullptr;
    while((pdu = m_packetCodec.takePacket()) != nullptr)
    {
        dispatchResponse(pdu);
        free(pdu);
        pdu = NULL;
    }
}

#if 0
void tcpClient::on_send_pb_clicked()
{
    QString strMsg = ui->lineEdit->text();
    if(!strMsg.isEmpty())
    {
        PDU *pdu = mkPDU(strMsg.size());
        pdu->uiMsgType = 8888;
        memcpy(pdu->caMsg, strMsg.toStdString().c_str(), strMsg.size());
        m_tcpSocket.write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
    else
    {
        QMessageBox::warning(this, "信息发送", "信息发送为空");
    }
}
#endif

void tcpClient::on_login_clicked()
{
    QString strName = ui->username_le->text();
    m_strLoginName = strName;
    QString strPwd = ui->pwd_le->text();
    if(!strName.isEmpty() && !strPwd.isEmpty())
    {
        PDU *pdu = mkPDU(0);
        pdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_REQUEST;
        PduFieldCodec::writeFixedPair(pdu->caData, strName, strPwd);
        m_tcpSocket.write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
    else
    {
        QMessageBox::critical(this, "登录", "登录失败!用户名或密码为空");
    }
}

void tcpClient::on_zhuce_clicked()
{
    QString strName = ui->username_le->text();
    QString strPwd = ui->pwd_le->text();
    if(!strName.isEmpty() && !strPwd.isEmpty())
    {
        PDU *pdu = mkPDU(0);
        pdu->uiMsgType = ENUM_MSG_TYPE_REGIST_REQUEST;
        PduFieldCodec::writeFixedPair(pdu->caData, strName, strPwd);
        m_tcpSocket.write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
    else
    {
        QMessageBox::critical(this, "注册", "注册失败!用户名或密码为空");
    }
}

void tcpClient::on_zhuxiao_clicked()
{

}
