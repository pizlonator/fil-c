TEMPLATE = app
TARGET = mousevideooverlay
DESTDIR = ./debug
QT += gui widgets opengl
CONFIG += debug link_pkgconfig
DEFINES += UNICODE QT_THREAD_SUPPORT QT_CORE_LIB QT_GUI_LIB
PKGCONFIG = gstreamer-1.0 gstreamer-video-1.0

win32 {
DEFINES += WIN32
INCLUDEPATH +=  ./GeneratedFiles \
    ./GeneratedFiles/Debug \
    C:/gstreamer/include \
    C:/gstreamer/include/libxml2 \
    C:/gstreamer/include/glib-2.0 \
    C:/gstreamer/lib/glib-2.0/include \
    C:/gstreamer/include/gstreamer-1.0
LIBS += -L"C:/gstreamer/lib" \
    -L"C:/gstreamer/bin" \
    -lgstreamer-1.0 \
    -lgstvideo-1.0 \
    -lglib-2.0 \
    -lgmodule-2.0 \
    -lgobject-2.0 \
    -lgthread-2.0 \
    -lopengl32 \
    -lglu32
}

unix {
DEFINES += UNIX
INCLUDEPATH += GeneratedFiles \
    GeneratedFiles/Debug 
LIBS += -lGLU -lGL
}    
    
DEPENDPATH += .
MOC_DIR += ./GeneratedFiles/debug
OBJECTS_DIR += debug
UI_DIR += ./GeneratedFiles
RCC_DIR += ./GeneratedFiles

#Include file(s)
include(mousevideooverlay.pri)
