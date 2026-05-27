#include "book.h"
#include "tcpclient.h"
#include "pdufieldcodec.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include "opewidget.h"
#include "sharefile.h"
#include <QLabel>
#include <QFileInfo>
#include <QSettings>

namespace {

QString formatBytes(qint64 bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    const int decimals = unitIndex == 0 ? 0 : 2;
    return QString("%1 %2").arg(value, 0, 'f', decimals).arg(units[unitIndex]);
}

QString resumeStorageKey(const QString &loginName, const QString &remotePath)
{
    const QByteArray encoded = remotePath.toUtf8().toBase64(
                QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString("downloads/%1/%2").arg(loginName, QString::fromLatin1(encoded));
}

} // namespace


Book::Book(QWidget *parent) : QWidget(parent)
{
    m_strEnterDir.clear();
    m_strDownloadRemotePath.clear();
    m_uploadFileOffset = 0;
    m_uploadTotalBytes = 0;
    m_uploadSentBytes = 0;
    m_uploadThread = nullptr;

    m_pFileListWidget = new QListWidget;                  // 文件列表

    m_pReturnPB = new QPushButton(QStringLiteral("返回上级"));                  // 返回主页面
    m_pReturnPB->setProperty("btnStyle", "default");

    m_pCreateDirPB = new QPushButton(QStringLiteral("新建文件夹"));            // 新建文件夹
    m_pCreateDirPB->setProperty("btnStyle", "primary");

    m_pDeleteDirPB = new QPushButton(QStringLiteral("删除文件夹"));           // 删除文件夹
    m_pDeleteDirPB->setProperty("btnStyle", "warning");

    m_pRenameFilePB = new QPushButton(QStringLiteral("重命名文件"));          // 重命名文件
    m_pRenameFilePB->setProperty("btnStyle", "default");

    m_pFlushFilePB = new QPushButton(QStringLiteral("刷新文件"));            // 刷新文件
    m_pFlushFilePB->setProperty("btnStyle", "primary");

    m_pUploadFilePB = new QPushButton(QStringLiteral("上传文件"));           // 上传文件
    m_pUploadFilePB->setProperty("btnStyle", "success");

    m_pDownLoadFilePB = new QPushButton(QStringLiteral("下载文件"));        // 下载文件
    m_pDownLoadFilePB->setProperty("btnStyle", "success");

    m_pDeleteFilePB = new QPushButton(QStringLiteral("删除文件"));          // 删除文件
    m_pDeleteFilePB->setProperty("btnStyle", "danger");

    m_pShareFilePB = new QPushButton(QStringLiteral("分享文件"));           // 分享文件
    m_pShareFilePB->setProperty("btnStyle", "primary");

    m_pMoveFilePB = new QPushButton(QStringLiteral("移动文件"));             //移动文件
    m_pMoveFilePB->setProperty("btnStyle", "default");

    m_pSelectMoveToDirPB = new QPushButton(QStringLiteral("目标目录"));      //移动文件到其他文件夹
    m_pSelectMoveToDirPB->setProperty("btnStyle", "default");
    m_pSelectMoveToDirPB->setEnabled(false);

    m_pPauseDownloadPB = new QPushButton(QStringLiteral("暂停下载"));
    m_pPauseDownloadPB->setProperty("btnStyle", "warning");
    m_pPauseDownloadPB->setEnabled(false);

    m_pResumeDownloadPB = new QPushButton(QStringLiteral("继续下载"));
    m_pResumeDownloadPB->setProperty("btnStyle", "success");
    m_pResumeDownloadPB->setEnabled(false);

    m_pTransferStatusLabel = new QLabel(QStringLiteral("传输状态：空闲"));
    m_pTransferStatusLabel->setStyleSheet("color: #000000; font-size: 12px;");

    m_pTransferProgressBar = new QProgressBar;
    m_pTransferProgressBar->setRange(0, 100);
    m_pTransferProgressBar->setValue(0);
    m_pTransferProgressBar->setTextVisible(true);
    m_pTransferProgressBar->setFormat(QStringLiteral("0%"));
    m_pTransferProgressBar->setStyleSheet(
                "QProgressBar { color: #000000; background: #ffffff; border: 1px solid #b8c2cc; border-radius: 4px; text-align: center; }"
                "QProgressBar::chunk { background: #2f80ed; border-radius: 3px; }");

    // 目录操作区
    QVBoxLayout *pDirVBL = new QVBoxLayout;
    pDirVBL->setSpacing(8);
    QLabel *pDirLabel = new QLabel(QStringLiteral("目录操作"));
    pDirLabel->setStyleSheet("color: #000000; font-size: 11px; font-weight: bold; padding-left: 2px;");
    pDirVBL->addWidget(pDirLabel);
    pDirVBL->addWidget(m_pReturnPB);
    pDirVBL->addWidget(m_pCreateDirPB);
    pDirVBL->addWidget(m_pDeleteDirPB);
    pDirVBL->addWidget(m_pRenameFilePB);
    pDirVBL->addWidget(m_pFlushFilePB);
    pDirVBL->addStretch();

    // 文件操作区
    QVBoxLayout *pFileVBL = new QVBoxLayout;
    pFileVBL->setSpacing(8);
    QLabel *pFileLabel = new QLabel(QStringLiteral("文件操作"));
    pFileLabel->setStyleSheet("color: #000000; font-size: 11px; font-weight: bold; padding-left: 2px;");
    pFileVBL->addWidget(pFileLabel);
    pFileVBL->addWidget(m_pUploadFilePB);
    pFileVBL->addWidget(m_pDownLoadFilePB);
    pFileVBL->addWidget(m_pDeleteFilePB);
    pFileVBL->addWidget(m_pShareFilePB);
    pFileVBL->addWidget(m_pMoveFilePB);
    pFileVBL->addWidget(m_pSelectMoveToDirPB);
    pFileVBL->addStretch();

    QVBoxLayout *pListVBL = new QVBoxLayout;
    pListVBL->setSpacing(8);
    pListVBL->addWidget(m_pFileListWidget, 1);
    pListVBL->addWidget(m_pTransferStatusLabel);

    QHBoxLayout *pTransferHBL = new QHBoxLayout;
    pTransferHBL->setSpacing(8);
    pTransferHBL->addWidget(m_pTransferProgressBar, 1);
    pTransferHBL->addWidget(m_pPauseDownloadPB);
    pTransferHBL->addWidget(m_pResumeDownloadPB);
    pListVBL->addLayout(pTransferHBL);

    QHBoxLayout *pMain = new QHBoxLayout;
    pMain->setSpacing(10);
    pMain->setContentsMargins(12, 12, 12, 12);
    pMain->addLayout(pListVBL, 1);
    pMain->addLayout(pDirVBL);
    pMain->addLayout(pFileVBL);

    setLayout(pMain);

    connect(m_pCreateDirPB, SIGNAL(clicked(bool)), this, SLOT(createDir()));
    connect(m_pFlushFilePB, SIGNAL(clicked(bool)), this, SLOT(flushFile()));
    connect(m_pDeleteDirPB, SIGNAL(clicked(bool)), this, SLOT(deleteDir()));
    connect(m_pRenameFilePB, SIGNAL(clicked(bool)), this, SLOT(renameFile()));
    connect(m_pFileListWidget ,SIGNAL(doubleClicked(QModelIndex)), this, SLOT(enterDir(QModelIndex)));
    connect(m_pReturnPB, SIGNAL(clicked(bool)), this, SLOT(returnPreContent()));
    connect(m_pUploadFilePB, SIGNAL(clicked(bool)), this, SLOT(uploadFile()));
    connect(m_pDeleteFilePB, SIGNAL(clicked(bool)), this, SLOT(deleteFile()));
    connect(m_pDownLoadFilePB, SIGNAL(clicked(bool)), this, SLOT(downloadFile()));
    connect(m_pShareFilePB, SIGNAL(clicked(bool)), this, SLOT(shareFile()));
    connect(m_pMoveFilePB, SIGNAL(clicked(bool)), this, SLOT(moveFile()));
    connect(m_pSelectMoveToDirPB, SIGNAL(clicked(bool)), this, SLOT(selectDestDir()));
    connect(m_pPauseDownloadPB, SIGNAL(clicked(bool)), this, SLOT(pauseDownload()));
    connect(m_pResumeDownloadPB, SIGNAL(clicked(bool)), this, SLOT(resumeDownload()));
}

void Book::updateFileList(PDU *pdu)
{
    if(NULL == pdu)
    {
        return;
    }
    FileInfo *pFileInfo = NULL;
    int iCount = pdu->uiMsgLen/sizeof(FileInfo);
    m_pFileListWidget->clear();
    for(int i = 0; i < iCount ; i++)
    {
        pFileInfo = (FileInfo*)(pdu->caMsg) + i;
        qDebug() << pFileInfo->caFileName << " " << pFileInfo->iFileType;
        QListWidgetItem *pItem = new QListWidgetItem;
        if(0 == pFileInfo->iFileType)
        {
            pItem->setIcon(QIcon(QPixmap(":/map/dir.png")));
        }
        else if(1 == pFileInfo->iFileType)
        {
            pItem->setIcon(QIcon(QPixmap(":/map/file.png")));
        }
        pItem->setText(pFileInfo->caFileName);
        m_pFileListWidget->addItem(pItem);

    }
    return;
}

void Book::clearEnterDir()
{
    m_strEnterDir.clear();
}

QString Book::enterDir()
{
    return m_strEnterDir;
}

QString Book::getFileSavePath()
{
    return m_strFileSavePath;
}

QString Book::getShareFileName()
{
    return m_strShareFileName;
}

QString Book::getDownloadRemotePath() const
{
    return m_strDownloadRemotePath;
}

void Book::setTransferProgress(qint64 currentBytes, qint64 totalBytes, const QString &statusText)
{
    if (!statusText.isEmpty()) {
        setTransferStatus(statusText);
    }

    if (totalBytes <= 0) {
        m_pTransferProgressBar->setRange(0, 0);
        m_pTransferProgressBar->setFormat(QStringLiteral("等待中"));
        return;
    }

    const qint64 safeCurrent = qBound<qint64>(0, currentBytes, totalBytes);
    const int value = static_cast<int>((safeCurrent * 1000) / totalBytes);
    const double percent = totalBytes == 0
            ? 0.0
            : static_cast<double>(safeCurrent) * 100.0 / static_cast<double>(totalBytes);

    m_pTransferProgressBar->setRange(0, 1000);
    m_pTransferProgressBar->setValue(value);
    m_pTransferProgressBar->setFormat(
                QString("%1% (%2 / %3)")
                .arg(percent, 0, 'f', 1)
                .arg(formatBytes(safeCurrent))
                .arg(formatBytes(totalBytes)));
}

void Book::setTransferStatus(const QString &statusText)
{
    const QString text = statusText.isEmpty() ? QStringLiteral("空闲") : statusText;
    m_pTransferStatusLabel->setText(QStringLiteral("传输状态：%1").arg(text));
}

void Book::resetTransferProgress(const QString &statusText)
{
    m_pTransferProgressBar->setRange(0, 100);
    m_pTransferProgressBar->setValue(0);
    m_pTransferProgressBar->setFormat(QStringLiteral("0%"));
    setTransferStatus(statusText);
}

void Book::setDownloadActive(bool active)
{
    m_pPauseDownloadPB->setEnabled(active);
    if (active) {
        m_pResumeDownloadPB->setEnabled(false);
    }
}

void Book::setDownloadPaused(bool paused)
{
    m_pPauseDownloadPB->setEnabled(false);
    m_pResumeDownloadPB->setEnabled(paused
                                    && !m_strDownloadRemotePath.isEmpty()
                                    && !m_strFileSavePath.isEmpty());
}

QString Book::resumePathForRemote(const QString &remotePath) const
{
    if (remotePath.isEmpty() || tcpClient::getInstance().loginName().isEmpty()) {
        return QString();
    }

    QSettings settings("TinyDisk", "TinyDiskClient");
    return settings.value(resumeStorageKey(tcpClient::getInstance().loginName(),
                                           remotePath)).toString();
}

void Book::saveDownloadResume(const QString &remotePath, const QString &localPath)
{
    if (remotePath.isEmpty() || localPath.isEmpty()
            || tcpClient::getInstance().loginName().isEmpty()) {
        return;
    }

    QSettings settings("TinyDisk", "TinyDiskClient");
    settings.setValue(resumeStorageKey(tcpClient::getInstance().loginName(),
                                       remotePath),
                      localPath);
}

void Book::clearDownloadResume(const QString &remotePath)
{
    if (remotePath.isEmpty() || tcpClient::getInstance().loginName().isEmpty()) {
        return;
    }

    QSettings settings("TinyDisk", "TinyDiskClient");
    settings.remove(resumeStorageKey(tcpClient::getInstance().loginName(),
                                     remotePath));
}



void Book::createDir()
{
    QString newDirName = QInputDialog::getText(this, "新建文件夹", "文件夹名字：");
    if(newDirName.isEmpty())
    {
        QMessageBox::warning(this, "新建文件夹", "文件夹名不能为空");
        return;
    }
    else if(newDirName.size() >= 32)
    {
        QMessageBox::warning(this, "新建文件夹", "too long");
    }
    QString strLoginName = tcpClient::getInstance().loginName();
    QString strCurPath = tcpClient::getInstance().currentPath();
    PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_REQUEST;
    PduFieldCodec::writeFixedPair(pdu->caData, strLoginName, newDirName);
    PduFieldCodec::writeMessage(pdu, strCurPath);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Book::flushFile()
{
    clearEnterDir();
    QString strCurPath = tcpClient::getInstance().currentPath();
    qDebug() << "book flush路径" << strCurPath;
    PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_REQUEST;
    PduFieldCodec::writeMessage(pdu, strCurPath);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;

}

void Book::deleteDir()
{
    QString strCurPath = tcpClient::getInstance().currentPath();
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "删除文件夹", "请选择删除的文件夹");
    }
    else
    {
        QString deleteName = pItem->text();
        PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_DIR_REQUEST;
        PduFieldCodec::writeFixedString(pdu->caData, 32, deleteName);
        PduFieldCodec::writeMessage(pdu, strCurPath);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }

}

