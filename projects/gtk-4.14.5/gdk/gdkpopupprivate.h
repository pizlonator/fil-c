#pragma once

#include "gdkpopup.h"

G_BEGIN_DECLS

struct _GdkPopupInterface
{
  GTypeInterface g_iface;

  gboolean      (* present)             (GdkPopup       *popup,
                                         int             width,
                                         int             height,
                                         GdkPopupLayout *layout);

  GdkGravity    (* get_surface_anchor)  (GdkPopup       *popup);
  GdkGravity    (* get_rect_anchor)     (GdkPopup       *popup);
  int           (* get_position_x)      (GdkPopup       *popup);
  int           (* get_position_y)      (GdkPopup       *popup);
};

typedef enum
{
  GDK_POPUP_PROP_PARENT,
  GDK_POPUP_PROP_AUTOHIDE,
  GDK_POPUP_NUM_PROPERTIES
} GdkPopupProperties;

guint gdk_popup_install_properties (GObjectClass *object_class,
                                    guint         first_prop);

G_END_DECLS

