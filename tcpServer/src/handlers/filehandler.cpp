#include "mytcpsocket.h"
#include "mytcpserver.h"
#include "fileworker.h"
#include "storageservice.h"

#include <QByteArray>
#include <QFileInfo>

#include <cstdio>
#include <cstring>

namespace {

QString fixedString(const char *data, int length)
{
    QByteArray bytes(data, length);
    const int terminator = bytes.indexOf('\0');
    if (terminator >= 0) {
        bytes.truncate(terminator);
    }
    return QString::fromUtf8(bytes).trimmed();
}

void sendSimpleResponse(MyTcpSocket *socket, unit msgType, const char *message)
{
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = msgType;
    strcpy(respdu->caData, message);
    socket->write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void sendFileListResponse(MyTcpSocket *socket, const QFileInfoList &fileList)
{
    const int fileCount = fileList.size();
    PDU *respdu = mkPDU(sizeof(FileInfo) * fileCount);
    respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
    for (int i = 0; i < fileCount; i++) {
        FileInfo *pFileInfo = (FileInfo*)(respdu->caMsg) + i;
        const QByteArray fileName = fileList[i].fileName().toUtf8();
        memcpy(pFileInfo->caFileName,
               fileName.constData(),
               qMin(fileName.size(), static_cast<int>(sizeof(pFileInfo->caFileName) - 1)));
        pFileInfo->iFileType = fileList[i].isDir() ? 0 : 1;
    }

    socket->write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

} // namespace

void MyTcpSocket::handleCreateDirRequest(PDU *pdu)
{
    QString strCurPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strCurPath;
    const QString strNewPath = fixedString(pdu->caData + 32, 32);

    StorageService::CreateDirResult result = StorageService::ParentDirNotExist;
    if (StorageService::resolveUserPath(m_strName, strCurPathRaw, &strCurPath)
            && StorageService::isSafeName(strNewPath)) {
        result = StorageService::createDir(strCurPath, strNewPath);
    }

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;
    if (result == StorageService::CreateDirOk) {
        strcpy(respdu->caData, CREATE_DIR_OK);
    } else if (result == StorageService::TargetDirAlreadyExist) {
        strcpy(respdu->caData, DIR_ALREADY_EXSIT);
    } else {
        strcpy(respdu->caData, DIR_N0_EXSIT);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleFlushFileRequest(PDU *pdu)
{
    QString strCurPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strCurPath;
    QFileInfoList fileList;
    if (StorageService::resolveUserPath(m_strName, strCurPathRaw, &strCurPath)) {
        fileList = StorageService::listDir(strCurPath);
    }

    sendFileListResponse(this, fileList);
}

void MyTcpSocket::handleDeleteDirRequest(PDU *pdu)
{
    const QString strDirName = fixedString(pdu->caData, 32);
    QString strDirPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strPath;

    if (StorageService::resolveUserChildPath(m_strName, strDirPathRaw,
                                             strDirName, &strPath)
            && StorageService::isDir(strPath)) {
        FileWorker *worker = new FileWorker(this);
        worker->setupDeleteDir(strPath);
        connect(worker, &FileWorker::taskFinished,
                this, [this](bool ok) {
            sendSimpleResponse(this,
                               ENUM_MSG_TYPE_DELETE_DIR_RESPOND,
                               ok ? DIR_DELETE_OK : DIR_DELETE_FAILED);
        });
        connect(worker, &FileWorker::finished, worker, &QObject::deleteLater);
        worker->start();
    } else {
        sendSimpleResponse(this, ENUM_MSG_TYPE_DELETE_DIR_RESPOND, DIR_DELETE_FAILED);
    }
}

void MyTcpSocket::handleRenameFileRequest(PDU *pdu)
{
    const QString oldFileName = fixedString(pdu->caData, 32);
    const QString newFileName = fixedString(pdu->caData + 32, 32);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strOldPath;
    QString strNewPath;

    bool ret = StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                                    oldFileName, &strOldPath)
            && StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                                    newFileName, &strNewPath)
            && StorageService::renamePath(strOldPath, strNewPath);
    sendSimpleResponse(this,
                       ENUM_MSG_TYPE_RENAME_FILE_RESPOND,
                       ret ? RENAME_FILE_OK : RENAME_FILE_FAILED);
}

void MyTcpSocket::handleEnterDirRequest(PDU *pdu)
{
    const QString caEnterDirName = fixedString(pdu->caData, 32);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath;

    if (StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                             caEnterDirName, &strNewPath)
            && StorageService::isDir(strNewPath)) {
        QFileInfoList fileList = StorageService::listDir(strNewPath);
        sendFileListResponse(this, fileList);
    } else {
        sendSimpleResponse(this, ENUM_MSG_TYPE_ENTER_DIR_RESPOND, ENTER_DIR_FAILED);
    }
}

void MyTcpSocket::handleUploadFileRequest(PDU *pdu)
{
    char uploadFileName[32] = {'\0'};
    qint64 uploadFileSize = 0;
    qint64 uploadedSize   = 0;
    sscanf(pdu->caData, "%31s %lld %lld",
           uploadFileName, &uploadFileSize, &uploadedSize);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath;

    bool openSuccess = false;
    if (StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                             QString::fromUtf8(uploadFileName),
                                             &strNewPath)
            && uploadFileSize >= 0
            && uploadedSize >= 0
            && uploadedSize <= uploadFileSize) {
        m_file.setFileName(strNewPath);
        if (uploadedSize == 0) {
            if (m_file.open(QIODevice::WriteOnly)) {
                m_bUpload   = true;
                m_iTotal    = uploadFileSize;
                m_iReceived = 0;
                openSuccess = true;
            }
        } else {
            if (StorageService::canResumeUpload(strNewPath, uploadedSize)) {
                if (m_file.open(QIODevice::ReadWrite | QIODevice::Append)) {
                    m_bUpload   = true;
                    m_iTotal    = uploadFileSize;
                    m_iReceived = uploadedSize;
                    openSuccess = true;
                }
            }
        }
    }

