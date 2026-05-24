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
    static QString userRootPath(const QString &userName);
    static bool isSafeName(const QString &name);
    static bool resolveUserPath(const QString &userName,
                                const QString &path,
                                QString *resolvedPath);
    static bool resolveUserChildPath(const QString &userName,
                                     const QString &parentPath,
                                     const QString &childName,
                                     QString *resolvedPath);
    static bool resolveSharedSourcePath(const QString &path,
                                        QString *resolvedPath);
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
