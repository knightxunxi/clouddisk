#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QFile>
#include <QTcpSocket>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include "protocol.h"
#include "opewidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class tcpClient;
}
QT_END_NAMESPACE

// 下载工作线程（独立顶层类，避免 MOC 不支持嵌套类 Q_OBJECT 的问题）
class DownloadThread : public QThread
{
    Q_OBJECT
public:
    DownloadThread(QObject *parent = nullptr)
        : QThread(parent), m_canceled(false) {}
    void setup(const QString &filePath, qint64 offset);
    void cancel();
    void enqueueData(const QByteArray &data);
signals:
    void bytesWritten(qint64 bytes);
    void finished(bool success, const QString &msg);
protected:
    void run() override;
private:
    QString m_filePath;
    qint64 m_offset;
    bool m_canceled;
    QQueue<QByteArray> m_queue;
    QMutex m_mutex;
    QWaitCondition m_cond;
};

class tcpClient : public QWidget
{
    Q_OBJECT

public:
    tcpClient(QWidget *parent = nullptr);
    ~tcpClient();
    void loadConfig();

    static tcpClient &getInstance();

    QTcpSocket &gettcpSocket();

    QString loginName();
    QString currentPath();
    void setCurrentPath(QString preContentPath);

    // 下载文件相关
    void startDownload(const QString &filePath, qint64 offset);
    void stopDownload();

public slots:
    void showConnect();
    void recvMsg();

private slots:
   // void on_send_pb_clicked();

    void on_login_clicked();

    void on_zhuce_clicked();

    void on_zhuxiao_clicked();

    void onDownloadBytesWritten(qint64 bytes);
    void onDownloadFinished(bool success, const QString &msg);

private:
    Ui::tcpClient *ui;
    QString m_strIP;
    quint16 m_usPort;

    QTcpSocket m_tcpSocket;
    QString m_strLoginName;
    QString m_strCurPath;

    DownloadThread *m_downloadThread;
};

#endif // TCPCLIENT_H
