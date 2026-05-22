#include "sharefile.h"
#include <QDebug>
#include "tcpclient.h"
#include "opewidget.h"
#include <QLabel>

ShareFile::ShareFile(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("分享文件"));
    setMinimumSize(350, 400);

    m_pSelectAllPB = new QPushButton(QStringLiteral("全选"));
    m_pSelectAllPB->setProperty("btnStyle", "primary");

    m_pCancelSelectPB = new QPushButton(QStringLiteral("取消选择"));
    m_pCancelSelectPB->setProperty("btnStyle", "default");

    m_pConfirmPB = new QPushButton(QStringLiteral("确定分享"));
    m_pConfirmPB->setProperty("btnStyle", "success");

    m_pCancelPB = new QPushButton(QStringLiteral("取消"));
    m_pCancelPB->setProperty("btnStyle", "default");

    m_pSA = new QScrollArea;
    m_pFriendW = new QWidget;
    m_pButtonGroup = new  QButtonGroup(m_pFriendW);
    m_pButtonGroup->setExclusive(false);
    m_pFriendWVBL = new QVBoxLayout(m_pFriendW);
    m_pFriendWVBL->setSpacing(6);
    m_pFriendWVBL->setContentsMargins(8, 8, 8, 8);

    // 顶部标题+按钮
    QHBoxLayout *pTopHBL = new QHBoxLayout;
    pTopHBL->setSpacing(10);
    QLabel *pTitleLabel = new QLabel(QStringLiteral("选择分享对象："));
    pTitleLabel->setStyleSheet("color: #2c3e50; font-size: 14px; font-weight: bold;");
    pTopHBL->addWidget(pTitleLabel);
    pTopHBL->addStretch();
    pTopHBL->addWidget(m_pSelectAllPB);
    pTopHBL->addWidget(m_pCancelSelectPB);

    // 底部确认栏
    QHBoxLayout *pDownHBL = new QHBoxLayout;
    pDownHBL->setSpacing(10);
    pDownHBL->addStretch();
    pDownHBL->addWidget(m_pConfirmPB);
    pDownHBL->addWidget(m_pCancelPB);

    QVBoxLayout *pMainVBL = new QVBoxLayout;
    pMainVBL->setSpacing(10);
    pMainVBL->setContentsMargins(16, 16, 16, 16);
    pMainVBL->addLayout(pTopHBL);
    pMainVBL->addWidget(m_pSA, 1);
    pMainVBL->addLayout(pDownHBL);
    setLayout(pMainVBL);

    connect(m_pCancelSelectPB, SIGNAL(clicked(bool)), this, SLOT(cancelSelect()));
    connect(m_pSelectAllPB, SIGNAL(clicked(bool)), this, SLOT(selectAll()));
    connect(m_pConfirmPB, SIGNAL(clicked(bool)), this, SLOT(shareConfirm()));
    connect(m_pCancelPB, SIGNAL(clicked(bool)), this, SLOT(shareCancel()));
}

ShareFile &ShareFile::getInstance()
{
    static ShareFile instance;
    return instance;
}

void ShareFile::updateFriend(QListWidget *pFriendList)
{
    if(NULL == pFriendList)
    {
        return;
    }
    QAbstractButton *tmp;
    QList<QAbstractButton*> preFriendList =  m_pButtonGroup->buttons();
    for(int i = 0; i < preFriendList.size(); i++)
    {
        tmp = preFriendList[i];
        m_pFriendWVBL->removeWidget(tmp);
        m_pButtonGroup->removeButton(tmp);
        preFriendList.removeOne(tmp);
        delete tmp;
        tmp = NULL;
    }
    QCheckBox *pCB;
    for (int i = 0; i < pFriendList->count(); i++)
    {
        qDebug() << "好友名字" << pFriendList->item(i)->text();
        pCB = new QCheckBox(pFriendList->item(i)->text());
        m_pFriendWVBL->addWidget(pCB);
        m_pButtonGroup->addButton(pCB);

    }
    m_pSA->setWidget(m_pFriendW);


}

void ShareFile::cancelSelect()
{
    QList<QAbstractButton*> cbList =  m_pButtonGroup->buttons();
    for(int i = 0; i < cbList.size(); i++)
    {
        if(cbList[i]->isChecked())
        {
            cbList[i]->setChecked(false);
        }
    }
}

void ShareFile::selectAll()
{
    QList<QAbstractButton*> cbList =  m_pButtonGroup->buttons();
    for(int i = 0; i < cbList.size(); i++)
    {
        if(!cbList[i]->isChecked())
        {
            cbList[i]->setChecked(true);
        }
    }
}

void ShareFile::shareConfirm()
{
    QString strName = tcpClient::getInstance().loginName();
    QString strCurPath = tcpClient::getInstance().currentPath();
    QString shareFileName = OpeWidget::getInstance().getBook()->getShareFileName();

    QString strSharePath = strCurPath + "/" + shareFileName; //完整的文件路径
    QList<QAbstractButton*> cbList =  m_pButtonGroup->buttons();
    int shareNum = 0;
    for(int i = 0; i < cbList.size(); i++)
    {
        if(cbList[i]->isChecked())
        {
            shareNum++;
        }
    }
    PDU *pdu = mkPDU(32 * shareNum + strSharePath.size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_REQUEST;
    sprintf(pdu->caData, "%s %d", strName.toStdString().c_str(), shareNum);
    int j = 0;
    for(int i = 0; i < cbList.size(); i++)
    {
        if(cbList[i]->isChecked())
        {
             qDebug() << "选中分享的好友：" << cbList[i]->text().split('\t')[0].toStdString().c_str();
             memcpy((char*)(pdu->caMsg) + j * 32, cbList[i]->text().split('\t')[0].toStdString().c_str(), cbList[i]->text().split('\t')[0].size());
             j++;
        }
    }
    memcpy((char*)(pdu->caMsg) + shareNum * 32, strSharePath.toStdString().c_str(), strSharePath.size());
    tcpClient::getInstance().gettcpSocket().write((char*)pdu, pdu->uiPDULen);
    free(pdu);
    pdu = NULL;
    hide();
}

void ShareFile::shareCancel()
{
    hide();
}
