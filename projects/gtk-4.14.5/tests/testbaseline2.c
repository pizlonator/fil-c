#include <gtk/gtk.h>

#define BASELINE_TYPE_WIDGET (baseline_widget_get_type ())
G_DECLARE_FINAL_TYPE (BaselineWidget, baseline_widget, BASELINE, WIDGET, GtkWidget)

enum
{
  PROP_ABOVE = 1,
  PROP_BELOW,
  PROP_ACROSS
};

struct _BaselineWidget
{
  GtkWidget parent_instance;

  int above, below, across;
};

struct _BaselineWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (BaselineWidget, baseline_widget, GTK_TYPE_WIDGET)

static void
baseline_widget_init (BaselineWidget *self)
{
}

static void
baseline_widget_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BaselineWidget *self = BASELINE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_ABOVE:
      self->above = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_BELOW:
      self->below = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    case PROP_ACROSS:
      self->across = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
baseline_widget_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
  BaselineWidget *self = BASELINE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_ABOVE:
      g_value_set_int (value, self->above);
      break;

    case PROP_BELOW:
      g_value_set_int (value, self->below);
      break;

    case PROP_ACROSS:
      g_value_set_int (value, self->across);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
baseline_widget_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  BaselineWidget *self = BASELINE_WIDGET (widget);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (self->below >= 0)
        {
          *minimum = *natural = self->above + self->below;
          *minimum_baseline = *natural_baseline = self->above;
        }
      else
        {
          *minimum = *natural = self->above;
          *minimum_baseline = *natural_baseline = -1;
        }
    }
  else
    {
      *minimum = *natural = self->across;
      *minimum_baseline = *natural_baseline = -1;
    }
}

static void
baseline_widget_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  BaselineWidget *self = BASELINE_WIDGET (widget);
  int height, baseline;
  graphene_rect_t bounds;
  float widths[4];
  GdkRGBA colors[4];
  GskRoundedRect outline;

  height = gtk_widget_get_height (widget);
  baseline = gtk_widget_get_baseline (widget);

  gtk_snapshot_save (snapshot);

  if (baseline > -1)
    {
      int y;
      if (self->below >= 0)
        y = MAX (baseline - self->above, 0);
      else
        y = CLAMP (baseline - self->above / 2, 0, height - self->above);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, y));
    }

  bounds.origin.x = 0;
  bounds.origin.y = 0;
  bounds.size.width = self->across;
  bounds.size.height = self->above;

  if (self->below >= 0)
    gtk_snapshot_append_color (snapshot, &(GdkRGBA){1, 1, 0, 0.2}, &bounds);
  else
    gtk_snapshot_append_color (snapshot, &(GdkRGBA){0, 1, 0, 0.2}, &bounds);

  outline = GSK_ROUNDED_RECT_INIT (0, 0, self->across, self->above);
  for (int i = 0; i < 4; i++)
    {
      widths[i] = 1.;
      gdk_rgba_parse (&colors[i], "black");
    }

  gtk_snapshot_append_border (snapshot, &outline, widths, colors);

  gtk_snapshot_restore (snapshot);

  if (self->below >= 0)
    {
      gtk_snapshot_save (snapshot);

      if (baseline > -1)
        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, baseline));
      else
        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, self->above));
      bounds.origin.x = 0;
      bounds.origin.y = 0;
      bounds.size.width = self->across;
      bounds.size.height = self->below;
      gtk_snapshot_append_color (snapshot, &(GdkRGBA){0, 0, 1, 0.2}, &bounds);

      outline = GSK_ROUNDED_RECT_INIT (0, 0, self->across, self->below);
      for (int i = 0; i < 4; i++)
        {
          widths[i] = 1.;
          gdk_rgba_parse (&colors[i], "black");
        }

      gtk_snapshot_append_border (snapshot, &outline, widths, colors);

      gtk_snapshot_restore (snapshot);
    }
}


