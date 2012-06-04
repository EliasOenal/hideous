TEMPLATE = app
QT -= core
CONFIG += console

SOURCES += test.c

HEADERS += hideous.h

macx{
SOURCES += \
    hideous_osx.c

DEFINES += "OSX=1"
LIBS += -pthread -framework Foundation -framework CoreFoundation -framework IOKit #Framework wth? OSX -.-"
}



