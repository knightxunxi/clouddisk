#include "mytcpsocket.h"
#include <QDebug>
#include <mytcpserver.h>
#include <QDir>
#include <QFileInfo>

// ═══════════════════════════════════════════════
//  FileWorker 实现
// ═══════════════════════════════════════════════

void FileWorker::run()
{
    m_canceled = false;
    switch (m_task)
    {
    case TASK_SEND_FILE:  runSendFile();  break;
    case TASK_COPY_FILE:  runCopyFile();  break;
    case TASK_COPY_DIR:   runCopyDir(m_srcPath, m_destPath); break;
    case TASK_DELETE_DIR: runDeleteDir(); break;
    }
}

// ── 下载：分块读取文件，每块通过信号传回主线程写 socket ──────────
void FileWorker::runSendFile()
{
    QFile file(m_srcPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[FileWorker] 打开文件失败：" << m_srcPath;
        emit taskFinished(false);
        return;
    }
    if (m_skipBytes > 0 && !file.seek(m_skipBytes)) {
        qDebug() << "[FileWorker] seek 失败，偏移：" << m_skipBytes;
        file.close();
        emit taskFinished(false);
        return;
    }

    const int BUF = 4096;
    char buf[BUF];
    qint64 ret = 0;
    while (!m_canceled && (ret = file.read(buf, BUF)) > 0) {
        emit dataBlock(QByteArray(buf, static_cast<int>(ret)));
        // 让出 CPU，避免发送过快撑爆发送缓冲区
        msleep(1);
    }
    file.close();
    emit taskFinished(!m_canceled && ret == 0);
}

// ── 文件复制 ──────────────────────────────────────────────────────
void FileWorker::runCopyFile()
{
    bool ok = QFile::copy(m_srcPath, m_destPath);
    qDebug() << "[FileWorker] 复制文件" << m_srcPath << "->" << m_destPath
             << (ok ? "成功" : "失败");
    emit taskFinished(ok);
}

// ── 目录递归复制（修复原始 += Bug，改用 =）─────────────────────────
void FileWorker::runCopyDir(const QString &src, const QString &dest)
{
    if (m_canceled) return;
    QDir dir;
    dir.mkdir(dest);
    dir.setPath(src);
    QFileInfoList fileInfoList = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (int i = 0; i < fileInfoList.size() && !m_canceled; ++i) {
        const QFileInfo &fi = fileInfoList[i];
        // 用 = 而非 +=，修复原有路径累加 Bug
        QString srcTmp  = src  + '/' + fi.fileName();
        QString destTmp = dest + '/' + fi.fileName();
        if (fi.isFile()) {
            QFile::copy(srcTmp, destTmp);
        } else if (fi.isDir()) {
            runCopyDir(srcTmp, destTmp);  // 递归
        }
    }
}

// ── 目录递归删除 ──────────────────────────────────────────────────
void FileWorker::runDeleteDir()
{
    QDir dir(m_srcPath);
    bool ok = dir.removeRecursively();
    qDebug() << "[FileWorker] 删除目录" << m_srcPath << (ok ? "成功" : "失败");
    emit taskFinished(ok);
}

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

