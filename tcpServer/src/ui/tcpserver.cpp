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
#include <QRegularExpression>
#include <QTimer>
#include <QComboBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QLabel>
#include "mytcpserver.h"
#include "opedb.h"

tcpServer::tcpServer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpServer)
{
    ui->setupUi(this);
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei";
            color: #000000;
            background-color: #f7f9fc;
        }
        QPushButton {
            padding: 6px 14px;
            border: 1px solid #b7c7d9;
            border-radius: 4px;
            background-color: #ffffff;
            color: #000000;
        }
        QPushButton:hover {
            background-color: #ecf5ff;
        }
        QPushButton:pressed {
            background-color: #d8ebff;
        }
    )");
    loadConfig();
    setupLogFilter();

    MyTcpServer::getInstance().listen(QHostAddress(m_strIP), m_usPort);

    // 连接信号：有用户登录或下线时自动刷新
    connect(&MyTcpServer::getInstance(), SIGNAL(userStatusChanged()),
            this, SLOT(refreshUserList()));
    connect(&MyTcpServer::getInstance(), SIGNAL(runtimeLog(QString,QString)),
            this, SLOT(onRuntimeLog(QString,QString)));

    // 用户列表点击事件
    connect(ui->listOnlineUsers, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onOnlineUserClicked(QListWidgetItem*)));
    connect(ui->listOfflineUsers, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onOfflineUserClicked(QListWidgetItem*)));

    // 文件树目录展开时懒加载子节点
    connect(ui->treeFileView, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(onTreeItemExpanded(QTreeWidgetItem*)));

    appendLog(QString("服务器启动 %1:%2").arg(m_strIP).arg(m_usPort), QStringLiteral("系统"));
    refreshUserList();
    QTimer::singleShot(0, this, SLOT(refreshUserList()));
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
        QString strData = QString::fromUtf8(baData);
        file.close();
        QStringList strList = strData.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (strList.size() < 2) {
            QMessageBox::critical(this, "open config", "server config format error");
            return;
        }
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
    appendLog("手动刷新用户列表", QStringLiteral("系统"));
}

void tcpServer::refreshUserList()
{
    // 清空两个列表
    ui->listOnlineUsers->clear();
    ui->listOfflineUsers->clear();

    // 从数据库获取所有用户：[name, online, name, online, ...]
    QStringList allUsers = OpeDB::getInstance().getAllUsers();
    appendLog(QString("读取数据库用户数：%1").arg(allUsers.size() / 2), QStringLiteral("系统"));

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
              .arg(onlineCnt).arg(offlineCnt), QStringLiteral("系统"));
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
        appendLog(QString("查看 %1 的文件：目录不存在 -> %2").arg(userName, userRoot),
                  QStringLiteral("文件"));
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

    appendLog(QString("查看 %1 的网盘文件结构").arg(userName), QStringLiteral("文件"));
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

void tcpServer::onRuntimeLog(const QString &category, const QString &msg)
{
    appendLog(msg, category);
}

void tcpServer::applyLogFilter()
{
    refreshLogView();
}

void tcpServer::setupLogFilter()
{
    ui->bottomBar->setMaximumHeight(180);

    m_logCategoryFilter = new QComboBox(this);
    m_logCategoryFilter->addItem(QStringLiteral("全部行为"), QString());
    m_logCategoryFilter->addItem(QStringLiteral("系统"), QStringLiteral("系统"));
    m_logCategoryFilter->addItem(QStringLiteral("连接"), QStringLiteral("连接"));
    m_logCategoryFilter->addItem(QStringLiteral("账号"), QStringLiteral("账号"));
    m_logCategoryFilter->addItem(QStringLiteral("好友"), QStringLiteral("好友"));
    m_logCategoryFilter->addItem(QStringLiteral("聊天"), QStringLiteral("聊天"));
    m_logCategoryFilter->addItem(QStringLiteral("文件"), QStringLiteral("文件"));
    m_logCategoryFilter->addItem(QStringLiteral("上传"), QStringLiteral("上传"));
    m_logCategoryFilter->addItem(QStringLiteral("下载"), QStringLiteral("下载"));
    m_logCategoryFilter->addItem(QStringLiteral("分享"), QStringLiteral("分享"));
    m_logCategoryFilter->addItem(QStringLiteral("移动"), QStringLiteral("移动"));
    m_logCategoryFilter->addItem(QStringLiteral("异常"), QStringLiteral("异常"));

    m_logKeywordFilter = new QLineEdit(this);
    m_logKeywordFilter->setPlaceholderText(QStringLiteral("按用户 / 文件 / 路径 / 动作筛选"));

    QHBoxLayout *filterLayout = new QHBoxLayout;
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->addWidget(new QLabel(QStringLiteral("行为筛选："), this));
    filterLayout->addWidget(m_logCategoryFilter);
    filterLayout->addWidget(m_logKeywordFilter, 1);

    ui->bottomLayout->insertLayout(1, filterLayout);

    connect(m_logCategoryFilter, SIGNAL(currentIndexChanged(int)),
            this, SLOT(applyLogFilter()));
    connect(m_logKeywordFilter, SIGNAL(textChanged(QString)),
            this, SLOT(applyLogFilter()));
}

void tcpServer::appendLog(const QString &msg, const QString &category)
{
    LogEntry entry;
    entry.time = QDateTime::currentDateTime();
    entry.category = category.isEmpty() ? QStringLiteral("系统") : category;
    entry.message = msg;
    m_logEntries.append(entry);

    const int maxLogEntries = 1000;
    if (m_logEntries.size() > maxLogEntries) {
        m_logEntries.remove(0, m_logEntries.size() - maxLogEntries);
        refreshLogView();
        return;
    }

    if (logEntryVisible(entry)) {
        ui->textLog->append(formatLogEntry(entry));
    }
}

void tcpServer::refreshLogView()
{
    ui->textLog->clear();
    for (const LogEntry &entry : m_logEntries) {
        if (logEntryVisible(entry)) {
            ui->textLog->append(formatLogEntry(entry));
        }
    }
}

bool tcpServer::logEntryVisible(const LogEntry &entry) const
{
    if (m_logCategoryFilter) {
        const QString category = m_logCategoryFilter->currentData().toString();
        if (!category.isEmpty() && entry.category != category) {
            return false;
        }
    }

    if (m_logKeywordFilter) {
        const QString keyword = m_logKeywordFilter->text().trimmed();
        if (!keyword.isEmpty()
                && !entry.message.contains(keyword, Qt::CaseInsensitive)
                && !entry.category.contains(keyword, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return true;
}

QString tcpServer::formatLogEntry(const LogEntry &entry) const
{
    return QString("[%1][%2] %3")
            .arg(entry.time.toString("hh:mm:ss"),
                 entry.category,
                 entry.message);
}
