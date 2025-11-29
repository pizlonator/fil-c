#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_BROADWAY
#include <gdk/broadway/gdkbroadway.h>
#endif

#ifdef GDK_WINDOWING_MACOS
#if HAVE_OBJC
#include <gdk/macos/gdkmacos.h>
#endif
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#ifndef __OBJC__
#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

int main() { return 0; }
