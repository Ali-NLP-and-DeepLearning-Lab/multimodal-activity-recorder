TEMPLATE = app


# plugins
QT += qml quick widgets network concurrent websockets

win32 {
} else:macx {
} else:android {
  QT += androidextras
} else:ios {
}


# basic config
CONFIG += c++11
CONFIG += resources_big

DEFINES += QT_DEPRECATED_WARNINGS

TARGET = MMRecorderServer

win32 {
  VERSION = 0.1.0
} else:macx {
  VERSION = 0.1.0

  QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.8
  QMAKE_LFLAGS += -ObjC
} else:android {
  VERSION = 0.1.0
} else:ios {
  VERSION = 0.1.0

  CONFIG -= bitcode

  QMAKE_IOS_DEPLOYMENT_TARGET = 8.0
}


# shared
include(MultiModalRecorderShared.pri)


# defines, headers, sources, and resources
DEFINES += MMRECORDER_SERVER

RESOURCES += \
    $$PWD/qml/qml_server.qrc

HEADERS += \
    $$PWD/sources/server/main.h \
    $$PWD/sources/server/mmrserver.h \
    sources/server/mmrconnection.h

SOURCES += \
    $$PWD/sources/server/main.cpp \
    $$PWD/sources/server/mmrserver.cpp \
    sources/server/mmrconnection.cpp

QML_IMPORT_PATH =

QML_DESIGNER_IMPORT_PATH =


# external sources
win32 {
} else:macx {
    #OBJECTIVE_SOURCES +=
} else:android {
} else:ios {
}


# default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
