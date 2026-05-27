#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QWidget>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QDateTime>
#include <QVector>

class QComboBox;
class QLineEdit;

QT_BEGIN_NAMESPACE
namespace Ui {
    class tcpServer;
}

//QT_END_NAMESPACE

class tcpServer : public QWidget
{
    Q_OBJECT

public:
    explicit tcpServer(QWidget *parent = nullptr);
    ~tcpServer();
    void loadConfig();

private slots:
    // 刷新按钮点击
    void on_btnRefresh_clicked();
    // 用户状态变化（登录/下线），自动刷新列表
    void refreshUserList();
    // 点击在线用户列表中某项，展示该用户的文件树
    void onOnlineUserClicked(QListWidgetItem *item);
    // 点击离线用户列表中某项，展示该用户的文件树
    void onOfflineUserClicked(QListWidgetItem *item);
    // 双击文件树中目录节点时展开/折叠（懒加载子节点）
    void onTreeItemExpanded(QTreeWidgetItem *item);
    // 服务端 socket/handler 运行日志
    void onRuntimeLog(const QString &category, const QString &msg);
    // 实时应用日志筛选
    void applyLogFilter();

private:
    struct LogEntry {
        QDateTime time;
        QString category;
        QString message;
    };

    // 构建某个用户的文件树（递归）
    void buildFileTree(const QString &rootPath, QTreeWidgetItem *parentItem);
    // 展示指定用户的文件结构
    void showUserFiles(const QString &userName);
    // 追加一条日志
    void appendLog(const QString &msg, const QString &category = QString());
    void setupLogFilter();
    void refreshLogView();
    bool logEntryVisible(const LogEntry &entry) const;
    QString formatLogEntry(const LogEntry &entry) const;

    Ui::tcpServer *ui;
    QComboBox *m_logCategoryFilter = nullptr;
    QLineEdit *m_logKeywordFilter = nullptr;
    QVector<LogEntry> m_logEntries;

    QString m_strIP;
    quint16 m_usPort;
    QString m_strRootDir;   // 服务器文件根目录（可执行文件所在目录）
};
#endif // TCPSERVER_H