void Book::renameFile()
{
    QString strCurPath = tcpClient::getInstance().currentPath();
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "重命名文件", "请选择重命名的文件");
    }
    else
    {
        QString newFileName = QInputDialog::getText(this, "重命名文件", "文件新名字：");
        QString oldFileName = pItem->text();
        if(newFileName.isEmpty())
        {
            QMessageBox::warning(this, "重命名文件", "文件名不能为空");
        }
        PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_RENAME_FILE_REQUEST;
        PduFieldCodec::writeFixedPair(pdu->caData, oldFileName, newFileName);
        PduFieldCodec::writeMessage(pdu, strCurPath);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
}

void Book::enterDir(const QModelIndex &index)
{

    QString selectDirName = index.data().toString();    //选择的文件名字
    qDebug() << selectDirName;
    m_strEnterDir = selectDirName;
    QString strCurPath = tcpClient::getInstance().currentPath();
    PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_ENTER_DIR_REQUEST;
    PduFieldCodec::writeFixedString(pdu->caData, 32, selectDirName);
    PduFieldCodec::writeMessage(pdu, strCurPath);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Book::returnPreContent()
{
    QString strCurPath = tcpClient::getInstance().currentPath();
    QString strRootPath = "./" + tcpClient::getInstance().loginName();
    if(strCurPath == strRootPath)
    {
        QMessageBox::warning(this, "返回上一级目录", "已经在根目录");
    }
    else
    {
      //"目录的格式： ./aa/bb/cc"
        int index = strCurPath.lastIndexOf('/');
        QString newPath = strCurPath.remove(index, strCurPath.size() - index);
        qDebug() << "上一级目录为：" << newPath;
        tcpClient::getInstance().setCurrentPath(newPath);
        clearEnterDir();
        flushFile();
    }
}

void Book::uploadFile()
{
    QString strCurPath = tcpClient::getInstance().currentPath();
    m_strUploadFilePath = QFileDialog::getOpenFileName();
    qDebug() << m_strUploadFilePath;
    if(m_strUploadFilePath.isEmpty())
    {
        QMessageBox::warning(this, "上传文件", "文件不能为空");
    }
    else
    {

        int index = m_strUploadFilePath.lastIndexOf('/');
        QString newFileName = m_strUploadFilePath.right(m_strUploadFilePath.size() - index - 1);
        qDebug() << newFileName;

        QFile file(m_strUploadFilePath );
        qint64 uploadFileSize = file.size();
        qint64 uploadedSize = 0;
        m_uploadFileOffset = uploadedSize; // 从该偏移量开始读取文件
        m_uploadTotalBytes = uploadFileSize;
        m_uploadSentBytes = uploadedSize;
        setTransferProgress(m_uploadSentBytes,
                            m_uploadTotalBytes,
                            QStringLiteral("等待服务端接收上传"));
        QString strCurPath = tcpClient::getInstance().currentPath();
        PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_REQUEST;
        PduFieldCodec::writeMessage(pdu, strCurPath);
        PduFieldCodec::writeUploadFileRequest(pdu->caData,
                                              newFileName,
                                              uploadFileSize,
                                              uploadedSize);
        qDebug() << "上传请求：" << newFileName << "总大小：" << uploadFileSize << "已上传：" << uploadedSize;
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;

    }

}

void Book::uploadFileData()
{
    // 如果已有上传线程在运行，先取消
    if (m_uploadThread) {
        m_uploadThread->cancel();
        // 不调用 wait()，避免阻塞主线程。
        // 旧线程结束后会通过 deleteLater 自动释放。
        qDebug() << "旧上传线程仍在运行，已发送取消请求";
        m_uploadThread = nullptr;
    }
    // 创建上传线程
    m_uploadThread = new UploadThread(m_strUploadFilePath, m_uploadFileOffset, this);
    connect(m_uploadThread, &UploadThread::dataBlockReady, this, &Book::onUploadDataBlock);
    connect(m_uploadThread, &UploadThread::progress, this, &Book::onUploadProgress);
    connect(m_uploadThread, &UploadThread::finished, this, &Book::onUploadFinished);
    connect(m_uploadThread, &UploadThread::error, this, &Book::onUploadError);
    // 线程结束后自动删除
    connect(m_uploadThread, &UploadThread::finished, m_uploadThread, &QObject::deleteLater);
    connect(m_uploadThread, &UploadThread::error, m_uploadThread, &QObject::deleteLater);
    setTransferProgress(m_uploadSentBytes,
                        m_uploadTotalBytes,
                        QStringLiteral("上传中"));
    m_uploadThread->start();
    qDebug() << "上传线程启动，偏移量：" << m_uploadFileOffset;
}

void Book::deleteFile()
{
    QString strCurPath = tcpClient::getInstance().currentPath();
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "删除文件", "请选择删除的文件");
    }
    else
    {
        QString deleteName = pItem->text();
        PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FILE_REQUEST;
        PduFieldCodec::writeFixedString(pdu->caData, 32, deleteName);
        PduFieldCodec::writeMessage(pdu, strCurPath);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }

}

