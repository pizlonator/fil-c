#pragma once

#include "gtknative.h"

G_BEGIN_DECLS

/**
 * GtkNativeIface:
 *
 * The list of functions that must be implemented for the `GtkNative` interface.
 */
struct _GtkNativeInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GdkSurface *  (* get_surface)           (GtkNative    *self);
  GskRenderer * (* get_renderer)          (GtkNative    *self);

  void          (* get_surface_transform) (GtkNative    *self,
                                           double       *x,
                                           double       *y);

  void          (* layout)                (GtkNative    *self,
                                           int           width,
                                           int           height);
};

void    gtk_native_queue_relayout         (GtkNative    *native);
void    gtk_native_update_opaque_region   (GtkNative    *native,
                                           GtkWidget    *contents,
                                           gboolean      subtract_decoration_corners,
                                           gboolean      subtract_shadow,
                                           int           resize_handle_size);

G_END_DECLS

