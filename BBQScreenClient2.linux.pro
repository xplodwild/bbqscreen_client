
TEMPLATE = app
TARGET = BBQScreenClient2
DESTDIR = ./Linux

QT += core network widgets gui
CONFIG += debug
DEFINES += QT_DLL QT_NETWORK_LIB QT_WIDGETS_LIB
INCLUDEPATH += ./GeneratedFiles \
    . \
    ./GeneratedFiles/Debug \
    $(QTDIR)/../qtmultimedia/include/QtMultimedia \
    $(QTDIR)/../qtmultimedia/include
PRECOMPILED_HEADER = stdafx.h
DEPENDPATH += .
MOC_DIR += ./GeneratedFiles/debug
OBJECTS_DIR += debug
UI_DIR += ./GeneratedFiles
RCC_DIR += ./GeneratedFiles
QMAKE_CXXFLAGS = -std=c++11 -DNEW_FFMPEG_API

include(BBQScreenClient2.pri)

# Set list of required FFmpeg libraries
LIBS += -lavutil \
    -lavcodec \
    -lavformat \
    -lswscale \
    -lz

# Add the path
LIBS += -L/usr/local/lib

# Requied for some C99 defines
DEFINES += __STDC_CONSTANT_MACROS

