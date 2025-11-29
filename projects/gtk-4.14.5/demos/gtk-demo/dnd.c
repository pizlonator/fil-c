/* Drag-and-Drop
 * #Keywords: dnd, menu, popover, gesture
 *
 * This demo shows dragging colors and widgets.
 * The items in this demo can be moved, recolored
 * and rotated.
 *
 * The demo also has an example for creating a
 * menu-like popover without using a menu model.
 */

#include <gtk/gtk.h>


G_DECLARE_FINAL_TYPE (CanvasItem, canvas_item, CANVAS, ITEM, GtkWidget)

struct _CanvasItem {
  GtkWidget parent;

  GtkWidget *fixed;
  GtkWidget *label;

  double r;
  double angle;
  double delta;

  GtkWidget *editor;

  GtkStyleProvider *provider;
  char *css_class;
};

struct _CanvasItemClass {
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CanvasItem, canvas_item, GTK_TYPE_WIDGET)

static int n_items = 0;

static void
unstyle_item (CanvasItem *item)
{
  if (item->provider)
    {
      gtk_style_context_remove_provider_for_display (gtk_widget_get_display (item->label), item->provider);
      g_clear_object (&item->provider);
    }

  if (item->css_class)
    {
      gtk_widget_remove_css_class (item->label, item->css_class);
      g_clear_pointer (&item->css_class, g_free);
    }
}

static void
set_color (CanvasItem *item,
           GdkRGBA    *color)
{
  char *css;
  char *str;
  GtkCssProvider *provider;
  const char *name;

  unstyle_item (item);

  str = gdk_rgba_to_string (color);
  name = gtk_widget_get_name (item->label);
  css = g_strdup_printf ("#%s { background: %s; }", name, str);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (item->label), GTK_STYLE_PROVIDER (provider), 700);
  item->provider = GTK_STYLE_PROVIDER (provider);

  g_free (str);
  g_free (css);
}

static void
set_css (CanvasItem *item,
         const char *class)
{
  unstyle_item (item);

  gtk_widget_add_css_class (item->label, class);
  item->css_class = g_strdup (class);
}

static gboolean
item_drag_drop (GtkDropTarget *dest,
                const GValue  *value,
                double         x,
                double         y)
{
  GtkWidget *label = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  CanvasItem *item = CANVAS_ITEM (gtk_widget_get_parent (gtk_widget_get_parent (label)));

  if (G_VALUE_TYPE (value) == GDK_TYPE_RGBA)
    set_color (item, g_value_get_boxed (value));
  else if (G_VALUE_TYPE (value) == G_TYPE_STRING)
    set_css (item, g_value_get_string (value));

  return TRUE;
}

static void
apply_transform (CanvasItem *item)
{
  GskTransform *transform;
  graphene_rect_t bounds;
  double x, y;

  /* Add css padding and margin */
  if (!gtk_widget_compute_bounds (item->label, item->label, &bounds))
    return;

  x = bounds.size.width / 2.;
  y = bounds.size.height / 2.;

  item->r = sqrt (x * x + y * y);

  transform = gsk_transform_translate (NULL, &(graphene_point_t) { item->r, item->r });
  transform = gsk_transform_rotate (transform, item->angle + item->delta);
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-x, -y));

  gtk_fixed_set_child_transform (GTK_FIXED (item->fixed), item->label, transform);
  gsk_transform_unref (transform);
}

