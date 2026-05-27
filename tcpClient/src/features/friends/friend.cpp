#include "friend.h"
#include "pdufieldcodec.h"
#include "protocol.h"
#include "tcpclient.h"
#include <QInputDialog>
#include <QMessageBox>
#include "privatechat.h"
#include <QLabel>


Friend::Friend(QWidget *parent) : QWidget(parent)
{
    m_pShowMsgTE = new QTextEdit;     // 显示群发信息
    m_pShowMsgTE->setReadOnly(true);
    m_pShowMsgTE->setPlaceholderText(QStringLiteral("群聊消息将在这里显示..."));

    m_pFriendListWidget = new QListWidget;         // 好友列表

    m_pInputMsgLE = new QLineEdit;       // 群聊信息输入框
    m_pInputMsgLE->setPlaceholderText(QStringLiteral("输入群聊消息..."));

    m_pDelFriendPB = new QPushButton(QStringLiteral("删除好友"));      // 删除好友
    m_pDelFriendPB->setProperty("btnStyle", "danger");

    m_pFlushFriendPB = new QPushButton(QStringLiteral("刷新好友"));    // 刷新好友列表
    m_pFlushFriendPB->setProperty("btnStyle", "primary");

    m_pShowOnlineUserPB = new QPushButton(QStringLiteral("在线用户")); // 显示所有在线用户
    m_pShowOnlineUserPB->setProperty("btnStyle", "default");

    m_pSearchUserPB = new QPushButton(QStringLiteral("查找用户"));     // 查找用户
    m_pSearchUserPB->setProperty("btnStyle", "default");

    m_pMsgSendPB = new QPushButton(QStringLiteral("发 送"));   // 群聊发送消息
    m_pMsgSendPB->setProperty("btnStyle", "primary");

    m_pPrivateChatPB = new QPushButton(QStringLiteral("私聊"));    // 私聊按钮，默认群聊
    m_pPrivateChatPB->setProperty("btnStyle", "success");

    // 左侧按钮区：带分组标签
    QVBoxLayout *pLeftPBVBL = new QVBoxLayout;
    pLeftPBVBL->setSpacing(8);

    QLabel *pActionLabel = new QLabel(QStringLiteral("操作"));
    pActionLabel->setStyleSheet("color: #000000; font-size: 11px; font-weight: bold; padding-left: 2px;");
    pLeftPBVBL->addWidget(pActionLabel);
    pLeftPBVBL->addWidget(m_pFlushFriendPB);
    pLeftPBVBL->addWidget(m_pShowOnlineUserPB);
    pLeftPBVBL->addWidget(m_pSearchUserPB);
    pLeftPBVBL->addWidget(m_pPrivateChatPB);
    pLeftPBVBL->addWidget(m_pDelFriendPB);
    pLeftPBVBL->addStretch();

    // 上部区域
    QHBoxLayout *pTopHBL = new QHBoxLayout;
    pTopHBL->setSpacing(10);
    pTopHBL->addWidget(m_pShowMsgTE, 3);
    pTopHBL->addWidget(m_pFriendListWidget, 2);
    pTopHBL->addLayout(pLeftPBVBL);

    // 底部输入行
    QHBoxLayout *pMsgHBL = new QHBoxLayout;
    pMsgHBL->setSpacing(10);
    pMsgHBL->addWidget(m_pInputMsgLE, 1);
    pMsgHBL->addWidget(m_pMsgSendPB);

    m_pOnline = new Online;

    QVBoxLayout *pMain = new QVBoxLayout;
    pMain->setSpacing(10);
    pMain->setContentsMargins(12, 12, 12, 12);
    pMain->addLayout(pTopHBL, 1);
    pMain->addLayout(pMsgHBL);
    pMain->addWidget(m_pOnline);
    m_pOnline->hide();

    setLayout(pMain);

    connect(m_pShowOnlineUserPB, SIGNAL(clicked(bool)), this, SLOT(showOnline()));
    connect(m_pSearchUserPB, SIGNAL(clicked(bool)), this, SLOT(searchUser()));
    connect(m_pFlushFriendPB, SIGNAL(clicked(bool)), this, SLOT(flushFriend()));
    connect(m_pDelFriendPB, SIGNAL(clicked(bool)), this, SLOT(deleteFriend()));
    connect(m_pPrivateChatPB, SIGNAL(clicked(bool)), this, SLOT(privateChat()));
    connect(m_pMsgSendPB, SIGNAL(clicked(bool)), this, SLOT(groupChat()));
}

