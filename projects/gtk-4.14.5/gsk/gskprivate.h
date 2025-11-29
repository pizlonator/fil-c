#pragma once

#include <glib.h>
#include <pango/pango.h>
#include <cairo.h>

G_BEGIN_DECLS

void       gsk_ensure_resources  (void);

PangoFont *gsk_reload_font (PangoFont            *font,
                            float                 scale,
                            cairo_hint_metrics_t  hint_metrics,
                            cairo_hint_style_t    hint_style,
                            cairo_antialias_t     antialias);

cairo_hint_style_t gsk_font_get_hint_style (PangoFont *font);

G_END_DECLS

