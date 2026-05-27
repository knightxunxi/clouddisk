#include "mytcpsocket.h"
#include "opedb.h"
#include "pdufieldcodec.h"
#include "storageservice.h"

#include <QDir>

#include <cstring>

void MyTcpSocket::handleRegistRequest(PDU *pdu)
{
    const PduFieldCodec::FixedPair account = PduFieldCodec::fixedPair(pdu->caData);

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;
    if (!StorageService::isSafeName(account.first)) {
        strcpy(respdu->caData, REGIST_FAILED);
        write((char*)respdu, respdu->uiPDULen);
        free(respdu); respdu = nullptr;
        return;
    }

    const QByteArray nameBytes = account.first.toUtf8();
    const QByteArray pwdBytes = account.second.toUtf8();
    bool ret = OpeDB::getInstance().handleRegist(nameBytes.constData(),
                                                 pwdBytes.constData());
    if (ret) {
        strcpy(respdu->caData, REGIST_OK);
        QDir dir;
        dir.mkpath(StorageService::userRootPath(account.first));
        emit userListChanged();
    } else {
        strcpy(respdu->caData, REGIST_FAILED);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleLoginRequest(PDU *pdu)
{
    const PduFieldCodec::FixedPair account = PduFieldCodec::fixedPair(pdu->caData);

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;
    if (!StorageService::isSafeName(account.first)) {
        strcpy(respdu->caData, LOGIN_FAILED);
        write((char*)respdu, respdu->uiPDULen);
        free(respdu); respdu = nullptr;
        return;
    }

    const QByteArray nameBytes = account.first.toUtf8();
    const QByteArray pwdBytes = account.second.toUtf8();
    bool ret = OpeDB::getInstance().handleLogin(nameBytes.constData(),
                                                pwdBytes.constData());
    if (ret) {
        strcpy(respdu->caData, LOGIN_OK);
        m_strName = account.first;
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
        PduFieldCodec::writeFixedString((char*)(respdu->caMsg) + i * 32,
                                        32,
                                        ret.at(i));
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleSearchUserRequest(PDU *pdu)
{
    const QString searchName = PduFieldCodec::fixedString(pdu->caData, 32);
    const QByteArray searchNameBytes = searchName.toUtf8();
    int ret = OpeDB::getInstance().handleSearchUser(searchNameBytes.constData());
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USER_RESPOND;
    if      (-1 == ret) strcpy(respdu->caData, SEARCH_USER_NO);
    else if ( 1 == ret) strcpy(respdu->caData, SEARCH_USER_ONLINE);
    else if ( 0 == ret) strcpy(respdu->caData, SEARCH_USER_OFFLINE);

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}
