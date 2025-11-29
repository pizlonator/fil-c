#include "demowidget.h"
#include "demolayout.h"

/* parent widget */

struct _DemoWidget
{
  GtkWidget parent_instance;

  gboolean backward; /* whether we go 0 -> 1 or 1 -> 0 */
  gint64 start_time; /* time the transition started */
  guint tick_id;     /* our tick cb */
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

/* The widget is controlling the transition by calling
 * demo_layout_set_position() in a tick callback.
 *
 * We take half a second to go from one layout to the other.
 */

#define DURATION (0.5 * G_TIME_SPAN_SECOND)

static gboolean
transition (GtkWidget     *widget,
            GdkFrameClock *frame_clock,
            gpointer       data)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  DemoLayout *demo_layout = DEMO_LAYOUT (gtk_widget_get_layout_manager (widget));
  gint64 now = gdk_frame_clock_get_frame_time (frame_clock);

  gtk_widget_queue_allocate (widget);

  if (self->backward)
    demo_layout_set_position (demo_layout, 1.0 - (now - self->start_time) / DURATION);
  else
    demo_layout_set_position (demo_layout, (now - self->start_time) / DURATION);

  if (now - self->start_time >= DURATION)
    {
      self->backward = !self->backward;
      demo_layout_set_position (demo_layout, self->backward ? 1.0 : 0.0);
      /* keep things interesting by shuffling the positions */
      if (!self->backward)
        demo_layout_shuffle (demo_layout);
      self->tick_id = 0;

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
clicked (GtkGestureClick *gesture,
         guint            n_press,
         double           x,
         double           y,
         gpointer         data)
{
  DemoWidget *self = data;
  GdkFrameClock *frame_clock;

  if (self->tick_id != 0)
    return;

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));
  self->start_time = gdk_frame_clock_get_frame_time (frame_clock);
  self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), transition, NULL, NULL);
}

static void
demo_widget_init (DemoWidget *self)
{
  GtkGesture *gesture;

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (clicked), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

static void
demo_widget_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  /* here is where we use our custom layout manager */
  gtk_widget_class_set_layout_manager_type (widget_class, DEMO_TYPE_LAYOUT);
}

GtkWidget *
demo_widget_new (void)
{
  return g_object_new (DEMO_TYPE_WIDGET, NULL);
}

void
demo_widget_add_child (DemoWidget *self,
                       GtkWidget  *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}
