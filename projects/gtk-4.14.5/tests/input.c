/* Demonstrates hooking up an input method context to a custom widget.
 */
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;

  GtkIMContext *im_context;
  PangoLayout *layout;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
commit_cb (GtkIMContext *context,
           const char   *str,
           DemoWidget   *demo)
{
  pango_layout_set_text (demo->layout, str, -1);
  pango_layout_set_attributes (demo->layout, NULL);
  gtk_widget_queue_draw (GTK_WIDGET (demo));
}

static void
preedit_changed_cb (GtkIMContext *context,
                    DemoWidget   *demo)
{
  char *str;
  PangoAttrList *attrs;
  int cursor_pos;

  gtk_im_context_get_preedit_string (context, &str, &attrs, &cursor_pos);
  pango_layout_set_text (demo->layout, str, -1);
  pango_layout_set_attributes (demo->layout, attrs);
  g_free (str);
  pango_attr_list_unref (attrs);

  gtk_widget_queue_draw (GTK_WIDGET (demo));
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                DemoWidget            *demo)
{
  if (keyval == GDK_KEY_BackSpace)
    {
      pango_layout_set_text (demo->layout, "", -1);
      pango_layout_set_attributes (demo->layout, NULL);
      gtk_widget_queue_draw (GTK_WIDGET (demo));

      return TRUE;
    }

  return FALSE;
}

static void
demo_widget_init (DemoWidget *demo)
{
  GtkEventController *controller;

  gtk_widget_set_focusable (GTK_WIDGET (demo), TRUE);

  demo->layout = gtk_widget_create_pango_layout (GTK_WIDGET (demo), "");

  demo->im_context = gtk_im_multicontext_new ();

  g_signal_connect (demo->im_context, "commit", G_CALLBACK (commit_cb), demo);
  g_signal_connect (demo->im_context, "preedit-changed", G_CALLBACK (preedit_changed_cb), demo);

  controller = gtk_event_controller_key_new ();
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (controller),
                                           demo->im_context);

  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed_cb), demo);

  gtk_widget_add_controller (GTK_WIDGET (demo), controller);
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *demo = DEMO_WIDGET (object);

  g_clear_object (&demo->layout);
  g_clear_object (&demo->im_context);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *demo = DEMO_WIDGET (widget);

  gtk_snapshot_render_layout (snapshot,
                              gtk_widget_get_style_context (widget),
                              0, 0,
                              demo->layout);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  widget_class->snapshot = demo_widget_snapshot;
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (demo_widget_get_type (), NULL);
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *demo;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());

  demo = demo_widget_new ();

  gtk_window_set_child (window, demo);

  gtk_window_present (window);

  gtk_widget_grab_focus (demo);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
