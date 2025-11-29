#pragma once

#include "gdktoplevel.h"
#include "gdktoplevelsizeprivate.h"

#include <graphene.h>

G_BEGIN_DECLS


struct _GdkToplevelInterface
{
  GTypeInterface g_iface;

  void          (* present)             (GdkToplevel       *toplevel,
                                         GdkToplevelLayout *layout);
  gboolean      (* minimize)            (GdkToplevel       *toplevel);
  gboolean      (* lower)               (GdkToplevel       *toplevel);
  void          (* focus)               (GdkToplevel       *toplevel,
                                         guint32            timestamp);
  gboolean      (* show_window_menu)    (GdkToplevel       *toplevel,
                                         GdkEvent          *event);
  gboolean      (* supports_edge_constraints) (GdkToplevel *toplevel);
  void          (* inhibit_system_shortcuts)  (GdkToplevel *toplevel,
                                               GdkEvent    *event);
  void          (* restore_system_shortcuts)  (GdkToplevel *toplevel);
  void          (* begin_resize)        (GdkToplevel       *toplevel,
                                         GdkSurfaceEdge     edge,
                                         GdkDevice         *device,
                                         int                button,
                                         double             x,
                                         double             y,
                                         guint32            timestamp);
  void          (* begin_move)          (GdkToplevel       *toplevel,
                                         GdkDevice         *device,
                                         int                button,
                                         double             x,
                                         double             y,
                                         guint32            timestamp);
  gboolean      (* titlebar_gesture)    (GdkToplevel       *toplevel,
                                         GdkTitlebarGesture gesture);

  void          (* export_handle)          (GdkToplevel          *toplevel,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);

  char *        (* export_handle_finish)   (GdkToplevel          *toplevel,
                                            GAsyncResult         *result,
                                            GError              **error);

  void          (* unexport_handle)        (GdkToplevel          *toplevel,
                                            const char           *handle);
};

typedef enum
{
  GDK_TOPLEVEL_PROP_STATE,
  GDK_TOPLEVEL_PROP_TITLE,
  GDK_TOPLEVEL_PROP_STARTUP_ID,
  GDK_TOPLEVEL_PROP_TRANSIENT_FOR,
  GDK_TOPLEVEL_PROP_MODAL,
  GDK_TOPLEVEL_PROP_ICON_LIST,
  GDK_TOPLEVEL_PROP_DECORATED,
  GDK_TOPLEVEL_PROP_DELETABLE,
  GDK_TOPLEVEL_PROP_FULLSCREEN_MODE,
  GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED,
  GDK_TOPLEVEL_NUM_PROPERTIES
} GdkToplevelProperties;

guint gdk_toplevel_install_properties (GObjectClass *object_class,
                                       guint         first_prop);

void gdk_toplevel_notify_compute_size (GdkToplevel     *toplevel,
                                       GdkToplevelSize *size);

void  gdk_toplevel_export_handle        (GdkToplevel          *toplevel,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);

char *gdk_toplevel_export_handle_finish (GdkToplevel          *toplevel,
                                         GAsyncResult         *result,
                                         GError              **error);

void  gdk_toplevel_unexport_handle      (GdkToplevel          *toplevel,
                                         const char           *handle);

G_END_DECLS

