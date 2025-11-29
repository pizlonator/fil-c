#pragma once

#include <gtk/gtk.h>

#define DEMO3_TYPE_WIDGET (demo3_widget_get_type ())
G_DECLARE_FINAL_TYPE (Demo3Widget, demo3_widget, DEMO3, WIDGET, GtkWidget)

GtkWidget * demo3_widget_new (const char *resource);
