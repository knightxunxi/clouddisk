#include "pdufieldcodec.h"

#include <QStringList>

#include <cstdio>
#include <cstring>

namespace {

QStringList splitDataFields(const char *data)
{
    return PduFieldCodec::fixedString(data, 64).split(' ', Qt::SkipEmptyParts);
}

void writeFormatted(char *dest, const char *format, const QByteArray &name, qint64 first, qint64 second)
{
    std::memset(dest, 0, 64);
    std::snprintf(dest, 64, format, name.constData(),
                  static_cast<long long>(first),
                  static_cast<long long>(second));
}

} // namespace

QString PduFieldCodec::fixedString(const char *data, int length)
{
    if (data == nullptr || length <= 0) {
        return QString();
    }

    QByteArray bytes(data, length);
    const int terminator = bytes.indexOf('\0');
    if (terminator >= 0) {
        bytes.truncate(terminator);
    }
    return QString::fromUtf8(bytes).trimmed();
}

void PduFieldCodec::writeFixedString(char *dest, int length, const QString &value)
{
    if (dest == nullptr || length <= 0) {
        return;
    }

    std::memset(dest, 0, static_cast<size_t>(length));
    const QByteArray bytes = value.toUtf8();
    const int copyLength = qMin(bytes.size(), length - 1);
    if (copyLength > 0) {
        std::memcpy(dest, bytes.constData(), static_cast<size_t>(copyLength));
    }
}

PduFieldCodec::FixedPair PduFieldCodec::fixedPair(const char *data)
{
    FixedPair pair;
    pair.first = fixedString(data, 32);
    pair.second = fixedString(data + 32, 32);
    return pair;
}

void PduFieldCodec::writeFixedPair(char *dest, const QString &first, const QString &second)
{
    writeFixedString(dest, 32, first);
    writeFixedString(dest + 32, 32, second);
}

QByteArray PduFieldCodec::messageBytes(const PDU *pdu)
{
    if (pdu == nullptr || pdu->uiMsgLen == 0) {
        return QByteArray();
    }
    return QByteArray(pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
}

QString PduFieldCodec::messageString(const PDU *pdu)
{
    if (pdu == nullptr) {
        return QString();
    }
    return fixedString(pdu->caMsg, static_cast<int>(pdu->uiMsgLen));
}

void PduFieldCodec::writeMessage(PDU *pdu, const QString &message)
{
    writeMessage(pdu, message.toUtf8());
}

void PduFieldCodec::writeMessage(PDU *pdu, const QByteArray &message)
{
    if (pdu == nullptr || message.isEmpty()) {
        return;
    }
    const int copyLength = qMin(message.size(), static_cast<int>(pdu->uiMsgLen));
    if (copyLength > 0) {
        std::memcpy(pdu->caMsg, message.constData(), static_cast<size_t>(copyLength));
    }
}

PduFieldCodec::UploadFileRequest PduFieldCodec::uploadFileRequest(const char *data)
{
    UploadFileRequest request;
    const QStringList fields = splitDataFields(data);
    if (fields.size() < 3) {
        return request;
    }

    bool fileSizeOk = false;
    bool transferredOk = false;
    request.fileName = fields.at(0);
    request.fileSize = fields.at(1).toLongLong(&fileSizeOk);
    request.transferredSize = fields.at(2).toLongLong(&transferredOk);
    request.valid = !request.fileName.isEmpty()
            && fileSizeOk
            && transferredOk
            && request.fileSize >= 0
            && request.transferredSize >= 0
            && request.transferredSize <= request.fileSize;
    return request;
}

void PduFieldCodec::writeUploadFileRequest(char *dest,
                                           const QString &fileName,
                                           qint64 fileSize,
                                           qint64 transferredSize)
{
    writeFormatted(dest, "%s %lld %lld", fileName.toUtf8(), fileSize, transferredSize);
}

PduFieldCodec::DownloadFileRequest PduFieldCodec::downloadFileRequest(const char *data)
{
    DownloadFileRequest request;
    const QStringList fields = splitDataFields(data);
    if (fields.size() < 2) {
        return request;
    }

    bool transferredOk = false;
    request.fileName = fields.at(0);
    request.transferredSize = fields.at(1).toLongLong(&transferredOk);
    request.valid = !request.fileName.isEmpty()
            && transferredOk
            && request.transferredSize >= 0;
    return request;
}

void PduFieldCodec::writeDownloadFileRequest(char *dest,
                                             const QString &fileName,
                                             qint64 transferredSize)
{
    std::memset(dest, 0, 64);
    std::snprintf(dest, 64, "%s %lld",
                  fileName.toUtf8().constData(),
                  static_cast<long long>(transferredSize));
}

PduFieldCodec::DownloadFileResponse PduFieldCodec::downloadFileResponse(const char *data)
{
    DownloadFileResponse response;
    const QStringList fields = splitDataFields(data);
    if (fields.size() < 3) {
        return response;
    }

    bool fileSizeOk = false;
    bool skipSizeOk = false;
    response.fileName = fields.at(0);
    response.fileSize = fields.at(1).toLongLong(&fileSizeOk);
    response.skipSize = fields.at(2).toLongLong(&skipSizeOk);
    response.valid = !response.fileName.isEmpty()
            && fileSizeOk
            && skipSizeOk
            && response.fileSize >= 0
            && response.skipSize >= 0
            && response.skipSize <= response.fileSize;
    return response;
}

void PduFieldCodec::writeDownloadFileResponse(char *dest,
                                              const QString &fileName,
                                              qint64 fileSize,
                                              qint64 skipSize)
{
    writeFormatted(dest, "%s %lld %lld", fileName.toUtf8(), fileSize, skipSize);
}

PduFieldCodec::ShareRequestData PduFieldCodec::shareRequestData(const char *data)
{
    ShareRequestData request;
    const QStringList fields = splitDataFields(data);
    if (fields.size() < 2) {
        return request;
    }

    bool receiverCountOk = false;
    request.senderName = fields.at(0);
    request.receiverCount = fields.at(1).toInt(&receiverCountOk);
    request.valid = !request.senderName.isEmpty()
            && receiverCountOk
            && request.receiverCount >= 0;
    return request;
}

void PduFieldCodec::writeShareRequestData(char *dest,
                                          const QString &senderName,
                                          int receiverCount)
{
    std::memset(dest, 0, 64);
    std::snprintf(dest, 64, "%s %d", senderName.toUtf8().constData(), receiverCount);
}

PduFieldCodec::MoveFileRequestData PduFieldCodec::moveFileRequestData(const char *data)
{
    MoveFileRequestData request;
    const QStringList fields = splitDataFields(data);
    if (fields.size() < 3) {
        return request;
    }

    bool sourceOk = false;
    bool destOk = false;
    request.sourcePathLength = fields.at(0).toInt(&sourceOk);
    request.destPathLength = fields.at(1).toInt(&destOk);
    request.fileName = fields.at(2);
    request.valid = sourceOk
            && destOk
            && request.sourcePathLength > 0
            && request.destPathLength > 0
            && !request.fileName.isEmpty();
    return request;
}

void PduFieldCodec::writeMoveFileRequestData(char *dest,
                                             int sourcePathLength,
                                             int destPathLength,
                                             const QString &fileName)
{
    std::memset(dest, 0, 64);
    std::snprintf(dest, 64, "%d %d %s",
                  sourcePathLength,
                  destPathLength,
                  fileName.toUtf8().constData());
}
