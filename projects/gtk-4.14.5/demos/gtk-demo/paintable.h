#pragma once

#include <gtk/gtk.h>

void            gtk_nuclear_snapshot           (GtkSnapshot     *snapshot,
                                                const GdkRGBA   *foreground,
                                                const GdkRGBA   *background,
                                                double           width,
                                                double           height,
                                                double           rotation);

GdkPaintable *  gtk_nuclear_icon_new            (double          rotation);
GdkPaintable *  gtk_nuclear_animation_new       (gboolean        draw_background);
GtkMediaStream *gtk_nuclear_media_stream_new    (void);
