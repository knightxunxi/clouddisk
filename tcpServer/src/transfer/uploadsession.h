#ifndef UPLOADSESSION_H
#define UPLOADSESSION_H

#include <QByteArray>
#include <QFile>
#include <QString>

class UploadSession
{
public:
    enum WriteResult {
        WriteInProgress,
        WriteComplete,
        WriteFailed
    };

    bool start(const QString &filePath, qint64 totalBytes, qint64 receivedBytes);
    WriteResult writeBlock(const QByteArray &data);
    void reset();

    bool isActive() const;
    bool isComplete() const;
    qint64 totalBytes() const;
    qint64 receivedBytes() const;

private:
    QFile m_file;
    qint64 m_totalBytes = 0;
    qint64 m_receivedBytes = 0;
    bool m_active = false;
    bool m_started = false;
};

#endif // UPLOADSESSION_H
