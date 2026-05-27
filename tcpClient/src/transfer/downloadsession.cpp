#include "downloadsession.h"

bool DownloadSession::start(const QString &filePath, qint64 totalBytes, qint64 receivedBytes)
{
    reset();

    if (filePath.isEmpty()
            || totalBytes < 0
            || receivedBytes < 0
            || receivedBytes > totalBytes) {
        return false;
    }

    m_filePath = filePath;
    m_totalBytes = totalBytes;
    m_receivedBytes = receivedBytes;
    m_active = m_receivedBytes < m_totalBytes;
    return true;
}

bool DownloadSession::addBytes(qint64 bytes)
{
    if (!m_active || bytes <= 0 || m_receivedBytes + bytes > m_totalBytes) {
        return false;
    }

    m_receivedBytes += bytes;
    if (m_receivedBytes == m_totalBytes) {
        m_active = false;
    }
    return true;
}

void DownloadSession::reset()
{
    m_filePath.clear();
    m_totalBytes = 0;
    m_receivedBytes = 0;
    m_active = false;
}

bool DownloadSession::isActive() const
{
    return m_active;
}

bool DownloadSession::isComplete() const
{
    return !m_filePath.isEmpty() && m_receivedBytes == m_totalBytes;
}

QString DownloadSession::filePath() const
{
    return m_filePath;
}

qint64 DownloadSession::totalBytes() const
{
    return m_totalBytes;
}

qint64 DownloadSession::receivedBytes() const
{
    return m_receivedBytes;
}
