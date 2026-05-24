#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QByteArray>
#include <QDebug>
#include <QMessageBox>
#include <QHostAddress>
#include "privatechat.h"

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
        QString strData = baData.toStdString().c_str();
        strData.replace("\r\n"," ");
        QStringList strList =  strData.split(" ");
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
    Book *pBook = OpeWidget::getInstance().getBook();
    pBook->m_iReceived += bytes;
    qDebug() << "下载写入数据块大小：" << bytes
             << "，已接收：" << pBook->m_iReceived
             << "，总共：" << pBook->m_iTotal;

    if (pBook->m_iTotal == pBook->m_iReceived)
    {
        pBook->m_iTotal = 0;
        pBook->m_iReceived = 0;
        pBook->setDownLoadStatus(false);
        QMessageBox::information(this, "下载文件", "下载文件成功");
        qDebug() << "下载完成";
        stopDownload();
    }
    else if (pBook->m_iTotal < pBook->m_iReceived)
    {
        qDebug() << "下载失败：总共的：" << pBook->m_iTotal << " 下载的" << pBook->m_iReceived;
        pBook->m_iTotal = 0;
        pBook->m_iReceived = 0;
        pBook->setDownLoadStatus(false);
        QMessageBox::critical(this, "下载文件", "下载文件失败");
        stopDownload();
    }
}

