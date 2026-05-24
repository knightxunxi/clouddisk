#ifndef STORAGESERVICE_H
#define STORAGESERVICE_H

#include <QFileInfoList>
#include <QString>

class StorageService
{
public:
    enum CreateDirResult {
        CreateDirOk,
        ParentDirNotExist,
        TargetDirAlreadyExist
    };

    struct DownloadInfo {
        qint64 fileSize = 0;
        qint64 skipSize = 0;
    };

    static QString pathFromPduMessage(const char *data, int length);
    static QString childPath(const QString &parentPath, const QString &name);
    static QString shareTargetPath(const QString &receiverName, const QString &sharePath);

    static CreateDirResult createDir(const QString &parentPath, const QString &dirName);
    static QFileInfoList listDir(const QString &path);
    static bool isDir(const QString &path);
    static bool isFile(const QString &path);
    static bool renamePath(const QString &oldPath, const QString &newPath);
    static bool removeFile(const QString &path);
    static bool canResumeUpload(const QString &filePath, qint64 uploadedSize);
    static DownloadInfo downloadInfo(const QString &filePath, qint64 downloadedSize);
    static bool moveFileToDir(const QString &srcPath, const QString &destDirPath, const QString &fileName);
};

#endif // STORAGESERVICE_H
