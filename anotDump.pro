#-------------------------------------------------
#
# Project created by pholynyk 2019Aug01T21:00:00
#
#-------------------------------------------------

message(Platform is $$QMAKESPEC )

CONFIG += c++11
CONFIG += rtti
CONFIG -= debug_and_release

QT += core widgets

TARGET = anotDump

TEMPLATE = app

QMAKE_TARGET_PRODUCT = anotDump
QMAKE_TARGET_COMPANY = The OSCAR Team
QMAKE_TARGET_COPYRIGHT = © 2019 The OSCAR Team
QMAKE_TARGET_DESCRIPTION = "OpenSource STR.edf Dumper"
VERSION = 0.5.0

SOURCES += \
	anotDump/main.cpp \
	dumpSTR/edfparser.cpp \

HEADERS  += \
    dumpSTR/common.h \
    dumpSTR/edfparser.h \

