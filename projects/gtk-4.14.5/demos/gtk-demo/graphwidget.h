#pragma once

#include <gtk/gtk.h>

#define GRAPH_TYPE_WIDGET (graph_widget_get_type ())
G_DECLARE_FINAL_TYPE (GraphWidget, graph_widget, GRAPH, WIDGET, GtkWidget)

GtkWidget * graph_widget_new       (void);
