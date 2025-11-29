#pragma once

#include <gtk/gtk.h>

#define PATH_TYPE_VIEW (path_view_get_type ())

G_DECLARE_FINAL_TYPE (PathView, path_view, PATH, VIEW, GtkWidget)

GtkWidget * path_view_new (GskPath *path);