    if (!openSuccess) {
        sendSimpleResponse(this, ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND, UPLOAD_FILE_FAILED);
    }
}

void MyTcpSocket::handleDeleteFileRequest(PDU *pdu)
{
    const QString strFileName = fixedString(pdu->caData, 32);
    QString strDirPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strPath;

    bool ret = false;
    if (StorageService::resolveUserChildPath(m_strName, strDirPathRaw,
                                             strFileName, &strPath)
            && StorageService::isFile(strPath)) {
        ret = StorageService::removeFile(strPath);
    }

    sendSimpleResponse(this,
                       ENUM_MSG_TYPE_DELETE_FILE_RESPOND,
                       ret ? FILE_DELETE_OK : FILE_DELETE_FAILED);
}

void MyTcpSocket::handleDownloadFileRequest(PDU *pdu)
{
    char caFileName[32] = {'\0'};
    qint64 downloadedSize = 0;
    sscanf(pdu->caData, "%31s %lld", caFileName, &downloadedSize);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath;

    StorageService::DownloadInfo downloadInfo;
    if (StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                             QString::fromUtf8(caFileName),
                                             &strNewPath)
            && downloadedSize >= 0
            && StorageService::isFile(strNewPath)) {
        downloadInfo = StorageService::downloadInfo(strNewPath, downloadedSize);
    }

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND;
    sprintf(respdu->caData, "%s %lld %lld",
            caFileName, downloadInfo.fileSize, downloadInfo.skipSize);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;

    if (downloadInfo.skipSize < downloadInfo.fileSize) {
        FileWorker *worker = new FileWorker(this);
        worker->setupSendFile(strNewPath, downloadInfo.skipSize);
        startFileWorker(worker);
    }
}

