# Set QTFFMPEGWRAPPER_SOURCE_PATH to point to the directory containing the QTFFmpegWrapper sources
QTFFMPEGWRAPPER_SOURCE_PATH = /Users/guigui/Development/ffmpeg

# Set FFMPEG_LIBRARY_PATH to point to the directory containing the FFmpeg import libraries (if needed - typically for Windows), i.e. the dll.a files
FFMPEG_LIBRARY_PATH = /usr/local/lib/

# Set FFMPEG_INCLUDE_PATH to point to the directory containing the FFMPEG includes (if needed - typically for Windows)
FFMPEG_INCLUDE_PATH = /Users/guigui/Development/ffmpeg

##################################################################

TEMPLATE = app
TARGET = BBQScreenClient2
DESTDIR = ./MacOSX
QT += core multimedia network widgets gui multimediawidgets
macx {
 LIBS += -framework Carbon
}
CONFIG += debug
DEFINES += QT_DLL QT_MULTIMEDIA_LIB QT_MULTIMEDIAWIDGETS_LIB QT_NETWORK_LIB QT_WIDGETS_LIB
INCLUDEPATH += ./GeneratedFiles \
    . \
    ./GeneratedFiles/Debug \
    $(QTDIR)/../qtmultimedia/include/QtMultimedia \
    $(QTDIR)/../qtmultimedia/include \
    $(FFMPEG_INCLUDE_PATH)

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
    -lswscale

# Add the path
LIBS += -L$$FFMPEG_LIBRARY_PATH

# Requied for some C99 defines
DEFINES += __STDC_CONSTANT_MACROS

