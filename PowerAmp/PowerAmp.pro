#-------------------------------------------------
#
# Project created by QtCreator 2015-12-14T15:31:27
#
#-------------------------------------------------

QT       -= gui
QT       += serialport

TARGET = PowerAmp
TEMPLATE = lib

DEFINES += POWERAMP_LIBRARY

INCLUDEPATH += ../lib/common

SOURCES += poweramp.cpp

HEADERS += poweramp.h\
        poweramp_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
