bbqscreen_client
================

Sources for the client app of BBQScreen



===========================================
Compiling on Windows:
 - Install QT5 and the QT VS Add-In
 - Open the vcxproj in Visual Studio 2012
 - Build it and enjoy

Compiling on Debian/Ubuntu:
 - Install QT5 (libqt5-dev, or from source from qt-project.org)
 - Install ffmpeg libraries: apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
 - Run: qmake BBQScreenClient2.linux.pro
 - Run: make
 - The final binary will be built at ./Linux/BBQScreenClient2

Compiling on Mac OS X:
 - Install QT5 from qt-project.org
 - Download and build ffmpeg libraries from ffmpeg official website
 - Edit the BBQScreenClient2.macosx.pro to the location of the ffmpeg libraries
 - In a Terminal, run "qmake BBQScreenClient2.macosx.pro"
 - In the same terminal, run "make"
