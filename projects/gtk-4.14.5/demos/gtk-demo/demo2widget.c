#include "demo2widget.h"
#include "demo2layout.h"

struct _Demo2Widget
{
  GtkWidget parent_instance;

  gint64 start_time;
  gint64 end_time;
  float start_position;
  float end_position;
  float start_offset;
  float end_offset;
  gboolean animating;
};

struct _Demo2WidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Demo2Widget, demo2_widget, GTK_TYPE_WIDGET)

static void
demo2_widget_init (Demo2Widget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

static void
demo2_widget_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (demo2_widget_parent_class)->dispose (object);
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static double
ease_out_cubic (double t)
{
  double p = t - 1;

  return p * p * p + 1;
}

static gboolean
update_position (GtkWidget     *widget,
                 GdkFrameClock *clock,
                 gpointer       data)
{
  Demo2Widget *self = DEMO2_WIDGET (widget);
  Demo2Layout *layout = DEMO2_LAYOUT (gtk_widget_get_layout_manager (widget));
  gint64 now;
  double t;

  now = gdk_frame_clock_get_frame_time (clock);

  if (now >= self->end_time)
    {
      self->animating = FALSE;

      return G_SOURCE_REMOVE;
    }

  t = (now - self->start_time) / (double) (self->end_time - self->start_time);

  t = ease_out_cubic (t);

  demo2_layout_set_position (layout, self->start_position + t * (self->end_position - self->start_position));
  demo2_layout_set_offset (layout, self->start_offset + t * (self->end_offset - self->start_offset));
  gtk_widget_queue_allocate (widget);

  return G_SOURCE_CONTINUE;
}

static void
rotate_sphere (GtkWidget  *widget,
               const char *action,
               GVariant   *parameters)
{
  Demo2Widget *self = DEMO2_WIDGET (widget);
  Demo2Layout *layout = DEMO2_LAYOUT (gtk_widget_get_layout_manager (widget));
  GtkOrientation orientation;
  int direction;

  g_variant_get (parameters, "(ii)", &orientation, &direction);

  self->end_position = self->start_position = demo2_layout_get_position (layout);
  self->end_offset = self->start_offset = demo2_layout_get_offset (layout);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    self->end_position += 10 * direction;
  else
    self->end_offset += 10 * direction;
  self->start_time = g_get_monotonic_time ();
  self->end_time = self->start_time + 0.5 * G_TIME_SPAN_SECOND;

  if (!self->animating)
    {
      gtk_widget_add_tick_callback (widget, update_position, NULL, NULL);
      self->animating = TRUE;
    }
}

static void
demo2_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      /* our layout manager sets this for children that are out of view */
      if (!gtk_widget_get_child_visible (child))
        continue;

      gtk_widget_snapshot_child (widget, child, snapshot);
    }
}

static void
demo2_widget_class_init (Demo2WidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo2_widget_dispose;

  widget_class->snapshot = demo2_widget_snapshot;

  gtk_widget_class_install_action (widget_class, "rotate", "(ii)", rotate_sphere);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Left, 0,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Right, 0,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Up, 0,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Down, 0,
                                       "rotate",
                                       "(ii)", GTK_ORIENTATION_VERTICAL, -1);

  /* here is where we use our custom layout manager */
  gtk_widget_class_set_layout_manager_type (widget_class, DEMO2_TYPE_LAYOUT);
}

GtkWidget *
demo2_widget_new (void)
{
  return g_object_new (DEMO2_TYPE_WIDGET, NULL);
}

void
demo2_widget_add_child (Demo2Widget *self,
                        GtkWidget   *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}
