#pragma once

#include <gtk/gtk.h>
#include "iconbrowserapp.h"


#define ICON_BROWSER_WINDOW_TYPE (icon_browser_window_get_type ())
#define ICON_BROWSER_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ICON_BROWSER_WINDOW_TYPE, IconBrowserWindow))


typedef struct _IconBrowserWindow         IconBrowserWindow;
typedef struct _IconBrowserWindowClass    IconBrowserWindowClass;


GType              icon_browser_window_get_type     (void);
IconBrowserWindow *icon_browser_window_new          (IconBrowserApp    *app);
