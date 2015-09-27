#-------------------------------------------------
#
# Project created by QtCreator 2015-08-21T12:10:28
#
#-------------------------------------------------

QT       -= gui
QT       += widgets

TARGET = DOController
TEMPLATE = lib

DEFINES += DOCONTROLLER_LIBRARY

INCLUDEPATH += ../lib/common

SOURCES += docontroller.cpp

HEADERS += docontroller.h\
        docontroller_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
