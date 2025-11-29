#pragma once

#include <gtk/gtk.h>

#define DEMO4_TYPE_WIDGET (demo4_widget_get_type ())
G_DECLARE_FINAL_TYPE (Demo4Widget, demo4_widget, DEMO4, WIDGET, GtkWidget)

GtkWidget * demo4_widget_new (void);
