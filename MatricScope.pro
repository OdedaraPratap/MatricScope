TEMPLATE = app
TARGET = MatricScope

CONFIG += c++11 console
QT += core gui concurrent widgets sql

CONFIG += link_pkgconfig
PKGCONFIG += opencv4

# Headers
HEADERS += \
    calibrationdialog.h \
    cameraworker.h \
    mainwindow.h \
    camerasettingdialog.h \
    databasehelper.h \
    customshapedialog.h \
    traysettingsdialog.h \
    variationdialog.h \
    passwordmanager.h \
    changepassworddialog.h \
    passworddialog.h \
    historydialog.h

# Sources
SOURCES += \
    main.cpp \
    calibrationdialog.cpp \
    cameraworker.cpp \
    mainwindow.cpp \
    camerasettingdialog.cpp \
    databasehelper.cpp \
    customshapedialog.cpp \
    traysettingsdialog.cpp \
    variationdialog.cpp \
    passwordmanager.cpp \
    changepassworddialog.cpp \
    passworddialog.cpp \
    historydialog.cpp

RESOURCES += \
    resources.qrc
INCLUDEPATH += /opt/MVS/include
LIBS += -L/opt/MVS/lib/64 -lMvCameraControl