void MyTcpSocket::handleShareFileRequest(PDU *pdu)
{
    char strSendName[32] = {'\0'};
    int shareNum = 0;
    sscanf(pdu->caData, "%31s %d", strSendName, &shareNum);
    const int size = shareNum * 32;
    if (shareNum < 0 || size > static_cast<int>(pdu->uiMsgLen)) {
        sendSimpleResponse(this, ENUM_MSG_TYPE_SHARE_FILE_RESPOND, SHARE_FILE_FAILED);
        return;
    }

    QString strSharePath = StorageService::pathFromPduMessage(
                (char*)(pdu->caMsg) + size,
                static_cast<int>(pdu->uiMsgLen) - size);
    QString resolvedSharePath;
    if (QString::fromUtf8(strSendName) != m_strName
            || !StorageService::resolveUserPath(m_strName,
                                                strSharePath,
                                                &resolvedSharePath)
            || (!StorageService::isFile(resolvedSharePath)
                && !StorageService::isDir(resolvedSharePath))) {
        sendSimpleResponse(this, ENUM_MSG_TYPE_SHARE_FILE_RESPOND, SHARE_FILE_FAILED);
        return;
    }

    PDU *respdu = mkPDU(pdu->uiMsgLen - size);
    respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_NOTE_REQUEST;
    strcpy(respdu->caData, strSendName);
    memcpy(respdu->caMsg, (char*)(pdu->caMsg) + size, pdu->uiMsgLen - size);

    char caReceiveName[32] = {'\0'};
    for (int i = 0; i < shareNum; i++) {
        memcpy(caReceiveName, (char*)(pdu->caMsg) + i * 32, 32);
        const QString receiveName = fixedString(caReceiveName, 32);
        if (StorageService::isSafeName(receiveName)) {
            const QByteArray receiveNameBytes = receiveName.toUtf8();
            MyTcpServer::getInstance().resend(receiveNameBytes.constData(), respdu);
        }
    }
    free(respdu); respdu = nullptr;

    sendSimpleResponse(this, ENUM_MSG_TYPE_SHARE_FILE_RESPOND, SHARE_FILE_OK);
}

void MyTcpSocket::handleShareFileNoteRespond(PDU *pdu)
{
    QString strSharePathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strSharePath;
    if (!StorageService::resolveSharedSourcePath(strSharePathRaw, &strSharePath)
            || (!StorageService::isFile(strSharePath)
                && !StorageService::isDir(strSharePath))) {
        return;
    }

    QString strReceivePath = StorageService::shareTargetPath(
                m_strName, strSharePath);
    if (strReceivePath.isEmpty()) {
        return;
    }

    FileWorker *worker = new FileWorker(this);
    if (StorageService::isDir(strSharePath)) {
        worker->setupCopyDir(strSharePath, strReceivePath);
    } else {
        worker->setupCopyFile(strSharePath, strReceivePath);
    }
    connect(worker, &FileWorker::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MyTcpSocket::handleMoveFileRequest(PDU *pdu)
{
    char caFileName[32] = {'\0'};
    int srcLen = 0, destLen = 0;
    sscanf(pdu->caData, "%d%d%31s", &srcLen, &destLen, caFileName);
    QString strSrcPath;
    QString strDestPath;
    QString strSrcPathRaw;
    QString strDestPathRaw;
    const QString fileName = QString::fromUtf8(caFileName);

    if (srcLen > 0
            && destLen > 0
            && srcLen + 1 + destLen <= static_cast<int>(pdu->uiMsgLen)) {
        strSrcPathRaw = StorageService::pathFromPduMessage(
                    pdu->caMsg, srcLen);
        strDestPathRaw = StorageService::pathFromPduMessage(
                    (char*)(pdu->caMsg) + (srcLen + 1), destLen);
    }

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_MOVE_FILE_RESPOND;
    if (StorageService::resolveUserPath(m_strName, strSrcPathRaw, &strSrcPath)
            && StorageService::resolveUserPath(m_strName, strDestPathRaw, &strDestPath)
            && StorageService::isSafeName(fileName)
            && QFileInfo(strSrcPath).fileName() == fileName
            && StorageService::isDir(strDestPath)) {
        bool ret = StorageService::moveFileToDir(
                    strSrcPath, strDestPath, fileName);
        strcpy(respdu->caData, ret ? MOVE_FILE_OK : COMMON_ERROR);
    } else {
        strcpy(respdu->caData, MOVE_FILE_FAILED);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}
