#CONFIG += release
CONFIG   += c++11

isEmpty(PSISDK) {
    include(../../psiplugin.pri)
} else {
    include($$PSISDK/plugins/psiplugin.pri)
}

RESOURCES = imagepreviewplugin.qrc

SOURCES += imagepreviewplugin.cpp \

