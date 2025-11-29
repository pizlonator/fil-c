#pragma once

#include "inspectoroverlay.h"

G_BEGIN_DECLS

#define GTK_TYPE_BASELINE_OVERLAY             (gtk_baseline_overlay_get_type ())
G_DECLARE_FINAL_TYPE (GtkBaselineOverlay, gtk_baseline_overlay, GTK, BASELINE_OVERLAY, GtkInspectorOverlay)

GtkInspectorOverlay *   gtk_baseline_overlay_new                 (void);

G_END_DECLS



