
#include "gtkgizmoprivate.h"
#include "gtkwidgetprivate.h"


G_DEFINE_TYPE (GtkGizmo, gtk_gizmo, GTK_TYPE_WIDGET);

static void
gtk_gizmo_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->measure_func)
    self->measure_func (self, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_gizmo_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->allocate_func)
    self->allocate_func (self, width, height, baseline);
}

static void
gtk_gizmo_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->snapshot_func)
    self->snapshot_func (self, snapshot);
  else
    GTK_WIDGET_CLASS (gtk_gizmo_parent_class)->snapshot (widget, snapshot);
}

static gboolean
gtk_gizmo_contains (GtkWidget *widget,
                    double     x,
                    double     y)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->contains_func)
    return self->contains_func (self, x, y);
  else
    return GTK_WIDGET_CLASS (gtk_gizmo_parent_class)->contains (widget, x, y);
}

static gboolean
gtk_gizmo_focus (GtkWidget        *widget,
                 GtkDirectionType  direction)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->focus_func)
    return self->focus_func (self, direction);

  return FALSE;
}

static gboolean
gtk_gizmo_grab_focus (GtkWidget *widget)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->grab_focus_func)
    return self->grab_focus_func (self);

  return FALSE;
}

static void
gtk_gizmo_finalize (GObject *object)
{
  GtkGizmo *self = GTK_GIZMO (object);
  GtkWidget *widget;

  widget = _gtk_widget_get_first_child (GTK_WIDGET (self));
  while (widget != NULL)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (widget);

      gtk_widget_unparent (widget);

      widget = next;
    }

  G_OBJECT_CLASS (gtk_gizmo_parent_class)->finalize (object);
}

static void
gtk_gizmo_class_init (GtkGizmoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_gizmo_finalize;

  widget_class->measure = gtk_gizmo_measure;
  widget_class->size_allocate = gtk_gizmo_size_allocate;
  widget_class->snapshot = gtk_gizmo_snapshot;
  widget_class->contains = gtk_gizmo_contains;
  widget_class->grab_focus = gtk_gizmo_grab_focus;
  widget_class->focus = gtk_gizmo_focus;
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
}

GtkWidget *
gtk_gizmo_new (const char            *css_name,
               GtkGizmoMeasureFunc    measure_func,
               GtkGizmoAllocateFunc   allocate_func,
               GtkGizmoSnapshotFunc   snapshot_func,
               GtkGizmoContainsFunc   contains_func,
               GtkGizmoFocusFunc      focus_func,
               GtkGizmoGrabFocusFunc  grab_focus_func)
{
  return gtk_gizmo_new_with_role (css_name,
                                  GTK_ACCESSIBLE_ROLE_GENERIC,
                                  measure_func,
                                  allocate_func,
                                  snapshot_func,
                                  contains_func,
                                  focus_func,
                                  grab_focus_func);
}

GtkWidget *
gtk_gizmo_new_with_role (const char            *css_name,
                         GtkAccessibleRole      role,
                         GtkGizmoMeasureFunc    measure_func,
                         GtkGizmoAllocateFunc   allocate_func,
                         GtkGizmoSnapshotFunc   snapshot_func,
                         GtkGizmoContainsFunc   contains_func,
                         GtkGizmoFocusFunc      focus_func,
                         GtkGizmoGrabFocusFunc  grab_focus_func)
{
  GtkGizmo *gizmo = GTK_GIZMO (g_object_new (GTK_TYPE_GIZMO,
                                             "css-name", css_name,
                                             "accessible-role", role,
                                             NULL));

  gizmo->measure_func  = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;
  gizmo->contains_func = contains_func;
  gizmo->focus_func = focus_func;
  gizmo->grab_focus_func = grab_focus_func;

  return GTK_WIDGET (gizmo);
}
