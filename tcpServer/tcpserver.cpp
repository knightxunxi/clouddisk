#include "tcpserver.h"
#include "ui_tcpserver.h"
#include <QByteArray>
#include <QDebug>
#include <QMessageBox>
#include <QHostAddress>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDateTime>
#include <QStyle>
#include <QBrush>
#include "mytcpserver.h"
#include "opedb.h"

tcpServer::tcpServer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpServer)
{
    ui->setupUi(this);
    loadConfig();

    MyTcpServer::getInstance().listen(QHostAddress(m_strIP), m_usPort);

    // 启动时刷新一次用户列表
    refreshUserList();

    // 连接信号：有用户登录或下线时自动刷新
    connect(&MyTcpServer::getInstance(), SIGNAL(userStatusChanged()),
            this, SLOT(refreshUserList()));

    // 用户列表点击事件
    connect(ui->listOnlineUsers, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onOnlineUserClicked(QListWidgetItem*)));
    connect(ui->listOfflineUsers, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onOfflineUserClicked(QListWidgetItem*)));

    // 文件树目录展开时懒加载子节点
    connect(ui->treeFileView, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(onTreeItemExpanded(QTreeWidgetItem*)));

    // 刷新按钮
    connect(ui->btnRefresh, SIGNAL(clicked()), this, SLOT(on_btnRefresh_clicked()));

    appendLog(QString("服务器启动 %1:%2").arg(m_strIP).arg(m_usPort));
}

tcpServer::~tcpServer()
{
    delete ui;
}

void tcpServer::loadConfig()
{
    QFile file(":/server.config");
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray baData = file.readAll();
        QString strData = baData.toStdString().c_str();
        file.close();
        strData.replace("\r\n"," ");
        QStringList strList =  strData.split(" ");
        m_strIP = strList.at(0);
        m_usPort = strList.at(1).toUShort();
        qDebug() << "IP:" << m_strIP << "port:" << m_usPort;
    }
    else
    {
        QMessageBox::critical(this, "open config", "open config failed");
    }

    // 服务器根目录为可执行文件所在目录
    m_strRootDir = QDir::currentPath();
}

void tcpServer::on_btnRefresh_clicked()
{
    refreshUserList();
    appendLog("手动刷新用户列表");
}

void tcpServer::refreshUserList()
{
    // 清空两个列表
    ui->listOnlineUsers->clear();
    ui->listOfflineUsers->clear();

    // 从数据库获取所有用户：[name, online, name, online, ...]
    QStringList allUsers = OpeDB::getInstance().getAllUsers();

    int onlineCnt  = 0;
    int offlineCnt = 0;

    for(int i = 0; i + 1 < allUsers.size(); i += 2)
    {
        QString name   = allUsers.at(i);
        int     online = allUsers.at(i + 1).toInt();

        if(online == 1)
        {
            QListWidgetItem *item = new QListWidgetItem(
                QIcon(), QString("🟢  %1").arg(name));
            item->setData(Qt::UserRole, name);   // 存储纯用户名
            ui->listOnlineUsers->addItem(item);
            ++onlineCnt;
        }
        else
        {
            QListWidgetItem *item = new QListWidgetItem(
                QIcon(), QString("⚫  %1").arg(name));
            item->setData(Qt::UserRole, name);
            ui->listOfflineUsers->addItem(item);
            ++offlineCnt;
        }
    }

    // 更新标题标签
    ui->labelOnline->setText(QString("在线用户 (%1)").arg(onlineCnt));
    ui->labelOffline->setText(QString("离线用户 (%1)").arg(offlineCnt));

    appendLog(QString("用户列表已刷新：在线 %1 人，离线 %2 人")
              .arg(onlineCnt).arg(offlineCnt));
}

void tcpServer::onOnlineUserClicked(QListWidgetItem *item)
{
    if(!item) return;
    QString userName = item->data(Qt::UserRole).toString();
    showUserFiles(userName);
    // 取消离线列表的选中
    ui->listOfflineUsers->clearSelection();
}

