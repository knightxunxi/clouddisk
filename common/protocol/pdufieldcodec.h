#ifndef PDUFIELDCODEC_H
#define PDUFIELDCODEC_H

#include "protocol.h"

#include <QByteArray>
#include <QString>

class PduFieldCodec
{
public:
    struct FixedPair {
        QString first;
        QString second;
    };

    struct UploadFileRequest {
        QString fileName;
        qint64 fileSize = 0;
        qint64 transferredSize = 0;
        bool valid = false;
    };

    struct DownloadFileRequest {
        QString fileName;
        qint64 transferredSize = 0;
        bool valid = false;
    };

    struct DownloadFileResponse {
        QString fileName;
        qint64 fileSize = 0;
        qint64 skipSize = 0;
        bool valid = false;
    };

    struct ShareRequestData {
        QString senderName;
        int receiverCount = 0;
        bool valid = false;
    };

    struct MoveFileRequestData {
        int sourcePathLength = 0;
        int destPathLength = 0;
        QString fileName;
        bool valid = false;
    };

    static QString fixedString(const char *data, int length);
    static void writeFixedString(char *dest, int length, const QString &value);
    static FixedPair fixedPair(const char *data);
    static void writeFixedPair(char *dest, const QString &first, const QString &second);

    static QByteArray messageBytes(const PDU *pdu);
    static QString messageString(const PDU *pdu);
    static void writeMessage(PDU *pdu, const QString &message);
    static void writeMessage(PDU *pdu, const QByteArray &message);

    static UploadFileRequest uploadFileRequest(const char *data);
    static void writeUploadFileRequest(char *dest,
                                       const QString &fileName,
                                       qint64 fileSize,
                                       qint64 transferredSize);

    static DownloadFileRequest downloadFileRequest(const char *data);
    static void writeDownloadFileRequest(char *dest,
                                         const QString &fileName,
                                         qint64 transferredSize);

    static DownloadFileResponse downloadFileResponse(const char *data);
    static void writeDownloadFileResponse(char *dest,
                                          const QString &fileName,
                                          qint64 fileSize,
                                          qint64 skipSize);

    static ShareRequestData shareRequestData(const char *data);
    static void writeShareRequestData(char *dest,
                                      const QString &senderName,
                                      int receiverCount);

    static MoveFileRequestData moveFileRequestData(const char *data);
    static void writeMoveFileRequestData(char *dest,
                                         int sourcePathLength,
                                         int destPathLength,
                                         const QString &fileName);
};

#endif // PDUFIELDCODEC_H
