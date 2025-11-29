
#pragma once

#include "gtkwidget.h"
#include "gtkenums.h"

#define GTK_TYPE_GIZMO                 (gtk_gizmo_get_type ())
#define GTK_GIZMO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_GIZMO, GtkGizmo))
#define GTK_GIZMO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_GIZMO, GtkGizmoClass))
#define GTK_IS_GIZMO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_GIZMO))
#define GTK_IS_GIZMO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GIZMO))
#define GTK_GIZMO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GIZMO, GtkGizmoClass))

typedef struct _GtkGizmo             GtkGizmo;
typedef struct _GtkGizmoClass        GtkGizmoClass;

typedef void    (* GtkGizmoMeasureFunc)   (GtkGizmo       *gizmo,
                                           GtkOrientation  orientation,
                                           int             for_size,
                                           int            *minimum,
                                           int            *natural,
                                           int            *minimum_baseline,
                                           int            *natural_baseline);
typedef void    (* GtkGizmoAllocateFunc)  (GtkGizmo *gizmo,
                                           int       width,
                                           int       height,
                                           int       baseline);
typedef void    (* GtkGizmoSnapshotFunc)  (GtkGizmo    *gizmo,
                                           GtkSnapshot *snapshot);
typedef gboolean (* GtkGizmoContainsFunc) (GtkGizmo  *gizmo,
                                           double     x,
                                           double     y);
typedef gboolean (* GtkGizmoFocusFunc)    (GtkGizmo         *gizmo,
                                           GtkDirectionType  direction);
typedef gboolean (* GtkGizmoGrabFocusFunc)(GtkGizmo         *gizmo);

struct _GtkGizmo
{
  GtkWidget parent_instance;

  GtkGizmoMeasureFunc   measure_func;
  GtkGizmoAllocateFunc  allocate_func;
  GtkGizmoSnapshotFunc  snapshot_func;
  GtkGizmoContainsFunc  contains_func;
  GtkGizmoFocusFunc     focus_func;
  GtkGizmoGrabFocusFunc grab_focus_func;
};

struct _GtkGizmoClass
{
  GtkWidgetClass parent_class;
};

GType      gtk_gizmo_get_type (void) G_GNUC_CONST;

GtkWidget *gtk_gizmo_new (const char            *css_name,
                          GtkGizmoMeasureFunc    measure_func,
                          GtkGizmoAllocateFunc   allocate_func,
                          GtkGizmoSnapshotFunc   snapshot_func,
                          GtkGizmoContainsFunc   contains_func,
                          GtkGizmoFocusFunc      focus_func,
                          GtkGizmoGrabFocusFunc  grab_focus_func);

GtkWidget *gtk_gizmo_new_with_role (const char            *css_name,
                                    GtkAccessibleRole      role,
                                    GtkGizmoMeasureFunc    measure_func,
                                    GtkGizmoAllocateFunc   allocate_func,
                                    GtkGizmoSnapshotFunc   snapshot_func,
                                    GtkGizmoContainsFunc   contains_func,
                                    GtkGizmoFocusFunc      focus_func,
                                    GtkGizmoGrabFocusFunc  grab_focus_func);


