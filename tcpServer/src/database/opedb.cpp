#include "opedb.h"
#include <QMessageBox>
#include <QDebug>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QSqlError>
#include <QRegularExpression>

OpeDB::OpeDB(QObject *parent) : QObject(parent)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
}

OpeDB &OpeDB::getInstance()
{
    static OpeDB instance;
    return instance;
}

// 计算密码的SHA-256哈希（十六进制字符串）
static QString hashPassword(const QString &password)
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

void OpeDB::init()
{
    QMutexLocker locker(&m_mutex);
    // 从配置文件读取数据库路径
    QString dbPath = "cloud.db"; // 默认值
    QFile file(":/server.config");
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray baData = file.readAll();
        file.close();
        QString strData = QString::fromUtf8(baData);
        QStringList strList = strData.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if(strList.size() >= 3) {
            dbPath = strList.at(2);
        }
    }

    m_db.setHostName("localhost");
    m_db.setDatabaseName(dbPath);
    qDebug() << "尝试打开数据库：" << dbPath << "，绝对路径：" << QDir(dbPath).absolutePath();
    if(m_db.open())
    {
        QSqlQuery query(m_db);

        // 创建userInfo表（如果不存在）
        query.exec("CREATE TABLE IF NOT EXISTS userInfo ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "name VARCHAR(32) UNIQUE NOT NULL, "
                  "pwd VARCHAR(64) NOT NULL, "  // 存储哈希密码（64字符十六进制）
                  "online INTEGER DEFAULT 0)");

        // 创建friend表（如果不存在）
        query.exec("CREATE TABLE IF NOT EXISTS friend ("
                  "id INTEGER NOT NULL, "
                  "friendId INTEGER NOT NULL, "
                  "PRIMARY KEY (id, friendId), "
                  "FOREIGN KEY (id) REFERENCES userInfo(id), "
                  "FOREIGN KEY (friendId) REFERENCES userInfo(id))");

        resetAllOnlineStatus();

        // 显示现有用户
        query.exec("select * from userInfo");
        while(query.next())
        {
            QString data = QString("%1, %2, %3, %4").arg(query.value(0).toString()).arg(query.value(1).toString())
               .arg(query.value(2).toString()).arg(query.value(3).toString());
            qDebug() << data;
        }

        qDebug() << "数据库初始化完成，表已创建/验证";
    }
    else
    {
        qDebug() << "数据库打开失败：" << m_db.lastError().text();
        QMessageBox::critical(NULL, "打开数据库", "打开数据库失败");
    }
}

void OpeDB::resetAllOnlineStatus()
{
    QSqlQuery query(m_db);
    if(!query.exec("UPDATE userInfo SET online=0")) {
        qDebug() << "重置在线状态失败：" << query.lastError().text();
    }
}

bool OpeDB::handleRegist(const char *name, const char *pwd)
{
    if(name == NULL || pwd == NULL){
        return false;
    }
    QMutexLocker locker(&m_mutex);

    // 检查用户名是否已存在
    QSqlQuery checkQuery(m_db);
    checkQuery.prepare("SELECT id FROM userInfo WHERE name=?");
    checkQuery.addBindValue(name);
    if(checkQuery.exec() && checkQuery.next()) {
        // 用户名已存在
        qDebug() << "注册失败：用户名" << name << "已存在";
        return false;
    }

    QString hashedPwd = hashPassword(QString(pwd));
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO userInfo(name, pwd, online) VALUES(?, ?, 0)");
    query.addBindValue(name);
    query.addBindValue(hashedPwd);
    bool success = query.exec();
    if(!success) {
        qDebug() << "注册失败：" ;
    }
    return success;
}

