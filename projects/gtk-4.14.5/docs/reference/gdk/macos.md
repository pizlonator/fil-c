Title: macOS Interaction

## macOS backend-specific functions

The functions in this section are specific to the GDK macOS backend.
To use them, you need to include the `<gdk/macos/gdkmacos.h>` header and
use the macOS-specific pkg-config `gtk4-macos` file to build your
application.

## Checking for the macOS backend

GDK can be built with support for multiple backends, which means you will
need to perform both compile time *and* run time checks if you wish to call
backend-specific API; for instance, the code sample below will guard the
calls to different backends, and error out on unsupported windowing systems:

```c
#ifdef GDK_WINDOWING_MACOS
#include <gdk/macos/gdkmacos.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (display))
    {
      // make macOS-specific calls here
    }
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      // make Wayland-specific calls here
    }
  else
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    {
      // make X11-specific calls here
    }
  else
#endif
  g_error ("Unsupported GDK backend");
```

The compile time check is performed by using the `GDK_WINDOWING_*`
pre-processor symbols; there is one defined for each windowing system
backend built into GDK. For Wayland, the symbol is `GDK_WINDOWING_MACOS`.

The run time check is performed by looking at the type of the
[class@Gdk.Display] object. For Wayland, the display objects will be of type
`GdkMacosDisplay`.
