

#pragma once

#include "inspectoroverlay.h"

G_BEGIN_DECLS

#define GTK_TYPE_LAYOUT_OVERLAY             (gtk_layout_overlay_get_type ())
G_DECLARE_FINAL_TYPE (GtkLayoutOverlay, gtk_layout_overlay, GTK, LAYOUT_OVERLAY, GtkInspectorOverlay)

GtkInspectorOverlay *   gtk_layout_overlay_new                 (void);

G_END_DECLS