bool OpeDB::handleLogin(const char *name, const char *pwd)
{
    if(name == NULL || pwd == NULL){
        return false;
    }
    QMutexLocker locker(&m_mutex);

    QString inputPwd = QString(pwd);
    QString hashedPwd = hashPassword(inputPwd);

    // 查询用户信息（不限制在线状态，允许重新登录）
    QSqlQuery query(m_db);
    query.prepare("SELECT pwd FROM userInfo WHERE name=?");
    query.addBindValue(name);

    if(!query.exec()) {
        qDebug() << "登录查询失败：" << query.lastError().text();
        return false;
    }

    if(query.next())
    {
        QString dbPwd = query.value(0).toString();
        bool passwordValid = false;

        // 检查密码是否匹配（支持哈希和明文两种格式）
        if(dbPwd == hashedPwd) {
            // 哈希密码匹配
            passwordValid = true;
        } else if(dbPwd == inputPwd) {
            // 明文密码匹配（向后兼容），迁移到哈希存储
            passwordValid = true;
            // 更新数据库中的密码为哈希值
            QSqlQuery updatePwdQuery(m_db);
            updatePwdQuery.prepare("UPDATE userInfo SET pwd=? WHERE name=?");
            updatePwdQuery.addBindValue(hashedPwd);
            updatePwdQuery.addBindValue(name);
            if(!updatePwdQuery.exec()) {
                qDebug() << "密码更新失败：" << updatePwdQuery.lastError().text();
            }
        }

        if(passwordValid) {
            // 设置用户在线状态
            QSqlQuery updateOnlineQuery(m_db);
            updateOnlineQuery.prepare("UPDATE userInfo SET online=1 WHERE name=?");
            updateOnlineQuery.addBindValue(name);
            if(!updateOnlineQuery.exec()) {
                qDebug() << "在线状态更新失败：" << updateOnlineQuery.lastError().text();
                return false;
            }
            qDebug() << "用户" << name << "登录成功";
            return true;
        } else {
            qDebug() << "用户" << name << "密码错误";
            return false;
        }
    }
    else
    {
        qDebug() << "登录失败：用户" << name << "不存在";
        return false;
    }
}

void OpeDB::handleOffline(const char *name)
{
    if(name == NULL){
        return;
    }
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE userInfo SET online=0 WHERE name=?");
    query.addBindValue(name);
    query.exec();
}

QStringList OpeDB::handleAllOnline()
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.exec("SELECT name FROM userInfo WHERE online=1");
    QStringList result;
    result.clear();
    while(query.next())
    {
        result.append(query.value(0).toString());
    }
    return result;
}

int OpeDB::handleSearchUser(const char *name)
{
    if(NULL == name){
        return -1;
    }
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT online FROM userInfo WHERE name=?");
    query.addBindValue(name);
    query.exec();
    //存在这个人
    if(query.next()){
        int ret = query.value(0).toInt();
        if(ret == 1){
            return 1;   //此人在线
        }else if(ret == 0){
            return 0;   //此人不在线
        }
    }
    else{
        return -1;      //没这个人
    }
    return -1;
}

int OpeDB::handleAddFriend(const char *pername, const char *name)
{
    if(NULL == pername || NULL == name){
        return -1;
    }
    QMutexLocker locker(&m_mutex);

    // 获取两个用户的ID
    int id1 = getIdByUserName(pername);
    int id2 = getIdByUserName(name);

    // 检查pername是否存在
    if(id1 == -1) {
        return 3; // 没这个人
    }

    // 检查是否是好友
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM friend WHERE (id=? AND friendId=?) OR (id=? AND friendId=?)");
    query.addBindValue(id1);
    query.addBindValue(id2);
    query.addBindValue(id2);
    query.addBindValue(id1);
    query.exec();

    if(query.next())
    {
        return 0;   //双方已经是好友
    }
    else            //不是好友
    {
        // 检查pername的在线状态
        QSqlQuery onlineQuery(m_db);
        onlineQuery.prepare("SELECT online FROM userInfo WHERE name=?");
        onlineQuery.addBindValue(pername);
        onlineQuery.exec();
        if(onlineQuery.next()) {
            int onlineStatus = onlineQuery.value(0).toInt();
            if(onlineStatus == 1){
                return 1;   //此人在线
            }else{
                return 2;   //此人不在线
            }
        } else {
            return 3;   //没这个人（应该不会发生，因为前面已经检查过id1）
        }
    }
}

