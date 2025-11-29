

#pragma once

#include "inspectoroverlay.h"

G_BEGIN_DECLS

#define GTK_TYPE_SUBSURFACE_OVERLAY             (gtk_subsurface_overlay_get_type ())
G_DECLARE_FINAL_TYPE (GtkSubsurfaceOverlay, gtk_subsurface_overlay, GTK, SUBSURFACE_OVERLAY, GtkInspectorOverlay)

GtkInspectorOverlay *   gtk_subsurface_overlay_new                 (void);

G_END_DECLS