void tcpServer::onOfflineUserClicked(QListWidgetItem *item)
{
    if(!item) return;
    QString userName = item->data(Qt::UserRole).toString();
    showUserFiles(userName);
    // 取消在线列表的选中
    ui->listOnlineUsers->clearSelection();
}

void tcpServer::showUserFiles(const QString &userName)
{
    ui->treeFileView->clear();

    // 每个用户的网盘根目录为 ./用户名/
    QString userRoot = QString("%1/%2").arg(m_strRootDir, userName);
    QDir dir(userRoot);

    if(!dir.exists())
    {
        ui->labelFilePathTip->setText(QString("路径：%1  （目录不存在）").arg(userRoot));
        QTreeWidgetItem *tip = new QTreeWidgetItem(ui->treeFileView);
        tip->setText(0, "（该用户暂无网盘数据）");
        tip->setForeground(0, QBrush(Qt::gray));
        appendLog(QString("查看 %1 的文件：目录不存在 -> %2").arg(userName, userRoot));
        return;
    }

    ui->labelFilePathTip->setText(QString("路径：%1").arg(userRoot));

    // 创建根节点
    QTreeWidgetItem *root = new QTreeWidgetItem(ui->treeFileView);
    root->setText(0, userName);
    root->setText(1, "根目录");
    root->setText(2, "—");
    root->setData(0, Qt::UserRole, userRoot);   // 保存完整路径
    root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

    // 递归构建文件树（只展开第一层，下层懒加载）
    buildFileTree(userRoot, root);
    root->setExpanded(true);

    appendLog(QString("查看 %1 的网盘文件结构").arg(userName));
}

void tcpServer::buildFileTree(const QString &dirPath, QTreeWidgetItem *parentItem)
{
    QDir dir(dirPath);
    // 先目录，后文件
    QFileInfoList fileInfoList = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);

    for(const QFileInfo &fi : fileInfoList)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, fi.fileName());
        item->setData(0, Qt::UserRole, fi.absoluteFilePath());

        if(fi.isDir())
        {
            item->setText(1, "文件夹");
            item->setText(2, "—");
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

            // 如果目录非空，添加一个占位子节点以显示展开箭头
            QDir subDir(fi.absoluteFilePath());
            if(!subDir.entryInfoList(QDir::Dirs | QDir::Files |
                                      QDir::NoDotAndDotDot).isEmpty())
            {
                QTreeWidgetItem *placeholder = new QTreeWidgetItem(item);
                placeholder->setText(0, "...");
                placeholder->setData(0, Qt::UserRole, QString("__placeholder__"));
            }
        }
        else
        {
            item->setText(1, "文件");
            // 格式化文件大小
            qint64 sz = fi.size();
            QString sizeStr;
            if(sz < 1024)
                sizeStr = QString("%1 B").arg(sz);
            else if(sz < 1024 * 1024)
                sizeStr = QString("%1 KB").arg(sz / 1024.0, 0, 'f', 1);
            else if(sz < 1024 * 1024 * 1024)
                sizeStr = QString("%1 MB").arg(sz / (1024.0 * 1024.0), 0, 'f', 2);
            else
                sizeStr = QString("%1 GB").arg(sz / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
            item->setText(2, sizeStr);
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

void tcpServer::onTreeItemExpanded(QTreeWidgetItem *item)
{
    if(!item) return;

    // 如果只有一个占位子节点，说明需要懒加载
    if(item->childCount() == 1 &&
       item->child(0)->data(0, Qt::UserRole).toString() == "__placeholder__")
    {
        // 删除占位节点
        delete item->takeChild(0);

        // 获取该节点对应的目录路径，重新构建子节点
        QString dirPath = item->data(0, Qt::UserRole).toString();
        buildFileTree(dirPath, item);
    }
}

void tcpServer::appendLog(const QString &msg)
{
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->append(QString("[%1] %2").arg(timeStr, msg));
}
