#include "storageservice.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>

QString StorageService::pathFromPduMessage(const char *data, int length)
{
    QByteArray bytes(data, length);
    int terminator = bytes.indexOf('\0');
    if (terminator >= 0) {
        bytes.truncate(terminator);
    }
    return QString::fromUtf8(bytes);
}

QString StorageService::childPath(const QString &parentPath, const QString &name)
{
    return QDir(parentPath).filePath(name);
}

QString StorageService::shareTargetPath(const QString &receiverName, const QString &sharePath)
{
    QFileInfo fileInfo(sharePath);
    return QDir(QString("./%1").arg(receiverName)).filePath(fileInfo.fileName());
}

StorageService::CreateDirResult StorageService::createDir(const QString &parentPath, const QString &dirName)
{
    QDir dir;
    if (!dir.exists(parentPath)) {
        return ParentDirNotExist;
    }

    QString targetPath = childPath(parentPath, dirName);
    if (dir.exists(targetPath)) {
        return TargetDirAlreadyExist;
    }

    dir.mkdir(targetPath);
    return CreateDirOk;
}

QFileInfoList StorageService::listDir(const QString &path)
{
    QDir dir(path);
    return dir.entryInfoList();
}

bool StorageService::isDir(const QString &path)
{
    return QFileInfo(path).isDir();
}

bool StorageService::isFile(const QString &path)
{
    return QFileInfo(path).isFile();
}

bool StorageService::renamePath(const QString &oldPath, const QString &newPath)
{
    QDir dir;
    return dir.rename(oldPath, newPath);
}

bool StorageService::removeFile(const QString &path)
{
    QDir dir;
    return dir.remove(path);
}

bool StorageService::canResumeUpload(const QString &filePath, qint64 uploadedSize)
{
    QFile file(filePath);
    return file.exists() && file.size() == uploadedSize;
}

StorageService::DownloadInfo StorageService::downloadInfo(const QString &filePath, qint64 downloadedSize)
{
    DownloadInfo info;
    QFileInfo fileInfo(filePath);
    info.fileSize = fileInfo.size();

    if (downloadedSize > 0 && downloadedSize < info.fileSize) {
        info.skipSize = downloadedSize;
    } else if (downloadedSize >= info.fileSize) {
        info.skipSize = info.fileSize;
    }

    return info;
}

bool StorageService::moveFileToDir(const QString &srcPath, const QString &destDirPath, const QString &fileName)
{
    if (!isDir(destDirPath)) {
        return false;
    }

    return QFile::rename(srcPath, childPath(destDirPath, fileName));
}