static void
baseline_widget_class_init (BaselineWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = baseline_widget_set_property;
  object_class->get_property = baseline_widget_get_property;

  widget_class->snapshot = baseline_widget_snapshot;
  widget_class->measure = baseline_widget_measure;

  g_object_class_install_property (object_class, PROP_ABOVE,
    g_param_spec_int ("above", NULL, NULL, 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_BELOW,
    g_param_spec_int ("below", NULL, NULL, -1, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_ACROSS,
    g_param_spec_int ("across", NULL, NULL, 0, G_MAXINT, 0, G_PARAM_READWRITE));
}

static GtkWidget *
baseline_widget_new (int above, int below, int across)
{
  return g_object_new (BASELINE_TYPE_WIDGET,
                       "above", above,
                       "below", below,
                       "across", across,
                       "valign", GTK_ALIGN_BASELINE_CENTER,
                       NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *header;
  GtkWidget *stack, *switcher;
  GtkWidget *box, *box1;
  GtkWidget *grid;
  GtkWidget *child;

  gtk_init ();

  window = gtk_window_new ();

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  stack = gtk_stack_new ();
  gtk_window_set_child (GTK_WINDOW (window), stack);

  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), switcher);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_valign (box, GTK_ALIGN_BASELINE_CENTER);

  gtk_stack_add_titled (GTK_STACK (stack), box, "boxes", "Boxes");

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (box1, GTK_ALIGN_FILL);
  gtk_widget_set_valign (box1, GTK_ALIGN_BASELINE_CENTER);
  gtk_widget_set_hexpand (box1, TRUE);

  child = baseline_widget_new (20, 10, 20);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (5, 20, 20);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (25, -1, 20);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (25, 20, 30);
  gtk_box_append (GTK_BOX (box1), child);

  gtk_box_append (GTK_BOX (box), box1);

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (box1, GTK_ALIGN_FILL);
  gtk_widget_set_valign (box1, GTK_ALIGN_BASELINE_CENTER);
  gtk_widget_set_hexpand (box1, TRUE);

  child = baseline_widget_new (10, 15, 10);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (80, -1, 20);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (60, 15, 20);
  gtk_box_append (GTK_BOX (box1), child);

  child = baseline_widget_new (5, 10, 30);
  gtk_box_append (GTK_BOX (box1), child);

  gtk_box_append (GTK_BOX (box), box1);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_valign (box, GTK_ALIGN_BASELINE_CENTER);

  grid = gtk_grid_new ();
  gtk_widget_set_valign (grid, GTK_ALIGN_BASELINE_CENTER);
  gtk_widget_set_hexpand (grid, TRUE);

  child = baseline_widget_new (20, 10, 20);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);

  child = baseline_widget_new (5, 20, 20);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 0, 1, 1);

  child = baseline_widget_new (25, -1, 20);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 1, 1, 1);

  child = baseline_widget_new (25, 20, 30);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 1, 1, 1);

  gtk_box_append (GTK_BOX (box), grid);

  grid = gtk_grid_new ();
  gtk_widget_set_valign (grid, GTK_ALIGN_BASELINE_CENTER);
  gtk_widget_set_hexpand (grid, TRUE);

  child = baseline_widget_new (10, 15, 10);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 0, 1, 1);

  child = baseline_widget_new (80, -1, 20);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 0, 1, 1);

  child = baseline_widget_new (60, 15, 20);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 1, 1, 1);

  child = baseline_widget_new (5, 10, 30);
  gtk_grid_attach (GTK_GRID (grid), child, 1, 1, 1, 1);

  gtk_box_append (GTK_BOX (box), grid);

  gtk_stack_add_titled (GTK_STACK (stack), box, "grids", "Grids");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_valign (box, GTK_ALIGN_BASELINE_CENTER);

  child = baseline_widget_new (60, 15, 20);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("Label");
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (child), "Entry");
  gtk_editable_set_width_chars (GTK_EDITABLE (child), 10);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_password_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (child), "Password");
  gtk_editable_set_width_chars (GTK_EDITABLE (child), 10);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_spin_button_new_with_range (0, 100, 1);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_spin_button_new_with_range (0, 100, 1);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (child), GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_switch_new ();
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_widget_set_size_request (child, 100, -1);
  //gtk_scale_add_mark (GTK_SCALE (child), 50, GTK_POS_BOTTOM, "50");
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  gtk_stack_add_titled (GTK_STACK (stack), box, "controls", "Controls");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_valign (box, GTK_ALIGN_BASELINE_CENTER);

  child = baseline_widget_new (60, 15, 20);
  gtk_widget_set_hexpand (child, TRUE);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("Label");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("Two\nlines");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("<span size='large'>Large</span>");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("<span size='xx-large'>Huge</span>");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("<span underline='double'>Underlined</span>");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new ("♥️");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new_from_icon_name ("edit-copy-symbolic");
  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE_CENTER);
  gtk_box_append (GTK_BOX (box), child);

  gtk_stack_add_titled (GTK_STACK (stack), box, "labels", "Labels");

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
