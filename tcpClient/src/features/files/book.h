#ifndef BOOK_H
#define BOOK_H

#include <QWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout> // 垂直布局
#include <QHBoxLayout> // 水平布局
#include "protocol.h"
#include <QThread>
#include <QFile>

#include "online.h"

// 上传工作线程（独立顶层类，避免 MOC 不支持嵌套类 Q_OBJECT 的问题）
class UploadThread : public QThread
{
    Q_OBJECT
public:
    UploadThread(const QString &filePath, qint64 offset, QObject *parent = nullptr)
        : QThread(parent), m_filePath(filePath), m_offset(offset), m_canceled(false) {}
    void cancel() { m_canceled = true; }
signals:
    void dataBlockReady(const QByteArray &data);
    void progress(qint64 bytesRead);
    void finished();
    void error(const QString &msg);
protected:
    void run() override {
        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit error(QString("无法打开文件: %1").arg(m_filePath));
            return;
        }
        if (m_offset > 0) {
            if (!file.seek(m_offset)) {
                emit error(QString("无法跳转到偏移量: %1").arg(m_offset));
                file.close();
                return;
            }
        }
        const int bufferSize = 64 * 1024;
        char buffer[bufferSize];
        qint64 bytesRead;
        while (!m_canceled && (bytesRead = file.read(buffer, bufferSize)) > 0) {
            emit dataBlockReady(QByteArray(buffer, bytesRead));
            emit progress(bytesRead);
        }
        file.close();
        if (!m_canceled) {
            emit finished();
        }
    }
private:
    QString m_filePath;
    qint64 m_offset;
    bool m_canceled;
};

class Book : public QWidget
{
    Q_OBJECT
public:
    explicit Book(QWidget *parent = nullptr);
    void updateFileList(PDU *pdu);
    void clearEnterDir();
    QString enterDir();

    QString getFileSavePath();
    QString getShareFileName();
    QString getDownloadRemotePath() const;

    void setTransferProgress(qint64 currentBytes, qint64 totalBytes, const QString &statusText);
    void setTransferStatus(const QString &statusText);
    void resetTransferProgress(const QString &statusText = QString());
    void setDownloadActive(bool active);
    void setDownloadPaused(bool paused);

    QString resumePathForRemote(const QString &remotePath) const;
    void saveDownloadResume(const QString &remotePath, const QString &localPath);
    void clearDownloadResume(const QString &remotePath);

signals:

public slots:
    void createDir();
    void flushFile();
    void deleteDir();
    void renameFile();
    void enterDir(const QModelIndex &index);
    void returnPreContent();
    void uploadFile();
    void uploadFileData();
    void deleteFile();

    void downloadFile();
    void shareFile();

    void moveFile();
    void selectDestDir();


private slots:
    void onUploadDataBlock(const QByteArray &data);
    void onUploadProgress(qint64 bytesRead);
    void onUploadFinished();
    void onUploadError(const QString &msg);
    void pauseDownload();
    void resumeDownload();

private:
    void requestDownload(const QString &currentPath,
                         const QString &downloadName,
                         const QString &savePath,
                         qint64 downloadedSize,
                         const QString &statusText);

    QListWidget *m_pFileListWidget;         // 文件列表
    QPushButton *m_pReturnPB;               // 返回主页面
    QPushButton *m_pCreateDirPB;            // 新建文件夹
    QPushButton *m_pDeleteDirPB;            // 删除文件夹
    QPushButton *m_pRenameFilePB;           // 重命名文件
    QPushButton *m_pFlushFilePB;            // 刷新文件
    QPushButton *m_pUploadFilePB;           // 上传文件
    QPushButton *m_pDownLoadFilePB;         // 下载文件
    QPushButton *m_pDeleteFilePB;           // 删除文件
    QPushButton *m_pShareFilePB;            // 分享文件
    QPushButton *m_pMoveFilePB;             //移动文件
    QPushButton *m_pSelectMoveToDirPB;      //移动文件到其他文件夹
    QPushButton *m_pPauseDownloadPB;        //暂停下载
    QPushButton *m_pResumeDownloadPB;       //继续下载
    QProgressBar *m_pTransferProgressBar;   //传输进度
    QLabel *m_pTransferStatusLabel;         //传输状态

    QString m_strEnterDir;
    QString m_strUploadFilePath;
    QString m_strFileSavePath;
    QString m_strDownloadRemotePath;
    QString m_strShareFileName;
    QString m_strMoveFileName;
    QString m_strMoveFilePath;
    QString m_strDestDirPath;

    qint64 m_uploadFileOffset;  // 文件读取偏移量
    qint64 m_uploadTotalBytes;
    qint64 m_uploadSentBytes;

    UploadThread *m_uploadThread;
};

#endif // BOOK_H
