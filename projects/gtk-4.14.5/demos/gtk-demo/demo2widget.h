#pragma once

#include <gtk/gtk.h>

#define DEMO2_TYPE_WIDGET (demo2_widget_get_type ())
G_DECLARE_FINAL_TYPE (Demo2Widget, demo2_widget, DEMO2, WIDGET, GtkWidget)

GtkWidget * demo2_widget_new       (void);

void        demo2_widget_add_child (Demo2Widget *self,
                                    GtkWidget  *child);