static void
angle_changed (GtkGestureRotate *gesture,
               double            angle,
               double            delta)
{
  CanvasItem *item = CANVAS_ITEM (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  item->delta = angle / M_PI * 180.0;

  apply_transform (item);
}

static void
rotate_done (GtkGesture *gesture)
{
  CanvasItem *item = CANVAS_ITEM (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  item->angle = item->angle + item->delta;
  item->delta = 0;
}

static void
click_done (GtkGesture *gesture)
{
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  GtkWidget *canvas = gtk_widget_get_parent (item);
  GtkWidget *last_child;

  last_child = gtk_widget_get_last_child (canvas);
  if (item != last_child)
    gtk_widget_insert_after (item, canvas, last_child);
}

/* GtkSettings treats `GTK_THEME=foo:dark` as theme name `foo`, variant `dark`,
 * and our embedded CSS files let `foo-dark` work as an alias for `foo:dark`. */
static gboolean
has_dark_suffix (const char *theme)
{
  return g_str_has_suffix (theme, ":dark") ||
         g_str_has_suffix (theme, "-dark");
}

/* So we can make a good guess whether the current theme is dark by checking for
 * either: it is suffixed `[:-]dark`, or Settings:…prefer-dark-theme is TRUE. */
static gboolean
theme_is_dark (void)
{
  const char *env_theme;
  GtkSettings *settings;
  char *theme;
  gboolean prefer_dark;
  gboolean dark;

  /* Like GtkSettings, 1st see if theme is overridden by environment variable */
  env_theme = g_getenv ("GTK_THEME");
  if (env_theme != NULL)
    return has_dark_suffix (env_theme);

  /* If not, test Settings:…theme-name in the same way OR :…prefer-dark-theme */
  settings = gtk_settings_get_default ();
  g_object_get (settings,
                "gtk-theme-name", &theme,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);
  dark = prefer_dark || has_dark_suffix (theme);
  g_free (theme);
  return dark;
}

static void
canvas_item_init (CanvasItem *item)
{
  char *text;
  char *id;
  GdkRGBA rgba;
  GtkDropTarget *dest;
  GtkGesture *gesture;
  GType types[2] = { GDK_TYPE_RGBA, G_TYPE_STRING };

  n_items++;

  text = g_strdup_printf ("Item %d", n_items);
  item->label = gtk_label_new (text);
  gtk_widget_add_css_class (item->label, "canvasitem");
  g_free (text);

  item->fixed = gtk_fixed_new ();
  gtk_widget_set_parent (item->fixed, GTK_WIDGET (item));
  gtk_fixed_put (GTK_FIXED (item->fixed), item->label, 0, 0);

  gtk_widget_add_css_class (item->label, "frame");

  id = g_strdup_printf ("item%d", n_items);
  gtk_widget_set_name (item->label, id);
  g_free (id);

  if (theme_is_dark ())
    gdk_rgba_parse (&rgba, "blue");
  else
    gdk_rgba_parse (&rgba, "yellow");

  set_color (item, &rgba);

  item->angle = 0;

  dest = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);
  gtk_drop_target_set_gtypes (dest, types, G_N_ELEMENTS (types));
  g_signal_connect (dest, "drop", G_CALLBACK (item_drag_drop), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item->label), GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_rotate_new ();
  g_signal_connect (gesture, "angle-changed", G_CALLBACK (angle_changed), NULL);
  g_signal_connect (gesture, "end", G_CALLBACK (rotate_done), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (click_done), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), GTK_EVENT_CONTROLLER (gesture));
}

static void
canvas_item_dispose (GObject *object)
{
  CanvasItem *item = CANVAS_ITEM (object);

  g_clear_pointer (&item->fixed, gtk_widget_unparent);
  g_clear_pointer (&item->editor, gtk_widget_unparent);

  G_OBJECT_CLASS (canvas_item_parent_class)->dispose (object);
}

static void
canvas_item_map (GtkWidget *widget)
{
  CanvasItem *item = CANVAS_ITEM (widget);

  GTK_WIDGET_CLASS (canvas_item_parent_class)->map (widget);

  apply_transform (item);
}

static void
canvas_item_class_init (CanvasItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = canvas_item_dispose;

  widget_class->map = canvas_item_map;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "item");
}

static GtkWidget *
canvas_item_new (void)
{
  CanvasItem *item = g_object_new (canvas_item_get_type (), NULL);

  return GTK_WIDGET (item);
}

static GdkPaintable *
canvas_item_get_drag_icon (CanvasItem *item)
{
  return gtk_widget_paintable_new (item->fixed);
}

static gboolean
canvas_item_is_editing (CanvasItem *item)
{
  return item->editor != NULL;
}

static void
scale_changed (GtkRange   *range,
               CanvasItem *item)
{
  item->angle = gtk_range_get_value (range);
  apply_transform (item);
}

