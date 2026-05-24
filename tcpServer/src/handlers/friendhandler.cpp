#include "mytcpsocket.h"
#include "mytcpserver.h"
#include "opedb.h"

#include <cstring>

void MyTcpSocket::handleAddFriendRequest(PDU *pdu)
{
    char caPerName[32] = {'\0'};
    char caName[32]    = {'\0'};
    strncpy(caPerName, pdu->caData,      32);
    strncpy(caName,    pdu->caData + 32, 32);

    int ret = OpeDB::getInstance().handleAddFriend(caPerName, caName);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
    if (-1 == ret) {
        strcpy(respdu->caData, UNKNOWN_ERROR);
        write((char*)respdu, respdu->uiPDULen);
    } else if (0 == ret) {
        strcpy(respdu->caData, EXISTED_FRIEND);
        write((char*)respdu, respdu->uiPDULen);
    } else if (1 == ret) {
        MyTcpServer::getInstance().resend(caPerName, pdu);
        strcpy(respdu->caData, ADD_FRIEND_OK);
        write((char*)respdu, respdu->uiPDULen);
    } else if (2 == ret) {
        strcpy(respdu->caData, ADD_FRIEND_OFFLINE);
        write((char*)respdu, respdu->uiPDULen);
    } else if (3 == ret) {
        strcpy(respdu->caData, ADD_FRIEND_NO_EXISTED);
        write((char*)respdu, respdu->uiPDULen);
    }

    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleAddFriendAgree(PDU *pdu)
{
    char addedName[32]  = {'\0'};
    char sourceName[32] = {'\0'};
    strncpy(addedName,  pdu->caData,      32);
    strncpy(sourceName, pdu->caData + 32, 32);

    OpeDB::getInstance().handleAddFriendAgree(addedName, sourceName);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE;
    MyTcpServer::getInstance().resend(sourceName, pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleAddFriendRefuse(PDU *pdu)
{
    char sourceName[32] = {'\0'};
    strncpy(sourceName, pdu->caData + 32, 32);

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;
    MyTcpServer::getInstance().resend(sourceName, pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleFlushFriendRequest(PDU *pdu)
{
    char sourceName[32] = {'\0'};
    strncpy(sourceName, pdu->caData, 32);

    QStringList strList = OpeDB::getInstance().handleFlushFriend(sourceName);
    unit uiMsgLenLocal = strList.size() / 2 * 36;
    PDU *respdu = mkPDU(uiMsgLenLocal);
    respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
    for (int i = 0; i * 2 < strList.size(); i++) {
        memcpy((char*)(respdu->caMsg) + 36 * i,
               strList.at(i*2).toStdString().c_str(), 32);
        memcpy((char*)(respdu->caMsg) + 36 * i + 32,
               strList.at(i*2+1).toStdString().c_str(), 4);
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleDeleteFriendRequest(PDU *pdu)
{
    char sourceName[32] = {'\0'};
    char deleteName[32] = {'\0'};
    strncpy(sourceName, pdu->caData,      32);
    strncpy(deleteName, pdu->caData + 32, 32);

    bool ret = OpeDB::getInstance().handleDeleteFriend(sourceName, deleteName);
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
    strcpy(respdu->caData, ret ? DELETE_OK : DELETE_FAILED);
    MyTcpServer::getInstance().resend(deleteName, pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handlePrivateChatRequest(PDU *pdu)
{
    char sourceName[32] = {'\0'};
    char chatName[32]   = {'\0'};
    strncpy(sourceName, pdu->caData,      32);
    strncpy(chatName,   pdu->caData + 32, 32);

    MyTcpServer::getInstance().resend(chatName,   pdu);
    MyTcpServer::getInstance().resend(sourceName, pdu);
}

void MyTcpSocket::handleGroupChatRequest(PDU *pdu)
{
    char sourceName[32] = {'\0'};
    strncpy(sourceName, pdu->caData, 32);

    QStringList onlineFriend = OpeDB::getInstance().handleGroupChat(sourceName);
    for (int i = 0; i * 2 < onlineFriend.size(); i++) {
        QString tmp = onlineFriend.at(i * 2);
        MyTcpServer::getInstance().resend(tmp.toStdString().c_str(), pdu);
    }
}
