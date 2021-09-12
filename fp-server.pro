QT += core serialport mqtt sql
QT -= gui

CONFIG += c++11

TARGET = fp-server
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    fingerprint.cpp \
    fpthread.cpp \
    fpmain.cpp

HEADERS += \
    fingerprint.h \
    fpthread.h \
    defs.h \
    fpmain.h