static void
text_changed (GtkEditable *editable,
              GParamSpec  *pspec,
              CanvasItem  *item)
{
  gtk_label_set_text (GTK_LABEL (item->label), gtk_editable_get_text (editable));
  apply_transform (item);
}

static void
canvas_item_stop_editing (CanvasItem *item)
{
  GtkWidget *scale;

  if (!item->editor)
    return;

  scale = gtk_widget_get_last_child (item->editor);
  g_signal_handlers_disconnect_by_func (scale, scale_changed, item);

  gtk_fixed_remove (GTK_FIXED (gtk_widget_get_parent (item->editor)), item->editor);
  item->editor = NULL;
}

static void
canvas_item_start_editing (CanvasItem *item)
{
  GtkWidget *canvas = gtk_widget_get_parent (GTK_WIDGET (item));
  GtkWidget *entry;
  GtkWidget *scale;
  graphene_point_t p;

  if (item->editor)
    return;

  item->editor = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  entry = gtk_entry_new ();

  gtk_editable_set_text (GTK_EDITABLE (entry),
                         gtk_label_get_text (GTK_LABEL (item->label)));

  gtk_editable_set_width_chars (GTK_EDITABLE (entry), 12);
  g_signal_connect (entry, "notify::text", G_CALLBACK (text_changed), item);
  g_signal_connect_swapped (entry, "activate", G_CALLBACK (canvas_item_stop_editing), item);

  gtk_box_append (GTK_BOX (item->editor), entry);

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 360, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_range_set_value (GTK_RANGE (scale), fmod (item->angle, 360));

  g_signal_connect (scale, "value-changed", G_CALLBACK (scale_changed), item);

  gtk_box_append (GTK_BOX (item->editor), scale);

  if (!gtk_widget_compute_point (GTK_WIDGET (item), canvas, &GRAPHENE_POINT_INIT (0, 0), &p))
    graphene_point_init (&p, 0, 0);
  gtk_fixed_put (GTK_FIXED (canvas), item->editor, p.x, p.y + 2 * item->r);
  gtk_widget_grab_focus (entry);

}

typedef struct {
  double x, y;
} Hotspot;

static GdkContentProvider *
prepare (GtkDragSource *source,
         double         x,
         double         y)
{
  GtkWidget *canvas;
  GtkWidget *item;
  Hotspot *hotspot;
  graphene_point_t p;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = gtk_widget_pick (canvas, x, y, GTK_PICK_DEFAULT);

  item = gtk_widget_get_ancestor (item, canvas_item_get_type ());
  if (!item)
    return NULL;

  g_object_set_data (G_OBJECT (canvas), "dragged-item", item);

  hotspot = g_new (Hotspot, 1);
  if (!gtk_widget_compute_point (canvas, item, &GRAPHENE_POINT_INIT (x, y), &p))
    graphene_point_init (&p, x, y);
  hotspot->x = p.x;
  hotspot->y = p.y;
  g_object_set_data_full (G_OBJECT (canvas), "hotspot", hotspot, g_free);

  return gdk_content_provider_new_typed (GTK_TYPE_WIDGET, item);
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag)
{
  GtkWidget *canvas;
  CanvasItem *item;
  GdkPaintable *paintable;
  Hotspot *hotspot;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = CANVAS_ITEM (g_object_get_data (G_OBJECT (canvas), "dragged-item"));
  hotspot = (Hotspot *) g_object_get_data (G_OBJECT (canvas), "hotspot");

  paintable = canvas_item_get_drag_icon (item);
  gtk_drag_source_set_icon (source, paintable, hotspot->x, hotspot->y);
  g_object_unref (paintable);

  gtk_widget_set_opacity (GTK_WIDGET (item), 0.3);
}

static void
drag_end (GtkDragSource *source,
          GdkDrag       *drag)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");
  g_object_set_data (G_OBJECT (canvas), "dragged-item", NULL);

  gtk_widget_set_opacity (item, 1.0);
}

static gboolean
drag_cancel (GtkDragSource       *source,
             GdkDrag             *drag,
             GdkDragCancelReason  reason)
{
  return FALSE;
}

