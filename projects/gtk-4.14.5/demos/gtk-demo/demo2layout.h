#pragma once

#include <gtk/gtk.h>

#define DEMO2_TYPE_LAYOUT (demo2_layout_get_type ())

G_DECLARE_FINAL_TYPE (Demo2Layout, demo2_layout, DEMO2, LAYOUT, GtkLayoutManager)

GtkLayoutManager * demo2_layout_new          (void);

void               demo2_layout_set_position (Demo2Layout *layout,
                                              float        position);
float              demo2_layout_get_position (Demo2Layout *layout);
void               demo2_layout_set_offset   (Demo2Layout *layout,
                                              float        offset);
float              demo2_layout_get_offset   (Demo2Layout *layout);
