#include "mytcpsocket.h"
#include "mytcpserver.h"
#include "fileworker.h"
#include "storageservice.h"

#include <cstdio>
#include <cstring>

void MyTcpSocket::handleCreateDirRequest(PDU *pdu)
{
    QString strCurPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;

    char strNewPath[32] = {'\0'};
    memcpy(strNewPath, pdu->caData + 32, 32);
    StorageService::CreateDirResult result =
            StorageService::createDir(strCurPath, strNewPath);
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
    QString strCurPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QFileInfoList fileList = StorageService::listDir(strCurPath);
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
}

void MyTcpSocket::handleDeleteDirRequest(PDU *pdu)
{
    char strDirName[32] = {'\0'};
    strcpy(strDirName, pdu->caData);
    QString strDirPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strPath = StorageService::childPath(strDirPath, strDirName);

    if (StorageService::isDir(strPath)) {
        FileWorker *worker = new FileWorker(this);
        worker->setupDeleteDir(strPath);
        connect(worker, &FileWorker::taskFinished,
                this, [this](bool ok) {
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
        PDU *respdu = mkPDU(0);
        respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_DIR_RESPOND;
        memcpy(respdu->caData, DIR_DELETE_FAILED, strlen(DIR_DELETE_FAILED));
        write((char*)respdu, respdu->uiPDULen);
        free(respdu); respdu = nullptr;
    }
}

void MyTcpSocket::handleRenameFileRequest(PDU *pdu)
{
    char oldFileName[32] = {'\0'};
    char newFileName[32] = {'\0'};
    strncpy(oldFileName, pdu->caData,      32);
    strncpy(newFileName, pdu->caData + 32, 32);
    QString strPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strOldPath = StorageService::childPath(strPath, oldFileName);
    QString strNewPath = StorageService::childPath(strPath, newFileName);

    bool ret = StorageService::renamePath(strOldPath, strNewPath);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_RENAME_FILE_RESPOND;
    memcpy(respdu->caData,
           ret ? RENAME_FILE_OK : RENAME_FILE_FAILED,
           ret ? strlen(RENAME_FILE_OK) : strlen(RENAME_FILE_FAILED));
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleEnterDirRequest(PDU *pdu)
{
    char caEnterDirName[32] = {'\0'};
    strncpy(caEnterDirName, pdu->caData, 32);
    QString strPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath = StorageService::childPath(strPath, caEnterDirName);

    if (StorageService::isDir(strNewPath)) {
        QFileInfoList fileList = StorageService::listDir(strNewPath);
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
}

void MyTcpSocket::handleUploadFileRequest(PDU *pdu)
{
    char uploadFileName[32] = {'\0'};
    qint64 uploadFileSize = 0;
    qint64 uploadedSize   = 0;
    sscanf(pdu->caData, "%s %lld %lld",
           uploadFileName, &uploadFileSize, &uploadedSize);
    QString strPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath = StorageService::childPath(strPath, uploadFileName);

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
        if (StorageService::canResumeUpload(strNewPath, uploadedSize)) {
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
}

void MyTcpSocket::handleDeleteFileRequest(PDU *pdu)
{
    char strFileName[32] = {'\0'};
    strcpy(strFileName, pdu->caData);
    QString strDirPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strPath = StorageService::childPath(strDirPath, strFileName);

    bool ret = false;
    if (StorageService::isFile(strPath)) {
        ret = StorageService::removeFile(strPath);
    }

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FILE_RESPOND;
    memcpy(respdu->caData,
           ret ? FILE_DELETE_OK : FILE_DELETE_FAILED,
           ret ? strlen(FILE_DELETE_OK) : strlen(FILE_DELETE_FAILED));
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleDownloadFileRequest(PDU *pdu)
{
    char caFileName[32] = {'\0'};
    qint64 downloadedSize = 0;
    sscanf(pdu->caData, "%s %lld", caFileName, &downloadedSize);
    QString strPath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strNewPath = StorageService::childPath(strPath, caFileName);

    StorageService::DownloadInfo downloadInfo =
            StorageService::downloadInfo(strNewPath, downloadedSize);

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
}

void MyTcpSocket::handleShareFileNoteRespond(PDU *pdu)
{
    QString strSharePath = StorageService::pathFromPduMessage(
                pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
    QString strReceivePath = StorageService::shareTargetPath(
                pdu->caData, strSharePath);

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
    sscanf(pdu->caData, "%d%d%s", &srcLen, &destLen, caFileName);
    QString strSrcPath = StorageService::pathFromPduMessage(
                pdu->caMsg, srcLen);
    QString strDestPath = StorageService::pathFromPduMessage(
                (char*)(pdu->caMsg) + (srcLen + 1), destLen);

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_MOVE_FILE_RESPOND;
    if (StorageService::isDir(strDestPath)) {
        bool ret = StorageService::moveFileToDir(
                    strSrcPath, strDestPath, caFileName);
        strcpy(respdu->caData, ret ? MOVE_FILE_OK : COMMON_ERROR);
    } else {
        strcpy(respdu->caData, MOVE_FILE_FAILED);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}
