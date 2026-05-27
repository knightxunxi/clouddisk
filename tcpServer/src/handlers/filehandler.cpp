#include "mytcpsocket.h"
#include "mytcpserver.h"
#include "fileworker.h"
#include "pdufieldcodec.h"
#include "storageservice.h"

#include <QByteArray>
#include <QDebug>
#include <QFileInfo>

#include <cstdio>
#include <cstring>

namespace {

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
        PduFieldCodec::writeFixedString(pFileInfo->caFileName,
                                         sizeof(pFileInfo->caFileName),
                                         fileList[i].fileName());
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
    const QString strNewPath = PduFieldCodec::fixedString(pdu->caData + 32, 32);

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
    const QString strDirName = PduFieldCodec::fixedString(pdu->caData, 32);
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
    const QString oldFileName = PduFieldCodec::fixedString(pdu->caData, 32);
    const QString newFileName = PduFieldCodec::fixedString(pdu->caData + 32, 32);
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
    const QString caEnterDirName = PduFieldCodec::fixedString(pdu->caData, 32);
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
    const PduFieldCodec::UploadFileRequest request =
            PduFieldCodec::uploadFileRequest(pdu->caData);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath;

    bool openSuccess = false;
    if (request.valid
            && StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                                    request.fileName,
                                                    &strNewPath)) {
        openSuccess = m_uploadSession.start(strNewPath,
                                            request.fileSize,
                                            request.transferredSize);
    }

    if (openSuccess && m_uploadSession.isComplete()) {
        emit behaviorLog(QStringLiteral("上传"),
                         QStringLiteral("用户 %1 完成上传：%2，大小 %3 字节")
                         .arg(m_strName, request.fileName)
                         .arg(request.fileSize));
        m_uploadSession.reset();
        sendSimpleResponse(this, ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND, UPLOAD_FILE_OK);
    } else if (openSuccess) {
        m_activeUploadPath = strNewPath;
        m_activeUploadFileName = request.fileName;
        emit behaviorLog(QStringLiteral("上传"),
                         QStringLiteral("用户 %1 开始上传：%2，目标 %3，大小 %4 字节，断点 %5 字节")
                         .arg(m_strName, request.fileName, strNewPath)
                         .arg(request.fileSize)
                         .arg(request.transferredSize));
        sendSimpleResponse(this, ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND, UPLOAD_FILE_RESUME);
    } else {
        emit behaviorLog(QStringLiteral("异常"),
                         QStringLiteral("用户 %1 上传请求失败：%2，路径 %3")
                         .arg(m_strName, request.fileName, strPathRaw));
        sendSimpleResponse(this, ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND, UPLOAD_FILE_FAILED);
    }
}

void MyTcpSocket::handleUploadFileDataRequest(PDU *pdu)
{
    handleUploadData(QByteArray(pdu->caMsg, static_cast<int>(pdu->uiMsgLen)));
}

void MyTcpSocket::handleDeleteFileRequest(PDU *pdu)
{
    const QString strFileName = PduFieldCodec::fixedString(pdu->caData, 32);
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
    const PduFieldCodec::DownloadFileRequest request =
            PduFieldCodec::downloadFileRequest(pdu->caData);
    QString strPathRaw = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath;

    StorageService::DownloadInfo downloadInfo;
    bool canDownload = false;
    if (request.valid
            && StorageService::resolveUserChildPath(m_strName, strPathRaw,
                                                    request.fileName,
                                                    &strNewPath)
            && StorageService::isFile(strNewPath)) {
        downloadInfo = StorageService::downloadInfo(strNewPath,
                                                    request.transferredSize);
        canDownload = true;
    }

    if (!canDownload) {
        qDebug() << "[MyTcpSocket] 下载请求失败，用户：" << m_strName
                 << "文件：" << request.fileName
                 << "路径：" << strPathRaw;
        emit behaviorLog(QStringLiteral("异常"),
                         QStringLiteral("用户 %1 下载请求失败：%2，路径 %3")
                         .arg(m_strName, request.fileName, strPathRaw));
    } else {
        qDebug() << "[MyTcpSocket] 下载请求，用户：" << m_strName
                 << "文件：" << strNewPath
                 << "大小：" << downloadInfo.fileSize
                 << "断点：" << downloadInfo.skipSize;
    }

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND;
    if (canDownload) {
        PduFieldCodec::writeDownloadFileResponse(respdu->caData,
                                                 request.fileName,
                                                 downloadInfo.fileSize,
                                                 downloadInfo.skipSize);
    } else {
        PduFieldCodec::writeDownloadFileResponse(respdu->caData,
                                                 QString(),
                                                 -1,
                                                 0);
    }
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;

    if (canDownload && downloadInfo.skipSize < downloadInfo.fileSize) {
        m_activeDownloadPath = strNewPath;
        m_activeDownloadFileName = request.fileName;
        m_downloadCanceledByClient = false;
        m_downloadSendCompleted = false;
        emit behaviorLog(QStringLiteral("下载"),
                         QStringLiteral("用户 %1 开始下载：%2，来源 %3，大小 %4 字节，断点 %5 字节")
                         .arg(m_strName, request.fileName, strNewPath)
                         .arg(downloadInfo.fileSize)
                         .arg(downloadInfo.skipSize));
        FileWorker *worker = new FileWorker(this);
        worker->setupSendFile(strNewPath, downloadInfo.skipSize);
        startFileWorker(worker);
    } else if (canDownload) {
        qDebug() << "[MyTcpSocket] 客户端文件已完整，无需继续发送：" << strNewPath;
        emit behaviorLog(QStringLiteral("下载"),
                         QStringLiteral("用户 %1 请求下载：%2，客户端文件已完整，无需发送")
                         .arg(m_strName, request.fileName));
    }
}

void MyTcpSocket::handleShareFileRequest(PDU *pdu)
{
    const PduFieldCodec::ShareRequestData shareRequest =
            PduFieldCodec::shareRequestData(pdu->caData);
    const int shareNum = shareRequest.receiverCount;
    const int size = shareNum * 32;
    if (!shareRequest.valid || size > static_cast<int>(pdu->uiMsgLen)) {
        sendSimpleResponse(this, ENUM_MSG_TYPE_SHARE_FILE_RESPOND, SHARE_FILE_FAILED);
        return;
    }

    QString strSharePath = StorageService::pathFromPduMessage(
                (char*)(pdu->caMsg) + size,
                static_cast<int>(pdu->uiMsgLen) - size);
    QString resolvedSharePath;
    if (shareRequest.senderName != m_strName
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
    PduFieldCodec::writeFixedString(respdu->caData, sizeof(respdu->caData),
                                    shareRequest.senderName);
    memcpy(respdu->caMsg, (char*)(pdu->caMsg) + size, pdu->uiMsgLen - size);

    char caReceiveName[32] = {'\0'};
    for (int i = 0; i < shareNum; i++) {
        memcpy(caReceiveName, (char*)(pdu->caMsg) + i * 32, 32);
        const QString receiveName = PduFieldCodec::fixedString(caReceiveName, 32);
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
    const PduFieldCodec::MoveFileRequestData request =
            PduFieldCodec::moveFileRequestData(pdu->caData);
    const int srcLen = request.sourcePathLength;
    const int destLen = request.destPathLength;
    QString strSrcPath;
    QString strDestPath;
    QString strSrcPathRaw;
    QString strDestPathRaw;
    const QString fileName = request.fileName;

    if (request.valid
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