void tcpClient::onDownloadFinished(bool success, const QString &msg)
{
    if (!success) {
        Book *pBook = OpeWidget::getInstance().getBook();
        pBook->m_iTotal = 0;
        pBook->m_iReceived = 0;
        pBook->setDownLoadStatus(false);
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
    if (bytesRead > 0 && m_downloadThread) {
        m_downloadThread->enqueueData(buffer);
    }
}

void tcpClient::recvMsg()
{
    if(!OpeWidget::getInstance().getBook()->getDownloadStatus())
    {
        m_packetCodec.append(m_tcpSocket.readAll());
        PDU *pdu = nullptr;
        while(!OpeWidget::getInstance().getBook()->getDownloadStatus()
              && (pdu = m_packetCodec.takePacket()) != nullptr)
        {
    //    qDebug() << pdu->uiMsgType << (char*)pdu->caMsg;
        switch (pdu->uiMsgType)
        {
        case ENUM_MSG_TYPE_REGIST_RESPOND:
        {
            if(0 == strcmp(pdu->caData, REGIST_OK))
            {
                QMessageBox::information(this, "注册", REGIST_OK);
            }
            else if(0 == strcmp(pdu -> caData, REGIST_FAILED))
            {
                QMessageBox::warning(this, "注册", REGIST_FAILED);
            }
            break;
        }
        case ENUM_MSG_TYPE_LOGIN_RESPOND:
        {
            if(0 == strcmp(pdu->caData, LOGIN_OK))
            {
                m_strCurPath = QString("./%1").arg(m_strLoginName);
                QMessageBox::information(this, "登录", LOGIN_OK);
                OpeWidget::getInstance().show();
                this->hide();
            }
            else if(0 == strcmp(pdu -> caData, LOGIN_FAILED))
            {
                QMessageBox::warning(this, "登录", LOGIN_FAILED);
            }
            break;
        }
        case ENUM_MSG_TYPE_ALL_ONLINE_RESPOND:
        {
            OpeWidget::getInstance().getFriend()->showAllOnlineUser(pdu);
            break;
        }
        case ENUM_MSG_TYPE_SEARCH_USER_RESPOND:
        {

           if(0 == strcmp(SEARCH_USER_NO, pdu->caData)){
               QMessageBox::information(this, "搜索", QString("%1: not exist").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
            }else if(0 == strcmp(SEARCH_USER_ONLINE, pdu->caData)){
               QMessageBox::information(this, "搜索", QString("%1: online").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
           }else if(0 == strcmp(SEARCH_USER_OFFLINE, pdu->caData)){
               QMessageBox::information(this, "搜索", QString("%1: offline").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
           }
           break;
        }
        case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
        {
            char caName[32] = {'\0'};
            strncpy(caName, pdu->caData + 32, 32);
            int ret = QMessageBox::information(this, "加好友", QString("%1 want to be your friend?").arg(caName),
                                     QMessageBox::Yes, QMessageBox::No);
            PDU *respdu = mkPDU(0);
            strncpy(respdu->caData, pdu->caData, 32); // 被加好友者用户名
            strncpy(respdu->caData + 32, pdu->caData + 32, 32); // 加好友者用户名
            if(QMessageBox::Yes == ret){
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE;
            }
            else
            {
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;
            }
            m_tcpSocket.write((char*)respdu, respdu->uiPDULen);
            free(respdu);
            respdu = NULL;
            break;
        }
        case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND:
        {
            QMessageBox::information(this, "添加好友", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_ADD_FRIEND_AGREE: // 对方同意加好友
        {
            QMessageBox::information(this, "添加好友", QString("%1 已同意您的好友申请！").arg(pdu->caData));
            break;
        }
        case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE: // 对方拒绝加好友
        {
            QMessageBox::information(this, "添加好友", QString("%1 已拒绝您的好友申请！").arg(pdu->caData));
            break;
        }
        case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:
        {

            OpeWidget::getInstance().getFriend()->updateFriendList(pdu);
            break;
        }
        case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:
        {
            if(0 == strcmp(pdu->caData, DELETE_OK))
            {
                QMessageBox::information(this, "删除好友", pdu->caData);
            }
            else if(0 == strcmp(pdu -> caData, DELETE_FAILED))
            {
                QMessageBox::warning(this, "删除好友", pdu->caData);
            }
            break;
        }
        case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST: // 处理服务器转发过来的删除好友请求
        {
            char sourceName[32] = {'\0'}; // 获取发送方用户名
            strncpy(sourceName, pdu->caData, 32);
            QMessageBox::information(this, "删除好友", QString("%1 已解除与您的好友关系！").arg(sourceName));
            break;
        }
        case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST : // 处理服务器转发过来的私聊请求
        {
            if(PrivateChat::getInstance().isHidden())
            {
                PrivateChat::getInstance().show();
            }
            char sourceName[32] = {'\0'}; // 获取发送方用户名
            char sendName[32] = {'\0'};
            strncpy(sourceName, pdu->caData, 32);
            strncpy(sendName, pdu->caData + 32, 32);
            QString ownName = sourceName;
            PrivateChat::getInstance().setChatName(ownName);
            PrivateChat::getInstance().updateMsg(pdu);
            break;
        }
        case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST : // 处理服务器转发过来的群发信息请求
        {
            OpeWidget::getInstance().getFriend()->updateGroupMsg(pdu);
            break;
        }
        case ENUM_MSG_TYPE_CREATE_DIR_RESPOND :
        {
            QMessageBox::information(this, "创建文件夹", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND :
        {

            OpeWidget::getInstance().getBook()->updateFileList(pdu);
            QString strEnterDir = OpeWidget::getInstance().getBook()->enterDir();

            if(!strEnterDir.isEmpty())
            {

                m_strCurPath = m_strCurPath + "/" + strEnterDir;
                qDebug() << "进入的文件夹路径:" << m_strCurPath << "文件夹名字：" << strEnterDir;

            }
            break;
        }
        case ENUM_MSG_TYPE_DELETE_DIR_RESPOND :
        {
            QMessageBox::information(this, "删除文件夹", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_RENAME_FILE_RESPOND :
        {
            QMessageBox::information(this, "重命名文件", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_ENTER_DIR_RESPOND :
        {
            OpeWidget::getInstance().getBook()->clearEnterDir();
            QMessageBox::information(this, "进入文件夹", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND :
        {
            QMessageBox::information(this, "上传文件", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_DELETE_FILE_RESPOND :
        {
            QMessageBox::information(this, "删除文件", pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND :
        {
            qDebug() << "pdu->caData:" <<  pdu->caData;
            char caFileName[32] = {'\0'};
            qint64 fileSize = 0;
            qint64 skipSize = 0;
            // 解析文件名、文件总大小、已跳过大小
            sscanf(pdu->caData, "%s %lld %lld", caFileName, &fileSize, &skipSize);
            Book *pBook = OpeWidget::getInstance().getBook();
            pBook->m_iTotal = fileSize;
            pBook->m_iReceived = skipSize;
            qDebug() << "respond:" << caFileName << " 总大小:" << fileSize << " 跳过:" << skipSize;
            if(strlen(caFileName) > 0 && fileSize > 0)
            {
                pBook->setDownLoadStatus(true);
                QString strFileSavePath = pBook->getFileSavePath();
                // 启动下载线程，在子线程中打开文件并写入
                startDownload(strFileSavePath, skipSize);
            }
            break;
        }
        case ENUM_MSG_TYPE_SHARE_FILE_RESPOND :
        {
             QMessageBox::information(this, "共享文件", pdu->caData);
             break;
        }
        case ENUM_MSG_TYPE_SHARE_FILE_NOTE_REQUEST :
        {
             char *pPath = new char[pdu->uiMsgLen];
             memcpy(pPath, pdu->caMsg, pdu->uiMsgLen);
             qDebug() << "pPath:" << pPath;
             char *pos = strrchr(pPath, '/');
             if(NULL != pos)
             {
                 pos++;
                 qDebug() << "pos:" << pos;
                 QString strNote = QString("%1 share file->%2 \n Do you accept?").arg(pdu->caData).arg(pos);
                 qDebug() << "strNote:" << strNote;
                 int ret = QMessageBox::question(this, "共享文件", strNote);
                 if(QMessageBox::Yes == ret)
                 {
                     PDU *respdu = mkPDU(pdu->uiMsgLen);
                     respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPOND;
                     memcpy(respdu->caMsg, pdu->caMsg, pdu->uiMsgLen);
                     QString strName = tcpClient::getInstance().loginName();
                     qDebug() << "被分享者：" << strName;
                     strcpy(respdu->caData, strName.toStdString().c_str());
                     m_tcpSocket.write((char*)respdu, respdu->uiPDULen);
                     free(respdu);
                     respdu = NULL;
                 }
             }
             else
             {
                 qDebug() << "没有pos";
             }
             delete[] pPath;
             break;
        }
        case ENUM_MSG_TYPE_MOVE_FILE_RESPOND :
        {
             QMessageBox::information(this, "移动文件", pdu->caData);
             break;
        }
        default:
            break;
        }
        free(pdu);
        pdu = NULL;
        }

        if(OpeWidget::getInstance().getBook()->getDownloadStatus())
        {
            handleDownloadData(m_packetCodec.takeBufferedData());
        }
    }
    else
    {
        // 下载模式：从 socket 读取裸数据，交给 DownloadThread 写入磁盘
        handleDownloadData(m_tcpSocket.readAll());
        // 注意：进度更新移至 onDownloadBytesWritten 回调
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
        strncpy(pdu->caData, strName.toStdString().c_str(), 32);
        strncpy(pdu->caData + 32, strPwd.toStdString().c_str(), 32);
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
        strncpy(pdu->caData, strName.toStdString().c_str(), 32);
        strncpy(pdu->caData + 32, strPwd.toStdString().c_str(), 32);
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
