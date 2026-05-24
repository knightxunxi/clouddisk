QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

include(../common/protocol/protocol.pri)

INCLUDEPATH += \
    $$PWD/src/network \
    $$PWD/src/ui \
    $$PWD/src/features/friends \
    $$PWD/src/features/files

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/network/tcpclient.cpp \
    src/ui/opewidget.cpp \
    src/features/friends/friend.cpp \
    src/features/friends/online.cpp \
    src/features/friends/privatechat.cpp \
    src/features/files/book.cpp \
    src/features/files/sharefile.cpp

HEADERS += \
    src/network/tcpclient.h \
    src/ui/opewidget.h \
    src/features/friends/friend.h \
    src/features/friends/online.h \
    src/features/friends/privatechat.h \
    src/features/files/book.h \
    src/features/files/sharefile.h

FORMS += \
    forms/online.ui \
    forms/privatechat.ui \
    forms/tcpclient.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources/FileType.qrc \
    resources/config.qrc
