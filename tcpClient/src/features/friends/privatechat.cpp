#include "privatechat.h"
#include "ui_privatechat.h"
#include "pdufieldcodec.h"
#include "protocol.h"
#include "tcpclient.h"
#include <QMessageBox>


PrivateChat::PrivateChat(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrivateChat)
{
    ui->setupUi(this);
}

PrivateChat::~PrivateChat()
{
    delete ui;
}

PrivateChat &PrivateChat::getInstance()
{
    static PrivateChat instance;
    return instance;
}

void PrivateChat::setChatName(QString name)
{
    m_strChatName = name;
    m_strLoginName = tcpClient::getInstance().loginName();

}

//
void PrivateChat::updateMsg(const PDU *pdu)
{
    if(NULL == pdu)
    {
        return;
    }
    QString strMsg = QString("%1 says: %2")
            .arg(PduFieldCodec::fixedString(pdu->caData, 32))
            .arg(PduFieldCodec::messageString(pdu));
    ui->showMsg_te->append(strMsg);
}

void PrivateChat::on_sendMsg_pb_clicked()
{
    QString strMsg = ui->inputMsg_le->text();
    ui->inputMsg_le->clear();
    if( strMsg.isEmpty())
    {
        QMessageBox::warning(this, "私聊", "聊天信息不能为空");
    }
    else
    {
        PDU *pdu = mkPDU(strMsg.toUtf8().size() + 1);
        pdu->uiMsgType = ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST;

        PduFieldCodec::writeFixedPair(pdu->caData, m_strLoginName, m_strChatName);
        PduFieldCodec::writeMessage(pdu, strMsg);
        tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
        free(pdu);
        pdu = NULL;

    }
}
