#include "mytcpsocket.h"
#include "opedb.h"

#include <QDir>

#include <cstring>

void MyTcpSocket::handleRegistRequest(PDU *pdu)
{
    char caName[32] = {'\0'};
    char caPwd[32]  = {'\0'};
    strncpy(caName, pdu->caData,      32);
    strncpy(caPwd,  pdu->caData + 32, 32);

    bool ret = OpeDB::getInstance().handleRegist(caName, caPwd);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;
    if (ret) {
        strcpy(respdu->caData, REGIST_OK);
        QDir dir;
        dir.mkdir(QString("./%1").arg(caName));
        emit userListChanged();
    } else {
        strcpy(respdu->caData, REGIST_FAILED);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleLoginRequest(PDU *pdu)
{
    char caName[32] = {'\0'};
    char caPwd[32]  = {'\0'};
    strncpy(caName, pdu->caData,      32);
    strncpy(caPwd,  pdu->caData + 32, 32);

    bool ret = OpeDB::getInstance().handleLogin(caName, caPwd);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;
    if (ret) {
        strcpy(respdu->caData, LOGIN_OK);
        m_strName = caName;
        emit userListChanged();
    } else {
        strcpy(respdu->caData, LOGIN_FAILED);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleAllOnlineRequest(PDU *pdu)
{
    Q_UNUSED(pdu);

    QStringList ret = OpeDB::getInstance().handleAllOnline();
    unit uiMsgLenLocal = ret.size() * 32;
    PDU *respdu = mkPDU(uiMsgLenLocal);
    respdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_RESPOND;
    for (int i = 0; i < ret.size(); i++) {
        memcpy((char*)(respdu->caMsg) + i * 32,
               ret.at(i).toStdString().c_str(), ret.at(i).size());
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleSearchUserRequest(PDU *pdu)
{
    int ret = OpeDB::getInstance().handleSearchUser(pdu->caData);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USER_RESPOND;
    if      (-1 == ret) strcpy(respdu->caData, SEARCH_USER_NO);
    else if ( 1 == ret) strcpy(respdu->caData, SEARCH_USER_ONLINE);
    else if ( 0 == ret) strcpy(respdu->caData, SEARCH_USER_OFFLINE);

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}
