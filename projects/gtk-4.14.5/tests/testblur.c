
#include <gtk/gtk.h>



struct _GtkBlurBox
{
  GtkBox parent_instance;

  double radius;
};
typedef struct _GtkBlurBox GtkBlurBox;

struct _GtkBlurBoxClass
{
  GtkBoxClass parent_class;
};
typedef struct _GtkBlurBoxClass GtkBlurBoxClass;

static GType gtk_blur_box_get_type (void);
G_DEFINE_TYPE (GtkBlurBox, gtk_blur_box, GTK_TYPE_BOX)


static void
snapshot_blur (GtkWidget   *widget,
               GtkSnapshot *snapshot)
{
  GtkBlurBox *box = (GtkBlurBox *) widget;

  gtk_snapshot_push_blur (snapshot, box->radius);

  GTK_WIDGET_CLASS (gtk_blur_box_parent_class)->snapshot (widget, snapshot);

  gtk_snapshot_pop (snapshot);
}


static void
gtk_blur_box_init (GtkBlurBox *box) {
  box->radius = 0;
}

static void
gtk_blur_box_class_init (GtkBlurBoxClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->snapshot = snapshot_blur;
}

static void
value_changed_cb (GtkRange *range,
                  gpointer  user_data)
{
  GtkBlurBox *box = user_data;
  double value = gtk_range_get_value (range);

  box->radius = value;
  gtk_widget_queue_draw (GTK_WIDGET (box));
}

static void
value_changed_cb2 (GtkRange *range,
                   gpointer  user_data)
{
  GtkLabel *label = user_data;
  double value = gtk_range_get_value (range);
  char *text;

  text = g_strdup_printf ("%.2f", value);
  gtk_label_set_label (label, text);
  g_free (text);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *blur_box;
  GtkWidget *scale;
  GtkWidget *value_label;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  blur_box = g_object_new (gtk_blur_box_get_type (),
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "spacing", 32,
                           NULL);

  value_label = gtk_label_new ("FF");
  gtk_widget_set_margin_top (value_label, 32);
  {
    PangoAttrList *attrs;

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_scale_new (6.0));
    gtk_label_set_attributes (GTK_LABEL (value_label), attrs);
    pango_attr_list_unref (attrs);
  }
  gtk_box_append (GTK_BOX (blur_box), value_label);


  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 10, 0.05);
  gtk_widget_set_size_request (scale, 200, -1);
  gtk_widget_set_halign (scale, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (scale, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (scale, TRUE);
  g_signal_connect (scale, "value-changed", G_CALLBACK (value_changed_cb), blur_box);
  g_signal_connect (scale, "value-changed", G_CALLBACK (value_changed_cb2), value_label);

  gtk_box_append (GTK_BOX (blur_box), scale);
  gtk_window_set_child (GTK_WINDOW (window), blur_box);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