int OpeDB::getIdByUserName(const char *name)
{
    if(NULL == name)
    {
        return -1;
    }
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM userInfo WHERE name=?");
    query.addBindValue(name);
    query.exec();
    if(query.next())
    {
        return query.value(0).toInt();
    }
    else
    {
        return -1; // 不存在该用户
    }
}

void OpeDB::handleAddFriendAgree(const char *addedName, const char *sourceName)
{
    if(NULL == addedName || NULL == sourceName)
    {
        return;
    }
    QMutexLocker locker(&m_mutex);
    int sourceUserId = -1;
    int addedUserId = -1;
    sourceUserId = getIdByUserName(sourceName);
    addedUserId = getIdByUserName(addedName);
    qDebug() << sourceUserId << " " << addedUserId;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO friend (id, friendId) VALUES (?, ?)");
    query.addBindValue(sourceUserId);
    query.addBindValue(addedUserId);
    query.exec();
    return;
}

QStringList OpeDB::handleFlushFriend(const char *name)
{
    QStringList strFriendList;
    strFriendList.clear();
    if(NULL == name)
    {
        return strFriendList;
    }
    QMutexLocker locker(&m_mutex);

    // 获取请求用户的ID（必须在线）
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM userInfo WHERE name=? AND online=1");
    query.addBindValue(name);
    query.exec();
    int sourceId = -1; // 请求方name对应的id

    if (query.next())
    {
        sourceId = query.value(0).toInt();
    }

    // 如果用户不在线，返回空列表
    if(sourceId == -1) {
        return strFriendList;
    }

    // 查询好友列表
    query.prepare("SELECT name, online FROM userInfo WHERE id IN (SELECT id FROM friend WHERE friendId=?) OR id IN (SELECT friendId FROM friend WHERE id=?)");
    query.addBindValue(sourceId);
    query.addBindValue(sourceId);
    query.exec();

    //既要返回好友的名字，同时返回好友的在线状态
    while(query.next())
    {
        qDebug() << "yes";
        char friendName[32] = {'\0'};
        char friendOnlineStatus[4] = {'\0'};
        memcpy(friendName, query.value(0).toString().toStdString().c_str(), 32);
        memcpy(friendOnlineStatus, query.value(1).toString().toStdString().c_str(), 4);
        strFriendList.append(friendName);
        strFriendList.append(friendOnlineStatus);
        qDebug() << "好友信息 " << friendName << " " << friendOnlineStatus;
        //qDebug() << strFriendList;
    }
    return strFriendList;
}

QStringList OpeDB::handleGroupChat(const char *name)
{
    QStringList strFriendList;
    strFriendList.clear();
    if(NULL == name)
    {
        return strFriendList;
    }
    strFriendList = handleFlushFriend(name);
    strFriendList.append(name);
    strFriendList.append("1");
    return strFriendList;
}

QStringList OpeDB::getAllUsers()
{
    QMutexLocker locker(&m_mutex);
    QStringList result;
    if (!m_db.isOpen()) {
        qDebug() << "获取用户列表失败：数据库未打开";
        return result;
    }

    QSqlQuery query(m_db);
    // 在线用户在前，离线用户在后
    if (!query.exec("SELECT name, online FROM userInfo ORDER BY online DESC, name ASC")) {
        qDebug() << "获取用户列表失败：" << query.lastError().text();
        return result;
    }

    while(query.next())
    {
        result.append(query.value(0).toString());        // 用户名
        result.append(query.value(1).toString());        // 在线状态 1/0
    }
    return result;
}

bool OpeDB::handleDeleteFriend(const char *sourceName, const char *deleteName)
{
    if(NULL == sourceName || NULL == deleteName)
    {
        return false;
    }
    QMutexLocker locker(&m_mutex);
    int sourceId = getIdByUserName(sourceName);
    int deleteId = getIdByUserName(deleteName); // 请求方name对应的id
    qDebug() << sourceId << deleteId;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM friend WHERE (id=? AND friendId=?) OR (id=? AND friendId=?)");
    query.addBindValue(sourceId);
    query.addBindValue(deleteId);
    query.addBindValue(deleteId);
    query.addBindValue(sourceId);
    query.exec();
    return true;
}
