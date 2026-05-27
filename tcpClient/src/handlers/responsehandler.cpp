#include "tcpclient.h"

#include "pdufieldcodec.h"
#include "privatechat.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include <cstring>

namespace {

QString dataText(PDU *pdu)
{
    return PduFieldCodec::fixedString(pdu->caData, sizeof(pdu->caData));
}

} // namespace

void tcpClient::dispatchResponse(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_REGIST_RESPOND:
    case ENUM_MSG_TYPE_LOGIN_RESPOND:
        handleSessionResponse(pdu);
        break;
    case ENUM_MSG_TYPE_ALL_ONLINE_RESPOND:
    case ENUM_MSG_TYPE_SEARCH_USER_RESPOND:
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
    case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND:
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE:
    case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:
    case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:
    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:
        handleFriendResponse(pdu);
        break;
    case ENUM_MSG_TYPE_CREATE_DIR_RESPOND:
    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:
    case ENUM_MSG_TYPE_DELETE_DIR_RESPOND:
    case ENUM_MSG_TYPE_RENAME_FILE_RESPOND:
    case ENUM_MSG_TYPE_ENTER_DIR_RESPOND:
    case ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND:
    case ENUM_MSG_TYPE_DELETE_FILE_RESPOND:
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND:
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND:
    case ENUM_MSG_TYPE_MOVE_FILE_RESPOND:
        handleFileResponse(pdu);
        break;
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND:
    case ENUM_MSG_TYPE_SHARE_FILE_NOTE_REQUEST:
        handleShareResponse(pdu);
        break;
    default:
        break;
    }
}

void tcpClient::handleSessionResponse(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_REGIST_RESPOND:
        if (dataText(pdu) == REGIST_OK) {
            QMessageBox::information(this, "注册", REGIST_OK);
        } else {
            QMessageBox::warning(this, "注册", REGIST_FAILED);
        }
        break;
    case ENUM_MSG_TYPE_LOGIN_RESPOND:
        if (dataText(pdu) == LOGIN_OK) {
            m_strCurPath = QString("./%1").arg(m_strLoginName);
            QMessageBox::information(this, "登录", LOGIN_OK);
            OpeWidget::getInstance().setCurrentUser(m_strLoginName);
            OpeWidget::getInstance().show();
            hide();
        } else {
            QMessageBox::warning(this, "登录", LOGIN_FAILED);
        }
        break;
    default:
        break;
    }
}

