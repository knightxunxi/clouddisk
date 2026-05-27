#include "book.h"
#include "tcpclient.h"
#include "pdufieldcodec.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include "opewidget.h"
#include "sharefile.h"
#include <QLabel>


Book::Book(QWidget *parent) : QWidget(parent)
{
    m_strEnterDir.clear();
    m_uploadFileOffset = 0;
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

    QHBoxLayout *pMain = new QHBoxLayout;
    pMain->setSpacing(10);
    pMain->setContentsMargins(12, 12, 12, 12);
    pMain->addWidget(m_pFileListWidget, 1);
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
    QListWidgetItem *pItem = m_pFileListWidget->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "下载文件", "请选择下载的文件");
    }
    else
    {
        QString strFileSavePath = QFileDialog::getSaveFileName();
        if(strFileSavePath.isEmpty())
        {
            QMessageBox::warning(this, "下载文件", "请选择文件保存位置");
            m_strFileSavePath.clear();
        }
        else
        {
            m_strFileSavePath = strFileSavePath;
            qDebug() << "文件保存的位置：" << m_strFileSavePath;

        }
        QString strCurPath = tcpClient::getInstance().currentPath();
        QString downloadName = pItem->text();
        // 检查本地已下载文件大小
        QFileInfo fileInfo(m_strFileSavePath);
        qint64 downloadedSize = 0;
        if (fileInfo.exists() && fileInfo.isFile()) {
            downloadedSize = fileInfo.size();
            qDebug() << "本地已存在部分文件，大小：" << downloadedSize;
        }
        PDU *pdu = mkPDU(strCurPath.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST;
        PduFieldCodec::writeDownloadFileRequest(pdu->caData,
                                                downloadName,
                                                downloadedSize);
        PduFieldCodec::writeMessage(pdu, strCurPath);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
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
    // 可以更新进度条，暂时只记录日志
    qDebug() << "上传进度，数据块大小：" << bytesRead;
}

void Book::onUploadFinished()
{
    qDebug() << "上传线程完成";
    // 注意：已在 uploadFileData() 中连接了 finished->deleteLater，
    // 此处不再调用 wait()/delete，避免在信号处理中等待线程结束导致死锁
    m_uploadThread = nullptr;
}

void Book::onUploadError(const QString &msg)
{
    qDebug() << "上传线程错误：" << msg;
    QMessageBox::warning(this, "上传文件", msg);
    if (m_uploadThread) {
        m_uploadThread->cancel();
        // 不再调用 wait()，避免死锁。cancel() 设置标志后线程会自行退出，
        // deleteLater 已在 uploadFileData() 中连接，线程结束后自动释放
        m_uploadThread = nullptr;
    }
}
