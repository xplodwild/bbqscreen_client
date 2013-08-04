bbqscreen_client
================

Sources for the client app of BBQScreen



===========================================
Compiling on Windows:
 - Install QT5 and the QT VS Add-In
 - Open the vcxproj in Visual Studio 2012
 - Build it and enjoy


Compiling on Linux/Mac OS X:
 - Install QT5, libavcodec and libswscale
 - Run "qmake BBQScreenClient2.os.pro" where os is either macosx or linux
 - On Mac OS X, edit the .pro to the location of the libraries
 - Run make