void tcpClient::handleFriendResponse(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_ALL_ONLINE_RESPOND:
        OpeWidget::getInstance().getFriend()->showAllOnlineUser(pdu);
        break;
    case ENUM_MSG_TYPE_SEARCH_USER_RESPOND:
        if (dataText(pdu) == SEARCH_USER_NO) {
            QMessageBox::information(this, "搜索",
                                     QString("%1: not exist").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
        } else if (dataText(pdu) == SEARCH_USER_ONLINE) {
            QMessageBox::information(this, "搜索",
                                     QString("%1: online").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
        } else if (dataText(pdu) == SEARCH_USER_OFFLINE) {
            QMessageBox::information(this, "搜索",
                                     QString("%1: offline").arg(OpeWidget::getInstance().getFriend()->m_strSearchName));
        }
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
    {
        const PduFieldCodec::FixedPair names = PduFieldCodec::fixedPair(pdu->caData);
        int ret = QMessageBox::information(this, "加好友",
                                           QString("%1 want to be your friend?").arg(names.second),
                                           QMessageBox::Yes,
                                           QMessageBox::No);
        PDU *respdu = mkPDU(0);
        PduFieldCodec::writeFixedPair(respdu->caData, names.first, names.second);
        respdu->uiMsgType = (ret == QMessageBox::Yes)
                ? ENUM_MSG_TYPE_ADD_FRIEND_AGREE
                : ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;
        m_tcpSocket.write((char*)respdu, respdu->uiPDULen);
        free(respdu); respdu = nullptr;
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND:
        QMessageBox::information(this, "添加好友", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE:
        QMessageBox::information(this, "添加好友",
                                 QString("%1 已同意您的好友申请！").arg(dataText(pdu)));
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:
        QMessageBox::information(this, "添加好友",
                                 QString("%1 已拒绝您的好友申请！").arg(dataText(pdu)));
        break;
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:
        OpeWidget::getInstance().getFriend()->updateFriendList(pdu);
        break;
    case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:
        if (dataText(pdu) == DELETE_OK) {
            QMessageBox::information(this, "删除好友", dataText(pdu));
        } else {
            QMessageBox::warning(this, "删除好友", dataText(pdu));
        }
        break;
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
        QMessageBox::information(this, "删除好友",
                                 QString("%1 已解除与您的好友关系！")
                                 .arg(PduFieldCodec::fixedString(pdu->caData, 32)));
        break;
    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:
    {
        if (PrivateChat::getInstance().isHidden()) {
            PrivateChat::getInstance().show();
        }
        const QString sourceName = PduFieldCodec::fixedString(pdu->caData, 32);
        PrivateChat::getInstance().setChatName(sourceName);
        PrivateChat::getInstance().updateMsg(pdu);
        break;
    }
    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:
        OpeWidget::getInstance().getFriend()->updateGroupMsg(pdu);
        break;
    default:
        break;
    }
}

void tcpClient::handleFileResponse(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_CREATE_DIR_RESPOND:
        QMessageBox::information(this, "创建文件夹", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:
    {
        OpeWidget::getInstance().getBook()->updateFileList(pdu);
        QString strEnterDir = OpeWidget::getInstance().getBook()->enterDir();
        if (!strEnterDir.isEmpty()) {
            m_strCurPath = m_strCurPath + "/" + strEnterDir;
            qDebug() << "进入的文件夹路径:" << m_strCurPath << "文件夹名字：" << strEnterDir;
        }
        break;
    }
    case ENUM_MSG_TYPE_DELETE_DIR_RESPOND:
        QMessageBox::information(this, "删除文件夹", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_RENAME_FILE_RESPOND:
        QMessageBox::information(this, "重命名文件", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_ENTER_DIR_RESPOND:
        OpeWidget::getInstance().getBook()->clearEnterDir();
        QMessageBox::information(this, "进入文件夹", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND:
        if (dataText(pdu) == UPLOAD_FILE_RESUME) {
            OpeWidget::getInstance().getBook()->uploadFileData();
        } else {
            QMessageBox::information(this, "上传文件", dataText(pdu));
        }
        break;
    case ENUM_MSG_TYPE_DELETE_FILE_RESPOND:
        QMessageBox::information(this, "删除文件", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND:
    {
        const PduFieldCodec::DownloadFileResponse response =
                PduFieldCodec::downloadFileResponse(pdu->caData);
        const QString savePath = OpeWidget::getInstance().getBook()->getFileSavePath();
        qDebug() << "download response:" << response.fileName
                 << "total:" << response.fileSize
                 << "skip:" << response.skipSize;

        if (!response.valid || savePath.isEmpty()) {
            QMessageBox::warning(this, "下载文件", "下载文件失败");
            break;
        }
        if (response.fileSize == 0) {
            QFile file(savePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                QMessageBox::information(this, "下载文件", "下载文件成功");
            } else {
                QMessageBox::warning(this, "下载文件", "无法创建保存文件");
            }
            break;
        }
        if (!m_downloadSession.start(savePath, response.fileSize, response.skipSize)) {
            QMessageBox::warning(this, "下载文件", "下载会话初始化失败");
            break;
        }
        if (m_downloadSession.isComplete()) {
            m_downloadSession.reset();
            QMessageBox::information(this, "下载文件", "下载文件成功");
            break;
        }
        startDownload(savePath, response.skipSize);
        break;
    }
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_DATA_RESPOND:
        handleDownloadData(PduFieldCodec::messageBytes(pdu));
        break;
    case ENUM_MSG_TYPE_MOVE_FILE_RESPOND:
        QMessageBox::information(this, "移动文件", dataText(pdu));
        break;
    default:
        break;
    }
}

void tcpClient::handleShareResponse(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_SHARE_FILE_RESPOND:
        QMessageBox::information(this, "共享文件", dataText(pdu));
        break;
    case ENUM_MSG_TYPE_SHARE_FILE_NOTE_REQUEST:
    {
        const QString sharedPath = PduFieldCodec::messageString(pdu);
        const QString senderName = PduFieldCodec::fixedString(pdu->caData, 32);
        const QString fileName = QFileInfo(sharedPath).fileName();
        if (fileName.isEmpty()) {
            qDebug() << "共享文件路径无效:" << sharedPath;
            break;
        }

        const QString note = QString("%1 share file->%2 \n Do you accept?")
                .arg(senderName)
                .arg(fileName);
        int ret = QMessageBox::question(this, "共享文件", note);
        if (ret == QMessageBox::Yes) {
            PDU *respdu = mkPDU(pdu->uiMsgLen);
            respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPOND;
            PduFieldCodec::writeMessage(respdu, PduFieldCodec::messageBytes(pdu));
            PduFieldCodec::writeFixedString(respdu->caData,
                                            sizeof(respdu->caData),
                                            loginName());
            m_tcpSocket.write((char*)respdu, respdu->uiPDULen);
            free(respdu); respdu = nullptr;
        }
        break;
    }
    default:
        break;
    }
}