static gboolean
drag_drop (GtkDropTarget *target,
           const GValue  *value,
           double         x,
           double         y)
{
  CanvasItem *item;
  GtkWidget *canvas;
  GtkWidget *last_child;

  item = g_value_get_object (value);

  canvas = gtk_widget_get_parent (GTK_WIDGET (item));
  last_child = gtk_widget_get_last_child (canvas);
  if (GTK_WIDGET (item) != last_child)
    gtk_widget_insert_after (GTK_WIDGET (item), canvas, last_child);

  gtk_fixed_move (GTK_FIXED (canvas), GTK_WIDGET (item), x - item->r, y - item->r);

  return TRUE;
}

static void
new_item_cb (GtkWidget *button, gpointer data)
{
  GtkWidget *canvas = data;
  GtkWidget *popover;
  GtkWidget *item;
  GdkRectangle rect;

  popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  gtk_popover_get_pointing_to (GTK_POPOVER (popover), &rect);

  item = canvas_item_new ();
  gtk_fixed_put (GTK_FIXED (canvas), item, rect.x, rect.y);
  apply_transform (CANVAS_ITEM (item));

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
edit_cb (GtkWidget *button, GtkWidget *child)
{
  CanvasItem *item = CANVAS_ITEM (child);

  if (button)
    gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));

  if (!canvas_item_is_editing (item))
    canvas_item_start_editing (item);
}

static void
delete_cb (GtkWidget *button, GtkWidget *child)
{
  GtkWidget *canvas = gtk_widget_get_parent (child);

  gtk_fixed_remove (GTK_FIXED (canvas), child);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);
  child = gtk_widget_get_ancestor (child, canvas_item_get_type ());

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *menu;
      GtkWidget *box;
      GtkWidget *item;

      menu = gtk_popover_new ();
      gtk_widget_set_parent (menu, widget);
      gtk_popover_set_has_arrow (GTK_POPOVER (menu), FALSE);
      gtk_popover_set_pointing_to (GTK_POPOVER (menu), &(GdkRectangle){ x, y, 1, 1});
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_popover_set_child (GTK_POPOVER (menu), box);

      item = gtk_button_new_with_label ("New");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      g_signal_connect (item, "clicked", G_CALLBACK (new_item_cb), widget);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_button_new_with_label ("Edit");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (edit_cb), child);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_button_new_with_label ("Delete");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (delete_cb), child);
      gtk_box_append (GTK_BOX (box), item);

      gtk_popover_popup (GTK_POPOVER (menu));
    }
}

static void
released_cb (GtkGesture *gesture,
             int         n_press,
             double      x,
             double      y,
             gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;
  CanvasItem *item;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, 0);
  item = (CanvasItem *)gtk_widget_get_ancestor (child, canvas_item_get_type ());
  if (!item)
    return;

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_PRIMARY)
    {
      if (canvas_item_is_editing (item))
        canvas_item_stop_editing (item);
      else
        canvas_item_start_editing (item);
    }
}

static GtkWidget *
canvas_new (void)
{
  GtkWidget *canvas;
  GtkDragSource *source;
  GtkDropTarget *dest;
  GtkGesture *gesture;

  canvas = gtk_fixed_new ();
  gtk_widget_set_hexpand (canvas, TRUE);
  gtk_widget_set_vexpand (canvas, TRUE);

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect (source, "prepare", G_CALLBACK (prepare), NULL);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  g_signal_connect (source, "drag-cancel", G_CALLBACK (drag_cancel), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (source));

  dest = gtk_drop_target_new (GTK_TYPE_WIDGET, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drop", G_CALLBACK (drag_drop), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed", G_CALLBACK (pressed_cb), NULL);
  g_signal_connect (gesture, "released", G_CALLBACK (released_cb), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (gesture));

  return canvas;
}

static GdkContentProvider *
css_drag_prepare (GtkDragSource *source,
                  double         x,
                  double         y,
                  GtkWidget     *button)
{
  const char *class;
  GdkPaintable *paintable;

  class = (const char *)g_object_get_data (G_OBJECT (button), "css-class");

  paintable = gtk_widget_paintable_new (button);
  gtk_drag_source_set_icon (source, paintable, 0, 0);
  g_object_unref (paintable);

  return gdk_content_provider_new_typed (G_TYPE_STRING, class);
}