void Friend::showAllOnlineUser(PDU *pdu)
{
    if(NULL == pdu){
        return;
    }
    m_pOnline->showUser(pdu);
}

void Friend::updateFriendList(PDU *pdu)
{
    if(NULL == pdu)
    {
        return;
    }
    unit strSize = pdu->uiMsgLen / 36;

    m_pFriendListWidget->clear();
    for(unit i = 0; i < strSize ; i++)
    {
        const QString caName = PduFieldCodec::fixedString((char*)(pdu->caMsg) + i * 36, 32);
        const QString onlineStatus = PduFieldCodec::fixedString((char*)(pdu->caMsg) + 32 + i * 36, 4);
        qDebug() << "客户端好友" << caName << " " << onlineStatus;
        m_pFriendListWidget->addItem(QString("%1\t%2").arg(caName)
                                     .arg(onlineStatus == "1" ? "在线" : "离线"));
    }
    return;
}

void Friend::updateGroupMsg(PDU *pdu)
{
    if(NULL == pdu)
    {
        return;
    }
    QString strMsg = QString("%1 says: %2")
            .arg(PduFieldCodec::fixedString(pdu->caData, 32))
            .arg(PduFieldCodec::messageString(pdu));
    m_pShowMsgTE->append(strMsg);
}

QListWidget *Friend::getFriendList()
{
    return m_pFriendListWidget;
}

void Friend::showOnline()
{
    if(m_pOnline->isHidden()){
        m_pOnline->show();
        PDU *pdu = mkPDU(0);
        pdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_REQUEST;
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
    else{
        m_pOnline->hide();
    }
}

void Friend::searchUser()
{
    m_strSearchName = QInputDialog::getText(this, "搜索", "用户名：");
    if(!m_strSearchName.isEmpty()){
        PDU *pdu = mkPDU(0);
        PduFieldCodec::writeFixedString(pdu->caData, 32, m_strSearchName);
        pdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USER_REQUEST;
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;
    }
}

void Friend::flushFriend()
{
    QString strName = tcpClient::getInstance().loginName();
    PDU *pdu = mkPDU(0);
    pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST;
    PduFieldCodec::writeFixedString(pdu->caData, 32, strName);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Friend::deleteFriend()
{
    QListWidgetItem *deleteItem = m_pFriendListWidget->currentItem();
    if(NULL == deleteItem)
    {
        QMessageBox::warning(this, "删除好友", "请选择要删除的好友");
        return;
    }
    QString deleteName = deleteItem->text().split("\t")[0];
    QString sourceName = tcpClient::getInstance().loginName(); // 登录用户用户名
    PDU *pdu = mkPDU(0);
    pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST;
    PduFieldCodec::writeFixedPair(pdu->caData, sourceName, deleteName);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}

void Friend::privateChat()
{
    QListWidgetItem *privateChatItem = m_pFriendListWidget->currentItem();
    if(NULL == privateChatItem)
    {
        QMessageBox::warning(this, "私聊好友", "请选择要私聊的好友");
        return;
    }
    QString privateChatName = privateChatItem->text().split("\t")[0];
    PrivateChat::getInstance().setChatName(privateChatName);
    if(PrivateChat::getInstance().isHidden())
    {
        PrivateChat::getInstance().show();
    }

}

void Friend::groupChat()
{
    QString strMsg = m_pInputMsgLE->text();
    if(strMsg.isEmpty())
    {
        QMessageBox::warning(this, "好友群发", "信息不能为空");
        return;
    }
    PDU *pdu = mkPDU(strMsg.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_GROUP_CHAT_REQUEST;
    QString sourceName = tcpClient::getInstance().loginName();
    PduFieldCodec::writeFixedString(pdu->caData, 32, sourceName);
    PduFieldCodec::writeMessage(pdu, strMsg);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
}
