#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QTcpSocket>
#include "protocol.h"
#include "opedb.h"
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QThread>

// ─────────────────────────────────────────────────
// FileWorker：异步文件 IO 工作线程
//   负责：① 下载时分块读文件发送给客户端
//          ② 文件/目录复制（分享功能）
//          ③ 目录递归删除
// ─────────────────────────────────────────────────
class FileWorker : public QThread
{
    Q_OBJECT
public:
    enum TaskType {
        TASK_SEND_FILE,     // 下载：分块读文件 → emit dataBlock
        TASK_COPY_FILE,     // 分享：复制单文件
        TASK_COPY_DIR,      // 分享：递归复制目录
        TASK_DELETE_DIR     // 删除：递归删除目录
    };

    explicit FileWorker(QObject *parent = nullptr)
        : QThread(parent), m_canceled(false) {}

    void cancel() { m_canceled = true; }

    // 配置任务后调用 start()
    void setupSendFile(const QString &filePath, qint64 skipBytes = 0) {
        m_task      = TASK_SEND_FILE;
        m_srcPath   = filePath;
        m_skipBytes = skipBytes;
    }
    void setupCopyFile(const QString &src, const QString &dest) {
        m_task    = TASK_COPY_FILE;
        m_srcPath = src;
        m_destPath = dest;
    }
    void setupCopyDir(const QString &src, const QString &dest) {
        m_task    = TASK_COPY_DIR;
        m_srcPath = src;
        m_destPath = dest;
    }
    void setupDeleteDir(const QString &path) {
        m_task    = TASK_DELETE_DIR;
        m_srcPath = path;
    }

signals:
    void dataBlock(const QByteArray &data);   // 供下载任务使用
    void taskFinished(bool success);

protected:
    void run() override;

private:
    void runSendFile();
    void runCopyFile();
    void runCopyDir(const QString &src, const QString &dest);
    void runDeleteDir();

    TaskType m_task    = TASK_SEND_FILE;
    QString  m_srcPath;
    QString  m_destPath;
    qint64   m_skipBytes = 0;
    bool     m_canceled;
};

// ─────────────────────────────────────────────────
// MyTcpSocket：单个客户端连接处理
// ─────────────────────────────────────────────────
class MyTcpSocket : public QTcpSocket
{
    Q_OBJECT
public:
    MyTcpSocket();
    QString getName();

signals:
    void offline(MyTcpSocket *mysocket);
    void userLoggedIn();   // 登录成功后发出，通知 UI 刷新用户列表

public slots:
    void recvMsg();
    void clientOffline();

    // 收到下载线程产生的数据块，写入 socket（在主线程执行，不阻塞）
    void onFileDataBlock(const QByteArray &data);
    // 文件工作线程结束
    void onFileWorkerFinished(bool success);

private:
    // 启动文件工作线程（先停止已有的）
    void startFileWorker(FileWorker *worker);

    QString  m_strName;
    QFile    m_file;        // 仅用于接收上传数据时写文件
    qint64   m_iTotal    = 0;
    qint64   m_iReceived = 0;
    bool     m_bUpload   = false;

    FileWorker *m_fileWorker = nullptr;  // 当前文件工作线程
};

#endif // MYTCPSOCKET_H
