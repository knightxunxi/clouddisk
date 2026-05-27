#include "opewidget.h"
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>

OpeWidget::OpeWidget(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("TinyDisk - 主界面"));
    resize(900, 600);
    setMinimumSize(700, 450);

    // 全局样式
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei";
            color: #000000;
        }
        QWidget#navPanel {
            background-color: #ffffff;
            border-right: 1px solid #dcdfe6;
        }
        QLabel#userLabel {
            background-color: #f5f7fa;
            border-bottom: 1px solid #dcdfe6;
            color: #000000;
            font-size: 13px;
            font-weight: bold;
            padding: 12px 10px;
        }
        /* 导航列表 */
        QListWidget#navList {
            background-color: #ffffff;
            border: none;
            outline: none;
            min-width: 120px;
            max-width: 140px;
            font-size: 14px;
        }
        QListWidget#navList::item {
            color: #000000;
            padding: 18px 16px;
            border: none;
        }
        QListWidget#navList::item:selected {
            background-color: #d8ebff;
            color: #000000;
            border-left: 3px solid #409eff;
        }
        QListWidget#navList::item:hover:!selected {
            background-color: #f0f7ff;
            color: #000000;
        }
        /* 好友面板 - 消息区 */
        QTextEdit {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 8px;
            font-size: 13px;
        }
        /* 好友/文件列表 */
        QListWidget {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            outline: none;
            font-size: 13px;
        }
        QListWidget::item {
            color: #000000;
            padding: 8px 10px;
            border-bottom: 1px solid #f0f0f0;
        }
        QListWidget::item:selected {
            background-color: #ecf5ff;
            color: #000000;
        }
        QListWidget::item:hover:!selected {
            background-color: #f5f7fa;
        }
        /* 输入框 */
        QLineEdit {
            padding: 8px 12px;
            border: 2px solid #dcdfe6;
            border-radius: 6px;
            background-color: #ffffff;
            color: #000000;
            placeholder-text-color: #606266;
            font-size: 13px;
            min-height: 22px;
        }
        QLineEdit:focus {
            border-color: #409eff;
        }
        /* 通用按钮 - 主色(蓝) */
        QPushButton {
            padding: 8px 16px;
            border: 1px solid #b7c7d9;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
            min-height: 18px;
            color: #000000;
        }
        QPushButton:hover {
            opacity: 0.85;
        }
        /* 按钮分类样式通过 objectName 区分 */
        QPushButton[btnStyle="primary"] {
            background-color: #d8ebff;
        }
        QPushButton[btnStyle="primary"]:hover {
            background-color: #c6e2ff;
        }
        QPushButton[btnStyle="primary"]:pressed {
            background-color: #b3d8ff;
        }
        QPushButton[btnStyle="success"] {
            background-color: #e1f3d8;
        }
        QPushButton[btnStyle="success"]:hover {
            background-color: #d4edc9;
        }
        QPushButton[btnStyle="success"]:pressed {
            background-color: #c2e7b0;
        }
        QPushButton[btnStyle="warning"] {
            background-color: #faecd8;
        }
        QPushButton[btnStyle="warning"]:hover {
            background-color: #f8dfb5;
        }
        QPushButton[btnStyle="warning"]:pressed {
            background-color: #f3d19e;
        }
        QPushButton[btnStyle="danger"] {
            background-color: #fde2e2;
        }
        QPushButton[btnStyle="danger"]:hover {
            background-color: #fcd3d3;
        }
        QPushButton[btnStyle="danger"]:pressed {
            background-color: #fab6b6;
        }
        QPushButton[btnStyle="info"] {
            background-color: #f0f2f5;
        }
        QPushButton[btnStyle="info"]:hover {
            background-color: #e4e7ed;
        }
        QPushButton[btnStyle="info"]:pressed {
            background-color: #dcdfe6;
        }
        QPushButton[btnStyle="default"] {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #dcdfe6;
        }
        QPushButton[btnStyle="default"]:hover {
            background-color: #ecf5ff;
            color: #000000;
            border-color: #c6e2ff;
        }
        QPushButton[btnStyle="default"]:pressed {
            background-color: #d9ecff;
        }
        /* QScrollArea */
        QScrollArea {
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            background-color: #ffffff;
        }
        /* QCheckBox */
        QCheckBox {
            font-size: 13px;
            spacing: 8px;
            padding: 6px;
            color: #000000;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 2px solid #dcdfe6;
            background-color: #ffffff;
        }
        QCheckBox::indicator:checked {
            background-color: #409eff;
            border-color: #409eff;
        }
    )");

    QWidget *pNavPanel = new QWidget(this);
    pNavPanel->setObjectName("navPanel");
    pNavPanel->setFixedWidth(140);

    m_pUserLabel = new QLabel(QStringLiteral("当前用户：\n未登录"), pNavPanel);
    m_pUserLabel->setObjectName("userLabel");
    m_pUserLabel->setWordWrap(true);
    m_pUserLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 导航列表
    m_pListW = new QListWidget(pNavPanel);
    m_pListW->setObjectName("navList");

    QListWidgetItem *friendItem = new QListWidgetItem(QStringLiteral("  好友"));
    friendItem->setSizeHint(QSize(0, 50));
    m_pListW->addItem(friendItem);

    QListWidgetItem *fileItem = new QListWidgetItem(QStringLiteral("  文件"));
    fileItem->setSizeHint(QSize(0, 50));
    m_pListW->addItem(fileItem);

    QVBoxLayout *pNavLayout = new QVBoxLayout;
    pNavLayout->setSpacing(0);
    pNavLayout->setContentsMargins(0, 0, 0, 0);
    pNavLayout->addWidget(m_pUserLabel);
    pNavLayout->addWidget(m_pListW, 1);
    pNavPanel->setLayout(pNavLayout);

    m_pFriend = new Friend;
    m_pBook = new Book;

    m_pSW = new QStackedWidget;
    m_pSW->addWidget(m_pFriend);
    m_pSW->addWidget(m_pBook);

    QHBoxLayout *pMain = new QHBoxLayout;
    pMain->setSpacing(0);
    pMain->setContentsMargins(0, 0, 0, 0);
    pMain->addWidget(pNavPanel);
    pMain->addWidget(m_pSW, 1);
    setLayout(pMain);

    connect(m_pListW, SIGNAL(currentRowChanged(int)), this, SLOT(onNavChanged(int)));
}

OpeWidget &OpeWidget::getInstance()
{
    static OpeWidget instance;
    return instance;
}

Friend *OpeWidget::getFriend()
{
    return m_pFriend;
}

Book *OpeWidget::getBook()
{
    return m_pBook;
}

void OpeWidget::setCurrentUser(const QString &userName)
{
    QString displayName = userName.trimmed();
    if (displayName.isEmpty()) {
        displayName = QStringLiteral("未登录");
    }

    m_pUserLabel->setText(QStringLiteral("当前用户：\n%1").arg(displayName));
    setWindowTitle(QStringLiteral("TinyDisk - %1").arg(displayName));
}

void OpeWidget::showFilePage()
{
    if (m_pListW->currentRow() != 1) {
        m_pListW->setCurrentRow(1);
    } else {
        onNavChanged(1);
    }
}

void OpeWidget::onNavChanged(int row)
{
    if (row < 0 || row >= m_pSW->count()) {
        return;
    }

    m_pSW->setCurrentIndex(row);
    if (row == 1) {
        m_pBook->flushFile();
    }
}