static GtkWidget *
css_button_new (const char *class)
{
  GtkWidget *button;
  GtkDragSource *source;

  button = gtk_image_new ();
  gtk_widget_set_size_request (button, 48, 32);
  gtk_widget_add_css_class (button, class);
  g_object_set_data (G_OBJECT (button), "css-class", (gpointer)class);

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (css_drag_prepare), button);
  gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (source));

  return button;
}

typedef struct
{
  GtkWidget parent_instance;
  GdkRGBA color;
} ColorSwatch;

typedef struct
{
  GtkWidgetClass parent_class;
} ColorSwatchClass;

G_DEFINE_TYPE (ColorSwatch, color_swatch, GTK_TYPE_WIDGET)

static GdkContentProvider *
color_swatch_drag_prepare (GtkDragSource  *source,
                           double          x,
                           double          y,
                           ColorSwatch    *swatch)
{
  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &swatch->color);
}

static void
color_swatch_init (ColorSwatch *swatch)
{
  GtkDragSource *source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (color_swatch_drag_prepare), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (source));
}

static void
color_swatch_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  ColorSwatch *swatch = (ColorSwatch *)widget;
  float w = gtk_widget_get_width (widget);
  float h = gtk_widget_get_height (widget);

  gtk_snapshot_append_color (snapshot, &swatch->color,
                             &GRAPHENE_RECT_INIT(0, 0, w, h));
}

void
color_swatch_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum_size,
                      int            *natural_size,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum_size = *natural_size = 48;
  else
    *minimum_size = *natural_size = 32;
}

static void
color_swatch_class_init (ColorSwatchClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = color_swatch_snapshot;
  widget_class->measure = color_swatch_measure;
  gtk_widget_class_set_css_name (widget_class, "colorswatch");
}

static GtkWidget *
color_swatch_new (const char *color)
{
  ColorSwatch *swatch = g_object_new (color_swatch_get_type (), NULL);

  gdk_rgba_parse (&swatch->color, color);

  return GTK_WIDGET (swatch);
}

static GtkWidget *window = NULL;

GtkWidget *
do_dnd (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *button;
      GtkWidget *sw;
      GtkWidget *canvas;
      GtkWidget *box, *box2, *box3;
      const char *colors[] = {
        "red", "green", "blue", "magenta", "orange", "gray", "black", "yellow",
        "white", "gray", "brown", "pink",  "cyan", "bisque", "gold", "maroon",
        "navy", "orchid", "olive", "peru", "salmon", "silver", "wheat",
        NULL
      };
      int i;
      int x, y;
      GtkCssProvider *provider;
      GString *css;

      button = gtk_color_dialog_button_new (gtk_color_dialog_new ());
      g_object_unref (g_object_ref_sink (button));

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/dnd/dnd.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      css = g_string_new ("");
      for (i = 0; colors[i]; i++)
        g_string_append_printf (css, ".canvasitem.%s { background: %s; }\n", colors[i], colors[i]);

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_string (provider, css->str);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);
      g_string_free (css, TRUE);

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drag-and-Drop");
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box2);

      canvas = canvas_new ();
      gtk_box_append (GTK_BOX (box2), canvas);

      n_items = 0;

      x = y = 40;
      for (i = 0; i < 4; i++)
        {
          GtkWidget *item;

          item = canvas_item_new ();
          gtk_fixed_put (GTK_FIXED (canvas), item, x, y);
          apply_transform (CANVAS_ITEM (item));

          x += 150;
          y += 100;
        }

      gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_NEVER);
      gtk_box_append (GTK_BOX (box), sw);

      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (box3, "linked");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box3);

      for (i = 0; colors[i]; i++)
        gtk_box_append (GTK_BOX (box3), color_swatch_new (colors[i]));

      gtk_box_append (GTK_BOX (box3), css_button_new ("rainbow1"));
      gtk_box_append (GTK_BOX (box3), css_button_new ("rainbow2"));
      gtk_box_append (GTK_BOX (box3), css_button_new ("rainbow3"));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
