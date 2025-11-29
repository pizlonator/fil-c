#pragma once

#include "gtklayoutmanager.h"

G_BEGIN_DECLS

void gtk_layout_manager_set_widget (GtkLayoutManager *manager,
                                    GtkWidget        *widget);

void gtk_layout_manager_remove_layout_child (GtkLayoutManager *manager,
                                             GtkWidget        *widget);

void gtk_layout_manager_set_root (GtkLayoutManager *manager,
                                  GtkRoot          *root);

G_END_DECLS