void Book::downloadFile()
{
    if (tcpClient::getInstance().isDownloadPaused()
            && !m_strDownloadRemotePath.isEmpty()
            && !m_strFileSavePath.isEmpty()) {
        resumeDownload();
        return;
    }

    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "下载文件", "请选择下载的文件");
    }
    else
    {
        QString strCurPath = tcpClient::getInstance().currentPath();
        QString downloadName = pItem->text();
        m_strDownloadRemotePath = strCurPath + '/' + downloadName;

        QString strFileSavePath;
        qint64 downloadedSize = 0;
        const QString resumePath = !m_strFileSavePath.isEmpty()
                ? m_strFileSavePath
                : resumePathForRemote(m_strDownloadRemotePath);
        if (!resumePath.isEmpty()) {
            QFileInfo resumeInfo(resumePath);
            if (resumeInfo.exists() && resumeInfo.isFile() && resumeInfo.size() > 0) {
                bool continueDownload = tcpClient::getInstance().isDownloadPaused();
                if (!continueDownload) {
                    const int ret = QMessageBox::question(
                                this,
                                QStringLiteral("继续下载"),
                                QStringLiteral("检测到未完成下载：\n%1\n已下载：%2\n是否继续？")
                                .arg(resumePath)
                                .arg(formatBytes(resumeInfo.size())));
                    continueDownload = ret == QMessageBox::Yes;
                }
                if (continueDownload) {
                    strFileSavePath = resumePath;
                    downloadedSize = resumeInfo.size();
                }
            } else {
                clearDownloadResume(m_strDownloadRemotePath);
            }
        }

        if (strFileSavePath.isEmpty()) {
            strFileSavePath = QFileDialog::getSaveFileName(this,
                                                           QStringLiteral("下载文件"),
                                                           downloadName);
            if(strFileSavePath.isEmpty())
            {
                QMessageBox::warning(this, "下载文件", "请选择文件保存位置");
                m_strFileSavePath.clear();
                resetTransferProgress(QStringLiteral("下载已取消"));
                return;
            }

            QFileInfo fileInfo(strFileSavePath);
            if (fileInfo.exists() && fileInfo.isFile()) {
                downloadedSize = fileInfo.size();
                qDebug() << "本地已存在部分文件，大小：" << downloadedSize;
            }
        }

        m_strFileSavePath = strFileSavePath;
        requestDownload(strCurPath,
                        downloadName,
                        m_strFileSavePath,
                        downloadedSize,
                        downloadedSize > 0
                        ? QStringLiteral("继续下载")
                        : QStringLiteral("请求下载"));
    }
}

