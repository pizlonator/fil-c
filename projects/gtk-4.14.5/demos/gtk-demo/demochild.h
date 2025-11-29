#pragma once

#include <gtk/gtk.h>

#define DEMO_TYPE_CHILD (demo_child_get_type ())
G_DECLARE_FINAL_TYPE (DemoChild, demo_child, DEMO, CHILD, GtkWidget)

GtkWidget * demo_child_new (const char *color);
