#ifndef DOWNLOADSESSION_H
#define DOWNLOADSESSION_H

#include <QString>

class DownloadSession
{
public:
    bool start(const QString &filePath, qint64 totalBytes, qint64 receivedBytes);
    bool addBytes(qint64 bytes);
    void reset();

    bool isActive() const;
    bool isComplete() const;
    QString filePath() const;
    qint64 totalBytes() const;
    qint64 receivedBytes() const;

private:
    QString m_filePath;
    qint64 m_totalBytes = 0;
    qint64 m_receivedBytes = 0;
    bool m_active = false;
};

#endif // DOWNLOADSESSION_H
