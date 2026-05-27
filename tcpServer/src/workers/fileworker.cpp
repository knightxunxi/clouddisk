#include "fileworker.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

    const qint64 totalSize = QFileInfo(m_srcPath).size();
    qint64 sentBytes = m_skipBytes;
    qint64 nextLogBytes = sentBytes + 8 * 1024 * 1024;
    qDebug() << "[FileWorker] 开始发送下载文件：" << m_srcPath
             << "大小：" << totalSize
             << "起始偏移：" << m_skipBytes;

    const int BUF = 64 * 1024;
    char buf[BUF];
    qint64 ret = 0;
    while (!m_canceled && (ret = file.read(buf, BUF)) > 0) {
        emit dataBlock(QByteArray(buf, static_cast<int>(ret)));
        sentBytes += ret;
        if (sentBytes >= nextLogBytes || sentBytes == totalSize) {
            qDebug() << "[FileWorker] 下载发送进度：" << sentBytes << "/" << totalSize
                     << m_srcPath;
            nextLogBytes = sentBytes + 8 * 1024 * 1024;
        }
    }
    file.close();
    const bool success = !m_canceled && ret == 0;
    qDebug() << "[FileWorker] 下载发送结束：" << m_srcPath
             << "success=" << success
             << "sent=" << sentBytes
             << "total=" << totalSize;
    emit taskFinished(success);
}

void FileWorker::runCopyFile()
{
    bool ok = QFile::copy(m_srcPath, m_destPath);
    qDebug() << "[FileWorker] 复制文件" << m_srcPath << "->" << m_destPath
             << (ok ? "成功" : "失败");
    emit taskFinished(ok);
}

void FileWorker::runCopyDir(const QString &src, const QString &dest)
{
    if (m_canceled) {
        return;
    }

    QDir dir;
    dir.mkdir(dest);
    dir.setPath(src);
    QFileInfoList fileInfoList = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (int i = 0; i < fileInfoList.size() && !m_canceled; ++i) {
        const QFileInfo &fi = fileInfoList[i];
        QString srcTmp  = src  + '/' + fi.fileName();
        QString destTmp = dest + '/' + fi.fileName();
        if (fi.isFile()) {
            QFile::copy(srcTmp, destTmp);
        } else if (fi.isDir()) {
            runCopyDir(srcTmp, destTmp);
        }
    }
}

void FileWorker::runDeleteDir()
{
    QDir dir(m_srcPath);
    bool ok = dir.removeRecursively();
    qDebug() << "[FileWorker] 删除目录" << m_srcPath << (ok ? "成功" : "失败");
    emit taskFinished(ok);
}
