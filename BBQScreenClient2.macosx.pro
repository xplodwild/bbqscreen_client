
TEMPLATE = app
TARGET = BBQScreenClient2
DESTDIR = ./MacOSX

QT += core network widgets gui opengl multimedia
CONFIG += debug
CONFIG += c++11
macx {
 QMAKE_CXXFLAGS += -stdlib=libc++
 QMAKE_CXXFLAGS += -mmacosx-version-min=10.7
 LIBS += -framework Carbon
}
DEFINES += QT_DLL QT_NETWORK_LIB QT_WIDGETS_LIB
INCLUDEPATH += ./GeneratedFiles \
    . \
    ./GeneratedFiles/Debug \
    $(QTDIR)/../qtmultimedia/include/QtMultimedia \
    $(QTDIR)/../qtmultimedia/include \
    ./QTFFmpegWrapper/

PRECOMPILED_HEADER = stdafx.h
DEPENDPATH += .
MOC_DIR += ./GeneratedFiles/debug
OBJECTS_DIR += debug
UI_DIR += ./GeneratedFiles
RCC_DIR += ./GeneratedFiles
QMAKE_CXXFLAGS = -std=c++11 -DNEW_FFMPEG_API

include(BBQScreenClient2.pri)

# Add the path
LIBS += -L/usr/local/lib

# Set list of required FFmpeg libraries
LIBS += -lavutil \
    -lavcodec \
    -lavformat \
    -lswscale \
    -lswresample \
    -lz \
    -lbz2 \
    -liconv

# Requied for some C99 defines
DEFINES += __STDC_CONSTANT_MACROS

