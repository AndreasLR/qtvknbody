#-------------------------------------------------
#
# Project created by QtCreator 2016-06-15T00:33:25
#
#-------------------------------------------------

QT       += core gui
unix: QT += x11extras

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = nbody
TEMPLATE = app

INCLUDEPATH += "khronos/vulkan/"
INCLUDEPATH += "external/"

unix:LIBS += -lvulkan
win32:LIBS += "$$PWD/lib/x64/vulkan-1.lib"

win32: RC_ICONS = doc/app.ico

OBJECTS_DIR = buildfiles/obj
MOC_DIR = buildfiles/moc
RCC_DIR = buildfiles/rcc
UI_DIR = buildfiles/ui

SOURCES += main.cpp\
    mainwindow.cpp \
    common.cpp \
    vulkanwindow.cpp \
    vulkanbase.cpp \
    vulkantextureloader.cpp

HEADERS  += BUILD_OPTIONS.h \
    mainwindow.hpp \
    common.hpp \
    platform.hpp \
    vulkanwindow.hpp \
    include/ccmatrix.hpp \
    include/matrix.hpp \
    include/rotationmatrix.hpp \
    vulkanbase.hpp \
    vulkantextureloader.hpp

FORMS    += mainwindow.ui

DISTFILES += \
    uncrustify.cfg \
    shaders/gaussblur.frag \
    shaders/nbody.frag \
    shaders/gaussblur.vert \
    shaders/nbody.vert \
    shaders/nbody_leapfrog_step_one.comp \
    shaders/nbody_leapfrog_step_two.comp \
    shaders/normal_texture.frag \
    shaders/normal_texture.vert \
    shaders/tone_mapping.frag \
    shaders/tone_mapping.vert \
    shaders/build.sh \
    shaders/luminosity.frag \
    shaders/luminosity.vert \
    shaders/performance_meter.frag \
    shaders/performance_meter.vert

RESOURCES += \
    resources.qrc
