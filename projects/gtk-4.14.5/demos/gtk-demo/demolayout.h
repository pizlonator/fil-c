#pragma once

#include <gtk/gtk.h>

#define DEMO_TYPE_LAYOUT (demo_layout_get_type ())

G_DECLARE_FINAL_TYPE (DemoLayout, demo_layout, DEMO, LAYOUT, GtkLayoutManager)

GtkLayoutManager * demo_layout_new          (void);

void               demo_layout_set_position (DemoLayout *layout,
                                             float       position);
void               demo_layout_shuffle      (DemoLayout *layout);