void Book::requestDownload(const QString &currentPath,
                           const QString &downloadName,
                           const QString &savePath,
                           qint64 downloadedSize,
                           const QString &statusText)
{
    if (currentPath.isEmpty() || downloadName.isEmpty() || savePath.isEmpty()) {
        QMessageBox::warning(this, "下载文件", "下载信息不完整，无法继续");
        return;
    }

    m_strFileSavePath = savePath;
    m_strDownloadRemotePath = currentPath + '/' + downloadName;
    saveDownloadResume(m_strDownloadRemotePath, m_strFileSavePath);
    setDownloadPaused(false);
    setTransferProgress(downloadedSize,
                        qMax<qint64>(downloadedSize, 1),
                        statusText);
    qDebug() << "文件保存的位置：" << m_strFileSavePath;

    PDU *pdu = mkPDU(currentPath.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST;
    PduFieldCodec::writeDownloadFileRequest(pdu->caData,
                                            downloadName,
                                            downloadedSize);
    PduFieldCodec::writeMessage(pdu, currentPath);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Book::shareFile()
{
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "分享文件", "请选择分享的文件");
        return;
    }
    else
    {
        m_strShareFileName = pItem->text();
        qDebug() << "选中的文件为： " << m_strShareFileName;
    }
    Friend *pFriend = OpeWidget::getInstance().getFriend();
    QListWidget *pFriendList = pFriend->getFriendList();
    ShareFile::getInstance().updateFriend(pFriendList);
    if(ShareFile::getInstance().isHidden())
    {
        ShareFile::getInstance().show();

    }


}

