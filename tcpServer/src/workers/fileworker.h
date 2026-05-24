#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <QByteArray>
#include <QString>
#include <QThread>

class FileWorker : public QThread
{
    Q_OBJECT
public:
    enum TaskType {
        TASK_SEND_FILE,
        TASK_COPY_FILE,
        TASK_COPY_DIR,
        TASK_DELETE_DIR
    };

    explicit FileWorker(QObject *parent = nullptr)
        : QThread(parent), m_canceled(false) {}

    void cancel() { m_canceled = true; }

    void setupSendFile(const QString &filePath, qint64 skipBytes = 0) {
        m_task      = TASK_SEND_FILE;
        m_srcPath   = filePath;
        m_skipBytes = skipBytes;
    }
    void setupCopyFile(const QString &src, const QString &dest) {
        m_task     = TASK_COPY_FILE;
        m_srcPath  = src;
        m_destPath = dest;
    }
    void setupCopyDir(const QString &src, const QString &dest) {
        m_task     = TASK_COPY_DIR;
        m_srcPath  = src;
        m_destPath = dest;
    }
    void setupDeleteDir(const QString &path) {
        m_task    = TASK_DELETE_DIR;
        m_srcPath = path;
    }

signals:
    void dataBlock(const QByteArray &data);
    void taskFinished(bool success);

protected:
    void run() override;

private:
    void runSendFile();
    void runCopyFile();
    void runCopyDir(const QString &src, const QString &dest);
    void runDeleteDir();

    TaskType m_task = TASK_SEND_FILE;
    QString m_srcPath;
    QString m_destPath;
    qint64 m_skipBytes = 0;
    bool m_canceled;
};

#endif // FILEWORKER_H
