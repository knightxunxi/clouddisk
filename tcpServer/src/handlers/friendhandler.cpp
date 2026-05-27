#include "mytcpsocket.h"
#include "mytcpserver.h"
#include "opedb.h"
#include "pdufieldcodec.h"

#include <cstring>

void MyTcpSocket::handleAddFriendRequest(PDU *pdu)
{
    const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
    const QByteArray perNameBytes = names.first.toUtf8();
    const QByteArray nameBytes = names.second.toUtf8();

    int ret = OpeDB::getInstance().handleAddFriend(perNameBytes.constData(),
                                                   nameBytes.constData());
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
    if (-1 == ret) {
        strcpy(respdu->caData, UNKNOWN_ERROR);
        write((char*)respdu, respdu->uiPDULen);
    } else if (0 == ret) {
        strcpy(respdu->caData, EXISTED_FRIEND);
        write((char*)respdu, respdu->uiPDULen);
    } else if (1 == ret) {
        MyTcpServer::getInstance().resend(perNameBytes.constData(), pdu);
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
    const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
    const QByteArray addedNameBytes = names.first.toUtf8();
    const QByteArray sourceNameBytes = names.second.toUtf8();

    OpeDB::getInstance().handleAddFriendAgree(addedNameBytes.constData(),
                                              sourceNameBytes.constData());
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE;
    MyTcpServer::getInstance().resend(sourceNameBytes.constData(), pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleAddFriendRefuse(PDU *pdu)
{
    const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
    const QByteArray sourceNameBytes = names.second.toUtf8();

    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;
    MyTcpServer::getInstance().resend(sourceNameBytes.constData(), pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleFlushFriendRequest(PDU *pdu)
{
    const QString sourceName = PduFieldCodec::fixedString(pdu->caData, 32);
    const QByteArray sourceNameBytes = sourceName.toUtf8();

    QStringList strList = OpeDB::getInstance().handleFlushFriend(sourceNameBytes.constData());
    unit uiMsgLenLocal = strList.size() / 2 * 36;
    PDU *respdu = mkPDU(uiMsgLenLocal);
    respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
    for (int i = 0; i * 2 < strList.size(); i++) {
        PduFieldCodec::writeFixedString((char*)(respdu->caMsg) + 36 * i,
                                        32,
                                        strList.at(i * 2));
        PduFieldCodec::writeFixedString((char*)(respdu->caMsg) + 36 * i + 32,
                                        4,
                                        strList.at(i * 2 + 1));
    }

    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handleDeleteFriendRequest(PDU *pdu)
{
    const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
    const QByteArray sourceNameBytes = names.first.toUtf8();
    const QByteArray deleteNameBytes = names.second.toUtf8();

    bool ret = OpeDB::getInstance().handleDeleteFriend(sourceNameBytes.constData(),
                                                       deleteNameBytes.constData());
    PDU *respdu = mkPDU(0);
    respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
    strcpy(respdu->caData, ret ? DELETE_OK : DELETE_FAILED);
    MyTcpServer::getInstance().resend(deleteNameBytes.constData(), pdu);
    write((char*)respdu, respdu->uiPDULen);
    free(respdu); respdu = nullptr;
}

void MyTcpSocket::handlePrivateChatRequest(PDU *pdu)
{
    const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
    const QByteArray sourceNameBytes = names.first.toUtf8();
    const QByteArray chatNameBytes = names.second.toUtf8();

    MyTcpServer::getInstance().resend(chatNameBytes.constData(),   pdu);
    MyTcpServer::getInstance().resend(sourceNameBytes.constData(), pdu);
}

void MyTcpSocket::handleGroupChatRequest(PDU *pdu)
{
    const QString sourceName = PduFieldCodec::fixedString(pdu->caData, 32);
    const QByteArray sourceNameBytes = sourceName.toUtf8();

    QStringList onlineFriend = OpeDB::getInstance().handleGroupChat(sourceNameBytes.constData());
    for (int i = 0; i * 2 < onlineFriend.size(); i++) {
        const QByteArray friendNameBytes = onlineFriend.at(i * 2).toUtf8();
        MyTcpServer::getInstance().resend(friendNameBytes.constData(), pdu);
    }
}