void Book::moveFile()
{
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "移动文件", "请选择移动的文件");
    }
    else
    {
        m_strMoveFileName = pItem->text();
        qDebug() << "移动的文件为： " << m_strMoveFileName;
        QString strCurPath = tcpClient::getInstance().currentPath();
        m_strMoveFilePath = strCurPath + '/' + m_strMoveFileName;
        m_pSelectMoveToDirPB->setEnabled(true);

    }
}

void Book::selectDestDir()
{
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "移动文件", "请选择移动到的文件夹");
    }
    else
    {
        QString destDirName = pItem->text();
        QString strCurPath = tcpClient::getInstance().currentPath();
        m_strDestDirPath = strCurPath + '/' + destDirName;

        const QByteArray srcPathBytes = m_strMoveFilePath.toUtf8();
        const QByteArray destPathBytes = m_strDestDirPath.toUtf8();
        int srcLen = srcPathBytes.size();
        int destLen = destPathBytes.size();
        PDU *pdu = mkPDU(srcLen + destLen + 2);
        pdu->uiMsgType = ENUM_MSG_TYPE_MOVE_FILE_REQUEST;
        PduFieldCodec::writeMoveFileRequestData(pdu->caData,
                                                srcLen,
                                                destLen,
                                                m_strMoveFileName);
        memcpy(pdu->caMsg, srcPathBytes.constData(), srcLen);
        memcpy((char*)(pdu->caMsg) + (srcLen + 1), destPathBytes.constData(), destLen);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
    m_pSelectMoveToDirPB->setEnabled(false);
}

