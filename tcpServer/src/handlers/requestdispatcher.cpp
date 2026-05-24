#include "mytcpsocket.h"

void MyTcpSocket::handlePdu(PDU *pdu)
{
    switch (pdu->uiMsgType)
    {
    case ENUM_MSG_TYPE_REGIST_REQUEST:
        handleRegistRequest(pdu);
        break;
    case ENUM_MSG_TYPE_LOGIN_REQUEST:
        handleLoginRequest(pdu);
        break;
    case ENUM_MSG_TYPE_ALL_ONLINE_REQUEST:
        handleAllOnlineRequest(pdu);
        break;
    case ENUM_MSG_TYPE_SEARCH_USER_REQUEST:
        handleSearchUserRequest(pdu);
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
        handleAddFriendRequest(pdu);
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE:
        handleAddFriendAgree(pdu);
        break;
    case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:
        handleAddFriendRefuse(pdu);
        break;
    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:
        handleFlushFriendRequest(pdu);
        break;
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
        handleDeleteFriendRequest(pdu);
        break;
    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:
        handlePrivateChatRequest(pdu);
        break;
    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:
        handleGroupChatRequest(pdu);
        break;
    case ENUM_MSG_TYPE_CREATE_DIR_REQUEST:
        handleCreateDirRequest(pdu);
        break;
    case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:
        handleFlushFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_DELETE_DIR_REQUEST:
        handleDeleteDirRequest(pdu);
        break;
    case ENUM_MSG_TYPE_RENAME_FILE_REQUEST:
        handleRenameFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_ENTER_DIR_REQUEST:
        handleEnterDirRequest(pdu);
        break;
    case ENUM_MSG_TYPE_UPLOAD_FILE_REQUEST:
        handleUploadFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_DELETE_FILE_REQUEST:
        handleDeleteFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST:
        handleDownloadFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
        handleShareFileRequest(pdu);
        break;
    case ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPOND:
        handleShareFileNoteRespond(pdu);
        break;
    case ENUM_MSG_TYPE_MOVE_FILE_REQUEST:
        handleMoveFileRequest(pdu);
        break;
    default:
        break;
    }
}
