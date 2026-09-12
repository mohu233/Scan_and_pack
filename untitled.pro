QT += widgets network openglwidgets core-private

CONFIG += c++17
VERSION = 1.0.4
win32:LIBS += -lwinmm

# Keep MinGW memory usage low on machines with a small/full system drive.
QMAKE_CXXFLAGS_DEBUG -= -g

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    untitled_zh_CN.ts
CONFIG += lrelease

# Do not use qmake's embed_translations here.  On Windows, qmake writes the
# generated .qrc using the active ANSI code page; a project path containing
# Chinese characters then cannot be parsed as UTF-8 by Qt 6's rcc.

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
