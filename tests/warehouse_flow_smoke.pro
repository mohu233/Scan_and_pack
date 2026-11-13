QT += widgets network openglwidgets core-private

CONFIG += console testcase c++17
TARGET = warehouse_flow_smoke
TEMPLATE = app

win32:LIBS += -lwinmm

SOURCES += \
    warehouse_flow_smoke.cpp \
    ../mainwindow.cpp

HEADERS += \
    ../mainwindow.h

FORMS += \
    ../mainwindow.ui
