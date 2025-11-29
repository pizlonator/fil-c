#include "demochild.h"

/* This is a trivial child widget just for demo purposes.
 * It draws a 32x32 square in fixed color.
 */

struct _DemoChild
{
  GtkWidget parent_instance;
  GdkRGBA color;
};

struct _DemoChildClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoChild, demo_child, GTK_TYPE_WIDGET)

static void
demo_child_init (DemoChild *self)
{
}

static void
demo_child_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  DemoChild *self = DEMO_CHILD (widget);
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_append_color (snapshot, &self->color,
                             &GRAPHENE_RECT_INIT(0, 0, width, height));
}

static void
demo_child_measure (GtkWidget        *widget,
                    GtkOrientation    orientation,
                    int               for_size,
                    int              *minimum,
                    int              *natural,
                    int              *minimum_baseline,
                    int              *natural_baseline)
{
  *minimum = *natural = 32;
}

static void
demo_child_class_init (DemoChildClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = demo_child_snapshot;
  widget_class->measure = demo_child_measure;
}

GtkWidget *
demo_child_new (const char *color)
{
  DemoChild *self;

  self = g_object_new (DEMO_TYPE_CHILD,
                       "tooltip-text", color,
                       NULL);

  gdk_rgba_parse (&self->color, color);

  return GTK_WIDGET (self);
}