void MyTcpSocket::recvMsg()
{
    if (!m_bUpload)
    {
        qDebug() << this->bytesAvailable();
        unit uiPDULen = 0;
        this->read((char*)&uiPDULen, sizeof(unit));
        unit uiMsgLen = uiPDULen - sizeof(PDU);
        PDU *pdu = mkPDU(uiMsgLen);
        this->read((char*)pdu + sizeof(unit), uiPDULen - sizeof(unit));

        switch (pdu->uiMsgType)
        {
        // ── 注册 ───────────────────────────────────────────────
        case ENUM_MSG_TYPE_REGIST_REQUEST:
        {
            char caName[32] = {'\0'};
            char caPwd[32]  = {'\0'};
            strncpy(caName, pdu->caData,      32);
            strncpy(caPwd,  pdu->caData + 32, 32);
            bool ret = OpeDB::getInstance().handleRegist(caName, caPwd);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;
            if (ret) {
                strcpy(respdu->caData, REGIST_OK);
                QDir dir;
                dir.mkdir(QString("./%1").arg(caName));
            } else {
                strcpy(respdu->caData, REGIST_FAILED);
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 登录 ───────────────────────────────────────────────
        case ENUM_MSG_TYPE_LOGIN_REQUEST:
        {
            char caName[32] = {'\0'};
            char caPwd[32]  = {'\0'};
            strncpy(caName, pdu->caData,      32);
            strncpy(caPwd,  pdu->caData + 32, 32);
            bool ret = OpeDB::getInstance().handleLogin(caName, caPwd);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;
            if (ret) {
                strcpy(respdu->caData, LOGIN_OK);
                m_strName = caName;
                emit userLoggedIn();
            } else {
                strcpy(respdu->caData, LOGIN_FAILED);
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 所有在线用户 ───────────────────────────────────────
        case ENUM_MSG_TYPE_ALL_ONLINE_REQUEST:
        {
            QStringList ret = OpeDB::getInstance().handleAllOnline();
            unit uiMsgLenLocal = ret.size() * 32;
            PDU *respdu = mkPDU(uiMsgLenLocal);
            respdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_RESPOND;
            for (int i = 0; i < ret.size(); i++) {
                memcpy((char*)(respdu->caMsg) + i * 32,
                       ret.at(i).toStdString().c_str(), ret.at(i).size());
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 搜索用户 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_SEARCH_USER_REQUEST:
        {
            int ret = OpeDB::getInstance().handleSearchUser(pdu->caData);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USER_RESPOND;
            if      (-1 == ret) strcpy(respdu->caData, SEARCH_USER_NO);
            else if ( 1 == ret) strcpy(respdu->caData, SEARCH_USER_ONLINE);
            else if ( 0 == ret) strcpy(respdu->caData, SEARCH_USER_OFFLINE);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 添加好友 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
        {
            char caPerName[32] = {'\0'};
            char caName[32]    = {'\0'};
            strncpy(caPerName, pdu->caData,      32);
            strncpy(caName,    pdu->caData + 32, 32);
            int ret = OpeDB::getInstance().handleAddFriend(caPerName, caName);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
            if (-1 == ret) {
                strcpy(respdu->caData, UNKNOWN_ERROR);
                write((char*)respdu, respdu->uiPDULen);
            } else if (0 == ret) {
                strcpy(respdu->caData, EXISTED_FRIEND);
                write((char*)respdu, respdu->uiPDULen);
            } else if (1 == ret) {
                MyTcpServer::getInstance().resend(caPerName, pdu);
                strcpy(respdu->caData, ADD_FRIEND_OK);
                write((char*)respdu, respdu->uiPDULen);
            } else if (2 == ret) {
                strcpy(respdu->caData, ADD_FRIEND_OFFLINE);
                write((char*)respdu, respdu->uiPDULen);
            } else if (3 == ret) {
                strcpy(respdu->caData, ADD_FRIEND_NO_EXISTED);
                write((char*)respdu, respdu->uiPDULen);
            }
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 同意好友 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_ADD_FRIEND_AGREE:
        {
            char addedName[32]  = {'\0'};
            char sourceName[32] = {'\0'};
            strncpy(addedName,  pdu->caData,      32);
            strncpy(sourceName, pdu->caData + 32, 32);
            OpeDB::getInstance().handleAddFriendAgree(addedName, sourceName);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE;
            MyTcpServer::getInstance().resend(sourceName, pdu);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 拒绝好友 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:
        {
            char sourceName[32] = {'\0'};
            strncpy(sourceName, pdu->caData + 32, 32);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;
            MyTcpServer::getInstance().resend(sourceName, pdu);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 刷新好友 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:
        {
            char sourceName[32] = {'\0'};
            strncpy(sourceName, pdu->caData, 32);
            QStringList strList = OpeDB::getInstance().handleFlushFriend(sourceName);
            unit uiMsgLenLocal = strList.size() / 2 * 36;
            PDU *respdu = mkPDU(uiMsgLenLocal);
            respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
            for (int i = 0; i * 2 < strList.size(); i++) {
                memcpy((char*)(respdu->caMsg) + 36 * i,
                       strList.at(i*2).toStdString().c_str(), 32);
                memcpy((char*)(respdu->caMsg) + 36 * i + 32,
                       strList.at(i*2+1).toStdString().c_str(), 4);
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 删除好友 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
        {
            char sourceName[32] = {'\0'};
            char deleteName[32] = {'\0'};
            strncpy(sourceName, pdu->caData,      32);
            strncpy(deleteName, pdu->caData + 32, 32);
            bool ret = OpeDB::getInstance().handleDeleteFriend(sourceName, deleteName);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
            strcpy(respdu->caData, ret ? DELETE_OK : DELETE_FAILED);
            MyTcpServer::getInstance().resend(deleteName, pdu);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 私聊 ───────────────────────────────────────────────
        case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:
        {
            char sourceName[32] = {'\0'};
            char chatName[32]   = {'\0'};
            strncpy(sourceName, pdu->caData,      32);
            strncpy(chatName,   pdu->caData + 32, 32);
            MyTcpServer::getInstance().resend(chatName,   pdu);
            MyTcpServer::getInstance().resend(sourceName, pdu);
            break;
        }
        // ── 群聊 ───────────────────────────────────────────────
        case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:
        {
            char sourceName[32] = {'\0'};
            strncpy(sourceName, pdu->caData, 32);
            QStringList onlineFriend = OpeDB::getInstance().handleGroupChat(sourceName);
            for (int i = 0; i * 2 < onlineFriend.size(); i++) {
                QString tmp = onlineFriend.at(i * 2);
                MyTcpServer::getInstance().resend(tmp.toStdString().c_str(), pdu);
            }
            break;
        }
        // ── 创建目录 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_CREATE_DIR_REQUEST:
        {
            QDir dir;
            QString strCurPath = QString("%1").arg((char*)pdu->caMsg);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;
            if (dir.exists(strCurPath)) {
                char strNewPath[32] = {'\0'};
                memcpy(strNewPath, pdu->caData + 32, 32);
                QString newPath = strCurPath + "/" + strNewPath;
                if (dir.exists(newPath)) {
                    strcpy(respdu->caData, DIR_ALREADY_EXSIT);
                } else {
                    strcpy(respdu->caData, CREATE_DIR_OK);
                    dir.mkdir(newPath);
                }
            } else {
                strcpy(respdu->caData, DIR_N0_EXSIT);
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 刷新文件列表 ───────────────────────────────────────
        case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:
        {
            char *pCurPath = new char[pdu->uiMsgLen];
            memcpy(pCurPath, pdu->caMsg, pdu->uiMsgLen);
            QDir dir(pCurPath);
            delete[] pCurPath; pCurPath = nullptr;
            QFileInfoList fileList = dir.entryInfoList();
            int fileCount = fileList.size();
            PDU *respdu = mkPDU(sizeof(FileInfo) * fileCount);
            respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
            for (int i = 0; i < fileCount; i++) {
                FileInfo *pFileInfo = (FileInfo*)(respdu->caMsg) + i;
                QString strFileName = fileList[i].fileName();
                memcpy(pFileInfo->caFileName,
                       strFileName.toStdString().c_str(), strFileName.size());
                pFileInfo->iFileType = fileList[i].isDir() ? 0 : 1;
            }
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 删除目录 — 移入子线程 ─────────────────────────────
        case ENUM_MSG_TYPE_DELETE_DIR_REQUEST:
        {
            char strDirName[32] = {'\0'};
            strcpy(strDirName, pdu->caData);
            char *pDirPath = new char[pdu->uiMsgLen];
            memcpy(pDirPath, pdu->caMsg, pdu->uiMsgLen);
            QString strPath = QString("%1/%2").arg(pDirPath).arg(strDirName);
            delete[] pDirPath; pDirPath = nullptr;

            QFileInfo fileInfo(strPath);
            if (fileInfo.isDir()) {
                // 异步删除，完成后通过信号回调（此处不再等待）
                FileWorker *worker = new FileWorker(this);
                worker->setupDeleteDir(strPath);
                // 删除完成后发响应（通过 lambda / 信号）
                QString capturedPath = strPath;
                connect(worker, &FileWorker::taskFinished,
                        this, [this, capturedPath](bool ok) {
                    PDU *respdu = mkPDU(0);
                    respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_DIR_RESPOND;
                    memcpy(respdu->caData,
                           ok ? DIR_DELETE_OK : DIR_DELETE_FAILED,
                           ok ? strlen(DIR_DELETE_OK) : strlen(DIR_DELETE_FAILED));
                    write((char*)respdu, respdu->uiPDULen);
                    free(respdu); respdu = nullptr;
                });
                connect(worker, &FileWorker::finished, worker, &QObject::deleteLater);
                worker->start();
            } else {
                // 不是目录（是文件或不存在），直接回复失败
                PDU *respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_DIR_RESPOND;
                memcpy(respdu->caData, DIR_DELETE_FAILED, strlen(DIR_DELETE_FAILED));
                write((char*)respdu, respdu->uiPDULen);
                free(respdu); respdu = nullptr;
            }
            break;
        }
        // ── 重命名 ─────────────────────────────────────────────
        case ENUM_MSG_TYPE_RENAME_FILE_REQUEST:
        {
            char oldFileName[32] = {'\0'};
            char newFileName[32] = {'\0'};
            strncpy(oldFileName, pdu->caData,      32);
            strncpy(newFileName, pdu->caData + 32, 32);
            char *pPath = new char[pdu->uiMsgLen];
            memcpy(pPath, pdu->caMsg, pdu->uiMsgLen);
            QString strOldPath = QString("%1/%2").arg(pPath).arg(oldFileName);
            QString strNewPath = QString("%1/%2").arg(pPath).arg(newFileName);
            delete[] pPath; pPath = nullptr;
            QDir dir;
            bool ret = dir.rename(strOldPath, strNewPath);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_RENAME_FILE_RESPOND;
            memcpy(respdu->caData,
                   ret ? RENAME_FILE_OK : RENAME_FILE_FAILED,
                   ret ? strlen(RENAME_FILE_OK) : strlen(RENAME_FILE_FAILED));
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 进入目录 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_ENTER_DIR_REQUEST:
        {
            char caEnterDirName[32] = {'\0'};
            strncpy(caEnterDirName, pdu->caData, 32);
            char *pPath = new char[pdu->uiMsgLen];
            memcpy(pPath, pdu->caMsg, pdu->uiMsgLen);
            QString strNewPath = QString("%1/%2").arg(pPath).arg(caEnterDirName);
            delete[] pPath; pPath = nullptr;
            QFileInfo fileInfo(strNewPath);
            if (fileInfo.isDir()) {
                QDir dir(strNewPath);
                QFileInfoList fileList = dir.entryInfoList();
                int fileCount = fileList.size();
                PDU *respdu = mkPDU(sizeof(FileInfo) * fileCount);
                respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
                for (int i = 0; i < fileCount; i++) {
                    FileInfo *pFileInfo = (FileInfo*)(respdu->caMsg) + i;
                    QString strFileName = fileList[i].fileName();
                    memcpy(pFileInfo->caFileName,
                           strFileName.toStdString().c_str(), strFileName.size());
                    pFileInfo->iFileType = fileList[i].isDir() ? 0 : 1;
                }
                write((char*)respdu, respdu->uiPDULen);
                free(respdu); respdu = nullptr;
            } else {
                PDU *respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_ENTER_DIR_RESPOND;
                memcpy(respdu->caData, ENTER_DIR_FAILED, strlen(ENTER_DIR_FAILED));
                write((char*)respdu, respdu->uiPDULen);
                free(respdu); respdu = nullptr;
            }
            break;
        }
        // ── 上传文件请求（仅打开文件，切换到上传接收模式）────────
        case ENUM_MSG_TYPE_UPLOAD_FILE_REQUEST:
        {
            char uploadFileName[32] = {'\0'};
            qint64 uploadFileSize = 0;
            qint64 uploadedSize   = 0;
            sscanf(pdu->caData, "%s %lld %lld",
                   uploadFileName, &uploadFileSize, &uploadedSize);
            char *pPath = new char[pdu->uiMsgLen];
            memcpy(pPath, pdu->caMsg, pdu->uiMsgLen);
            QString strNewPath = QString("%1/%2").arg(pPath).arg(uploadFileName);
            delete[] pPath; pPath = nullptr;

            m_file.setFileName(strNewPath);
            bool openSuccess = false;
            if (uploadedSize == 0) {
                if (m_file.open(QIODevice::WriteOnly)) {
                    m_bUpload   = true;
                    m_iTotal    = uploadFileSize;
                    m_iReceived = 0;
                    openSuccess = true;
                }
            } else {
                if (m_file.exists() && m_file.size() == uploadedSize) {
                    if (m_file.open(QIODevice::ReadWrite | QIODevice::Append)) {
                        m_bUpload   = true;
                        m_iTotal    = uploadFileSize;
                        m_iReceived = uploadedSize;
                        openSuccess = true;
                    }
                }
            }
            if (!openSuccess) {
                PDU *respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND;
                strcpy(respdu->caData, UPLOAD_FILE_FAILED);
                write((char*)respdu, respdu->uiPDULen);
                free(respdu); respdu = nullptr;
            }
            break;
        }
        // ── 删除文件 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_DELETE_FILE_REQUEST:
        {
            char strFileName[32] = {'\0'};
            strcpy(strFileName, pdu->caData);
            char *pDirPath = new char[pdu->uiMsgLen];
            memcpy(pDirPath, pdu->caMsg, pdu->uiMsgLen);
            QString strPath = QString("%1/%2").arg(pDirPath).arg(strFileName);
            delete[] pDirPath; pDirPath = nullptr;
            QFileInfo fileInfo(strPath);
            bool ret = false;
            if (fileInfo.isFile()) {
                QDir dir;
                ret = dir.remove(strPath);
            }
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FILE_RESPOND;
            memcpy(respdu->caData,
                   ret ? FILE_DELETE_OK : FILE_DELETE_FAILED,
                   ret ? strlen(FILE_DELETE_OK) : strlen(FILE_DELETE_FAILED));
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 下载文件请求 — 用 FileWorker 分块异步发送 ────────────
        case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST:
        {
            char caFileName[32] = {'\0'};
            qint64 downloadedSize = 0;
            sscanf(pdu->caData, "%s %lld", caFileName, &downloadedSize);
            char *pPath = new char[pdu->uiMsgLen];
            memcpy(pPath, pdu->caMsg, pdu->uiMsgLen);
            QString strNewPath = QString("%1/%2").arg(pPath).arg(caFileName);
            delete[] pPath; pPath = nullptr;

            QFileInfo fileInfo(strNewPath);
            qint64 fileSize = fileInfo.size();
            qint64 skipSize = 0;
            if (downloadedSize > 0 && downloadedSize < fileSize)
                skipSize = downloadedSize;
            else if (downloadedSize >= fileSize)
                skipSize = fileSize;

            // 先发响应头
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND;
            sprintf(respdu->caData, "%s %lld %lld", caFileName, fileSize, skipSize);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;

            // 若还有数据需要传，异步发送
            if (skipSize < fileSize) {
                FileWorker *worker = new FileWorker(this);
                worker->setupSendFile(strNewPath, skipSize);
                startFileWorker(worker);
            }
            break;
        }
        // ── 分享文件请求（通知被分享者）──────────────────────────
        case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
        {
            char strSendName[32] = {'\0'};
            int shareNum = 0;
            sscanf(pdu->caData, "%s %d", strSendName, &shareNum);
            int size = shareNum * 32;
            PDU *respdu = mkPDU(pdu->uiMsgLen - size);
            respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_NOTE_REQUEST;
            strcpy(respdu->caData, strSendName);
            memcpy(respdu->caMsg, (char*)(pdu->caMsg) + size, pdu->uiMsgLen - size);
            char caReceiveName[32] = {'\0'};
            for (int i = 0; i < shareNum; i++) {
                memcpy(caReceiveName, (char*)(pdu->caMsg) + i * 32, 32);
                MyTcpServer::getInstance().resend(caReceiveName, respdu);
            }
            free(respdu); respdu = nullptr;
            respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_RESPOND;
            strcpy(respdu->caData, SHARE_FILE_OK);
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        // ── 被分享者接受分享 — 异步复制 ───────────────────────────
        case ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPOND:
        {
            QString strReceivePath = QString("./%1").arg(pdu->caData);
            QString strSharePath   = QString("%1").arg((char*)(pdu->caMsg));
            int index = strSharePath.lastIndexOf('/');
            QString strFileName = strSharePath.right(strSharePath.size() - index - 1);
            strReceivePath = strReceivePath + '/' + strFileName;

            QFileInfo fileInfo(strSharePath);
            FileWorker *worker = new FileWorker(this);
            if (fileInfo.isDir()) {
                worker->setupCopyDir(strSharePath, strReceivePath);
            } else {
                worker->setupCopyFile(strSharePath, strReceivePath);
            }
            connect(worker, &FileWorker::finished, worker, &QObject::deleteLater);
            worker->start();
            break;
        }
        // ── 移动文件 ───────────────────────────────────────────
        case ENUM_MSG_TYPE_MOVE_FILE_REQUEST:
        {
            char caFileName[32] = {'\0'};
            int srcLen = 0, destLen = 0;
            sscanf(pdu->caData, "%d%d%s", &srcLen, &destLen, caFileName);
            char *pSrcPath  = new char[srcLen + 1]();
            char *pDestPath = new char[destLen + 1 + 32]();
            memcpy(pSrcPath,  pdu->caMsg,                        srcLen);
            memcpy(pDestPath, (char*)(pdu->caMsg) + (srcLen + 1), destLen);
            QFileInfo fileInfo(pDestPath);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_MOVE_FILE_RESPOND;
            if (fileInfo.isDir()) {
                strcat(pDestPath, "/");
                strcat(pDestPath, caFileName);
                bool ret = QFile::rename(pSrcPath, pDestPath);
                strcpy(respdu->caData, ret ? MOVE_FILE_OK : COMMON_ERROR);
            } else {
                strcpy(respdu->caData, MOVE_FILE_FAILED);
            }
            delete[] pSrcPath;  pSrcPath  = nullptr;
            delete[] pDestPath; pDestPath = nullptr;
            write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
            break;
        }
        default:
            break;
        } // end switch

        free(pdu); pdu = nullptr;
    }
    else
    {
        // ── 上传数据接收（文件裸流模式）──────────────────────────
        // 直接在主线程写文件（单次 readAll < 64KB，几乎不卡）
        PDU *respdu = mkPDU(0);
        respdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND;
        QByteArray buffer = readAll();
        qint64 bytesRead  = buffer.size();
        if (bytesRead > 0) {
            m_file.write(buffer);
            m_iReceived += bytesRead;
        }
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
