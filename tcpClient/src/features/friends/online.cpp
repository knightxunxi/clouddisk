#include "online.h"
#include "ui_online.h"
#include <QDebug>
#include "pdufieldcodec.h"
#include "tcpclient.h"
#include <QMessageBox>

Online::Online(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Online)
{
    ui->setupUi(this);
}

Online::~Online()
{
    delete ui;
}

void Online::showUser(PDU *pdu)
{
    if(NULL == pdu){
        return;
    }
    unit uiSize = pdu->uiMsgLen / 32;
    for(unit i = 0; i < uiSize; i++){
        const QString caTmp = PduFieldCodec::fixedString((char*)(pdu->caMsg) + i * 32, 32);
        // 补充：不显示自己信息，防止之后添加自己为好友等操作错误
        if(caTmp == tcpClient::getInstance().loginName())
        {
            continue;
        }
        ui->online_lw->addItem(caTmp);
    }
}

void Online::on_addFriend_pb_clicked()
{
    QListWidgetItem *pItem = ui->online_lw->currentItem();
    if(NULL == pItem)
    {
        QMessageBox::warning(this, "添加好友", "请选择要添加的好友");
        return;
    }
    QString strPerUserName = pItem->text();
    QString strLoginName = tcpClient::getInstance().loginName();
    PDU *pdu = mkPDU(0);
    qDebug() << "on_addfriend_pb_clicked  " << strPerUserName << " " << strLoginName;
    pdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REQUEST;
    PduFieldCodec::writeFixedPair(pdu->caData, strPerUserName, strLoginName);
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;

}