void Book::onUploadDataBlock(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    PDU *pdu = mkPDU(static_cast<unit>(data.size()));
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_DATA_REQUEST;
    memcpy(pdu->caMsg, data.constData(), data.size());
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Book::onUploadProgress(qint64 bytesRead)
{
    m_uploadSentBytes += bytesRead;
    setTransferProgress(m_uploadSentBytes,
                        m_uploadTotalBytes,
                        QStringLiteral("上传中"));
    qDebug() << "上传进度，已发送：" << m_uploadSentBytes
             << "总大小：" << m_uploadTotalBytes;
}

void Book::onUploadFinished()
{
    qDebug() << "上传线程完成";
    setTransferProgress(m_uploadSentBytes,
                        m_uploadTotalBytes,
                        QStringLiteral("上传数据发送完成，等待服务端确认"));
    // 注意：已在 uploadFileData() 中连接了 finished->deleteLater，
    // 此处不再调用 wait()/delete，避免在信号处理中等待线程结束导致死锁
    m_uploadThread = nullptr;
}

void Book::onUploadError(const QString &msg)
{
    qDebug() << "上传线程错误：" << msg;
    setTransferStatus(QStringLiteral("上传失败"));
    QMessageBox::warning(this, "上传文件", msg);
    if (m_uploadThread) {
        m_uploadThread->cancel();
        // 不再调用 wait()，避免死锁。cancel() 设置标志后线程会自行退出，
        // deleteLater 已在 uploadFileData() 中连接，线程结束后自动释放
        m_uploadThread = nullptr;
    }
}

void Book::pauseDownload()
{
    tcpClient::getInstance().pauseDownload();
}

void Book::resumeDownload()
{
    if (m_strDownloadRemotePath.isEmpty() || m_strFileSavePath.isEmpty()) {
        QMessageBox::warning(this, "继续下载", "没有可继续的下载任务");
        setDownloadPaused(false);
        return;
    }

    QFileInfo fileInfo(m_strFileSavePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(this, "继续下载", "本地未完成文件不存在，无法继续");
        setDownloadPaused(false);
        return;
    }

    const int index = m_strDownloadRemotePath.lastIndexOf('/');
    if (index <= 0 || index >= m_strDownloadRemotePath.size() - 1) {
        QMessageBox::warning(this, "继续下载", "远程文件路径无效，无法继续");
        setDownloadPaused(false);
        return;
    }

    const QString currentPath = m_strDownloadRemotePath.left(index);
    const QString downloadName = m_strDownloadRemotePath.mid(index + 1);
    requestDownload(currentPath,
                    downloadName,
                    m_strFileSavePath,
                    fileInfo.size(),
                    QStringLiteral("继续下载"));
}
