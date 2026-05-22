#include "opewidget.h"
#include <QFont>

OpeWidget::OpeWidget(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("TinyDisk - 主界面"));
    resize(900, 600);
    setMinimumSize(700, 450);

    // 全局样式
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei";
        }
        /* 导航列表 */
        QListWidget#navList {
            background-color: #2c3e50;
            border: none;
            outline: none;
            min-width: 120px;
            max-width: 140px;
            font-size: 14px;
        }
        QListWidget#navList::item {
            color: #bdc3c7;
            padding: 18px 16px;
            border: none;
        }
        QListWidget#navList::item:selected {
            background-color: #34495e;
            color: #ffffff;
            border-left: 3px solid #409eff;
        }
        QListWidget#navList::item:hover:!selected {
            background-color: #34495e;
            color: #ecf0f1;
        }
        /* 好友面板 - 消息区 */
        QTextEdit {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 8px;
            font-size: 13px;
        }
        /* 好友/文件列表 */
        QListWidget {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            outline: none;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 8px 10px;
            border-bottom: 1px solid #f0f0f0;
        }
        QListWidget::item:selected {
            background-color: #ecf5ff;
            color: #409eff;
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
            font-size: 13px;
            min-height: 22px;
        }
        QLineEdit:focus {
            border-color: #409eff;
        }
        /* 通用按钮 - 主色(蓝) */
        QPushButton {
            padding: 8px 16px;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
            min-height: 18px;
            color: #ffffff;
        }
        QPushButton:hover {
            opacity: 0.85;
        }
        /* 按钮分类样式通过 objectName 区分 */
        QPushButton[btnStyle="primary"] {
            background-color: #409eff;
        }
        QPushButton[btnStyle="primary"]:hover {
            background-color: #66b1ff;
        }
        QPushButton[btnStyle="primary"]:pressed {
            background-color: #3a8ee6;
        }
        QPushButton[btnStyle="success"] {
            background-color: #67c23a;
        }
        QPushButton[btnStyle="success"]:hover {
            background-color: #85ce61;
        }
        QPushButton[btnStyle="success"]:pressed {
            background-color: #5daf34;
        }
        QPushButton[btnStyle="warning"] {
            background-color: #e6a23c;
        }
        QPushButton[btnStyle="warning"]:hover {
            background-color: #ebb563;
        }
        QPushButton[btnStyle="warning"]:pressed {
            background-color: #cf9236;
        }
        QPushButton[btnStyle="danger"] {
            background-color: #f56c6c;
        }
        QPushButton[btnStyle="danger"]:hover {
            background-color: #f78989;
        }
        QPushButton[btnStyle="danger"]:pressed {
            background-color: #dd6161;
        }
        QPushButton[btnStyle="info"] {
            background-color: #909399;
        }
        QPushButton[btnStyle="info"]:hover {
            background-color: #a6a9ad;
        }
        QPushButton[btnStyle="info"]:pressed {
            background-color: #82848a;
        }
        QPushButton[btnStyle="default"] {
            background-color: #ffffff;
            color: #606266;
            border: 1px solid #dcdfe6;
        }
        QPushButton[btnStyle="default"]:hover {
            background-color: #ecf5ff;
            color: #409eff;
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

    // 导航列表
    m_pListW = new QListWidget(this);
    m_pListW->setObjectName("navList");

    QListWidgetItem *friendItem = new QListWidgetItem(QStringLiteral("  好友"));
    friendItem->setSizeHint(QSize(0, 50));
    m_pListW->addItem(friendItem);

    QListWidgetItem *fileItem = new QListWidgetItem(QStringLiteral("  文件"));
    fileItem->setSizeHint(QSize(0, 50));
    m_pListW->addItem(fileItem);

    m_pFriend = new Friend;
    m_pBook = new Book;

    m_pSW = new QStackedWidget;
    m_pSW->addWidget(m_pFriend);
    m_pSW->addWidget(m_pBook);

    QHBoxLayout *pMain = new QHBoxLayout;
    pMain->setSpacing(0);
    pMain->setContentsMargins(0, 0, 0, 0);
    pMain->addWidget(m_pListW);
    pMain->addWidget(m_pSW, 1);
    setLayout(pMain);

    connect(m_pListW, SIGNAL(currentRowChanged(int)), m_pSW, SLOT(setCurrentIndex(int)));
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
