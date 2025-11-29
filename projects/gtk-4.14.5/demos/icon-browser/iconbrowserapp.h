#pragma once

#include <gtk/gtk.h>


#define ICON_BROWSER_APP_TYPE (icon_browser_app_get_type ())
#define ICON_BROWSER_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ICON_BROWSER_APP_TYPE, IconBrowserApp))


typedef struct _IconBrowserApp       IconBrowserApp;
typedef struct _IconBrowserAppClass  IconBrowserAppClass;


GType           icon_browser_app_get_type    (void);
IconBrowserApp *icon_browser_app_new         (void);
