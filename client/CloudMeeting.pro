QT += core gui network multimedia multimediawidgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS


win32-msvc*:QMAKE_CXXFLAGS += /wd"4819" /utf-8

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AudioInput.cpp \
    AudioOutput.cpp \
    chatmessage.cpp \
    dialog.cpp \
    logqueue.cpp \
    main.cpp \
    mytcpsocket.cpp \
    mytextedit.cpp \
    myvideosurface.cpp \
    netheader.cpp \
    partner.cpp \
    recvsolve.cpp \
    screen.cpp \
    sendimg.cpp \
    sendtext.cpp \
    widget.cpp \
    adminpanel.cpp
# 工厂模块（无需改动内部代码）
INCLUDEPATH += $$PWD/factory
SOURCES += $$files($$PWD/factory/*.cpp)
HEADERS += $$files($$PWD/factory/*.h)

# Windows 下排除不兼容的 factory/server.cpp（包含 arpa/inet.h）
win32:CONFIG += no_server_cpp
win32:!isEmpty(CONFIG):contains(CONFIG, no_server_cpp) {
    SOURCES -= $$PWD/factory/server.cpp \
               $$PWD/factory/socket.cpp
}

HEADERS += \
    AudioInput.h \
    AudioOutput.h \
    chatmessage.h \
    dialog.h \
    logqueue.h \
    mytcpsocket.h \
    mytextedit.h \
    myvideosurface.h \
    netheader.h \
    partner.h \
    recvsolve.h \
    screen.h \
    sendimg.h \
    sendtext.h \
    widget.h \
    adminpanel.h

FORMS += \
    dialog.ui \
    widget.ui \
    adminpanel.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

LIBS += -lws2_32

