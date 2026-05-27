#include "uploadsession.h"

#include <QFileInfo>

bool UploadSession::start(const QString &filePath, qint64 totalBytes, qint64 receivedBytes)
{
    reset();

    if (filePath.isEmpty()
            || totalBytes < 0
            || receivedBytes < 0
            || receivedBytes > totalBytes) {
        return false;
    }

    m_file.setFileName(filePath);
    bool openSuccess = false;
    if (receivedBytes == 0) {
        openSuccess = m_file.open(QIODevice::WriteOnly);
    } else if (QFileInfo(filePath).isFile()
               && QFileInfo(filePath).size() == receivedBytes) {
        openSuccess = m_file.open(QIODevice::ReadWrite | QIODevice::Append);
    }

    if (!openSuccess) {
        reset();
        return false;
    }

    m_totalBytes = totalBytes;
    m_receivedBytes = receivedBytes;
    m_started = true;
    m_active = m_receivedBytes < m_totalBytes;

    if (!m_active) {
        m_file.close();
    }

    return true;
}

UploadSession::WriteResult UploadSession::writeBlock(const QByteArray &data)
{
    if (!m_active || data.isEmpty() || !m_file.isOpen()) {
        return WriteFailed;
    }

    const qint64 bytes = data.size();
    if (m_receivedBytes + bytes > m_totalBytes
            || m_file.write(data) != bytes) {
        reset();
        return WriteFailed;
    }

    m_receivedBytes += bytes;
    if (m_receivedBytes == m_totalBytes) {
        m_file.close();
        m_active = false;
        return WriteComplete;
    }

    return WriteInProgress;
}

void UploadSession::reset()
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_file.setFileName(QString());
    m_totalBytes = 0;
    m_receivedBytes = 0;
    m_active = false;
    m_started = false;
}

bool UploadSession::isActive() const
{
    return m_active;
}

bool UploadSession::isComplete() const
{
    return m_started && m_receivedBytes == m_totalBytes;
}

qint64 UploadSession::totalBytes() const
{
    return m_totalBytes;
}

qint64 UploadSession::receivedBytes() const
{
    return m_receivedBytes;
}
