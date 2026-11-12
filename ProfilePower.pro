QT += widgets network

CONFIG += c++17
TARGET = ProfilePower
VERSION = $$cat($$PWD/VERSION.txt)
DEFINES += PROFILEPOWER_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    envmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    proxy_manager.cpp \
    profile_runtime.cpp \
    proxy_config.cpp \
    proxy_workflow.cpp \
    wireguard_proxy.cpp \
    browser_proxy_badge.cpp \
    proxy_exit_ip.cpp \
    environment_ip.cpp \
    ui_appearance.cpp

HEADERS += \
    envmanager.h \
    mainwindow.h \
    profile_runtime.h \
    proxy_config.h \
    wireguard_proxy.h \
    browser_proxy_badge.h \
    proxy_exit_ip.h

FORMS += \
    mainwindow.ui

RESOURCES += profilepower_resources.qrc

win32:RC_ICONS = profilepower.ico
win32:LIBS += -lversion -luser32 -lgdi32

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
