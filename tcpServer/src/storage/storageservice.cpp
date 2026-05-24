#include "storageservice.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace {

QString normalizedPathText(QString path)
{
    path = path.trimmed();
    path.replace('\\', '/');
    return path;
}

QString absoluteCleanPath(const QString &path)
{
    return QDir::fromNativeSeparators(
                QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

bool hasParentTraversal(const QString &path)
{
    const QStringList parts = normalizedPathText(path).split(
                '/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

bool isPathInRoot(const QString &path, const QString &root)
{
    QString normalizedPath = QDir::fromNativeSeparators(QDir::cleanPath(path));
    QString normalizedRoot = QDir::fromNativeSeparators(QDir::cleanPath(root));
#ifdef Q_OS_WIN
    normalizedPath = normalizedPath.toCaseFolded();
    normalizedRoot = normalizedRoot.toCaseFolded();
#endif
    return normalizedPath == normalizedRoot
            || normalizedPath.startsWith(normalizedRoot + '/');
}

QString userNameFromPath(const QString &path)
{
    QString normalizedPath = normalizedPathText(path);
    const QStringList parts = normalizedPath.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QString();
    }
    if (parts.first() == ".") {
        return parts.size() > 1 ? parts.at(1) : QString();
    }
    return parts.first();
}

} // namespace

QString StorageService::pathFromPduMessage(const char *data, int length)
{
    QByteArray bytes(data, length);
    int terminator = bytes.indexOf('\0');
    if (terminator >= 0) {
        bytes.truncate(terminator);
    }
    return QString::fromUtf8(bytes);
}

QString StorageService::userRootPath(const QString &userName)
{
    return QDir::fromNativeSeparators(
                QDir::cleanPath(QDir::current().absoluteFilePath(userName)));
}

bool StorageService::isSafeName(const QString &name)
{
    const QString normalizedName = normalizedPathText(name);
    return !normalizedName.isEmpty()
            && normalizedName != "."
            && normalizedName != ".."
            && !normalizedName.contains('/')
            && !QDir::isAbsolutePath(normalizedName);
}

bool StorageService::resolveUserPath(const QString &userName,
                                     const QString &path,
                                     QString *resolvedPath)
{
    if (!isSafeName(userName)) {
        return false;
    }

    const QString normalizedPath = normalizedPathText(path);
    if (normalizedPath.isEmpty()
            || QDir::isAbsolutePath(normalizedPath)
            || hasParentTraversal(normalizedPath)) {
        return false;
    }

    const QString rootPath = userRootPath(userName);
    const QString absolutePath = absoluteCleanPath(normalizedPath);
    if (!isPathInRoot(absolutePath, rootPath)) {
        return false;
    }

    if (resolvedPath) {
        *resolvedPath = absolutePath;
    }
    return true;
}

bool StorageService::resolveUserChildPath(const QString &userName,
                                          const QString &parentPath,
                                          const QString &childName,
                                          QString *resolvedPath)
{
    QString resolvedParentPath;
    if (!resolveUserPath(userName, parentPath, &resolvedParentPath)
            || !isSafeName(childName)) {
        return false;
    }

    const QString rootPath = userRootPath(userName);
    const QString absolutePath = absoluteCleanPath(
                childPath(resolvedParentPath, normalizedPathText(childName)));
    if (!isPathInRoot(absolutePath, rootPath)) {
        return false;
    }

    if (resolvedPath) {
        *resolvedPath = absolutePath;
    }
    return true;
}

bool StorageService::resolveSharedSourcePath(const QString &path,
                                             QString *resolvedPath)
{
    const QString ownerName = userNameFromPath(path);
    if (ownerName.isEmpty()) {
        return false;
    }

    return resolveUserPath(ownerName, path, resolvedPath);
}

QString StorageService::childPath(const QString &parentPath, const QString &name)
{
    return QDir(parentPath).filePath(name);
}

QString StorageService::shareTargetPath(const QString &receiverName, const QString &sharePath)
{
    QFileInfo fileInfo(sharePath);
    const QString fileName = fileInfo.fileName();
    if (!isSafeName(receiverName) || !isSafeName(fileName)) {
        return QString();
    }
    return QDir(userRootPath(receiverName)).filePath(fileName);
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
