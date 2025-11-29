/* Lists/Colors
 * #Keywords: GtkSortListModel, GtkMultiSelection
 *
 * This demo displays a grid of colors.
 *
 * It is using a GtkGridView, and shows how to display
 * and sort the data in various ways. The controls for
 * this are implemented using GtkDropDown.
 *
 * The dataset used here has up to 16 777 216 items.
 *
 * Note that this demo also functions as a performance
 * test for some of the list model machinery, and the
 * biggest sizes here can lock up the application for
 * extended times when used with sorting.
 */

#include <gtk/gtk.h>

#include <stdlib.h>

#define GTK_TYPE_COLOR (gtk_color_get_type ())
G_DECLARE_FINAL_TYPE (GtkColor, gtk_color, GTK, COLOR, GObject)

/* This is our object. It's just a color */
typedef struct _GtkColor GtkColor;
struct _GtkColor
{
  GObject parent_instance;

  char *name;
  GdkRGBA color;
  int h, s, v;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_COLOR,
  PROP_RED,
  PROP_GREEN,
  PROP_BLUE,
  PROP_HUE,
  PROP_SATURATION,
  PROP_VALUE,

  N_COLOR_PROPS
};

static void
gtk_color_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  GtkColor *self = GTK_COLOR (paintable);

  gtk_snapshot_append_color (snapshot, &self->color, &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static int
gtk_color_get_intrinsic_width (GdkPaintable *paintable)
{
  return 32;
}

static int
gtk_color_get_intrinsic_height (GdkPaintable *paintable)
{
  return 32;
}

static void
gtk_color_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_color_snapshot;
  iface->get_intrinsic_width = gtk_color_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_color_get_intrinsic_height;
}

/*
 * Finally, we define the type. The important part is adding the paintable
 * interface, so GTK knows that this object can indeed be drawn.
 */
G_DEFINE_TYPE_WITH_CODE (GtkColor, gtk_color, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_color_paintable_init))

static GParamSpec *color_properties[N_COLOR_PROPS] = { NULL, };

static void
rgb_to_hsv (GdkRGBA *rgba,
            double  *h_out,
            double  *s_out,
            double  *v_out)
{
  double red, green, blue;
  double h, s, v;
  double min, max;
  double delta;

  red = rgba->red;
  green = rgba->green;
  blue = rgba->blue;

  h = 0.0;

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  v = max;

  if (max != 0.0)
    s = (max - min) / max;
  else
    s = 0.0;

  if (s == 0.0)
    h = 0.0;
  else
    {
      delta = max - min;

      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;

      h /= 6.0;

      if (h < 0.0)
        h += 1.0;
      else if (h > 1.0)
        h -= 1.0;
    }

  *h_out = h;
  *s_out = s;
  *v_out = v;
}

static void
gtk_color_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkColor *self = GTK_COLOR (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_RED:
      g_value_set_float (value, self->color.red);
      break;

    case PROP_GREEN:
      g_value_set_float (value, self->color.green);
      break;

    case PROP_BLUE:
      g_value_set_float (value, self->color.blue);
      break;

    case PROP_HUE:
      g_value_set_int (value, self->h);
      break;

    case PROP_SATURATION:
      g_value_set_int (value, self->s);
      break;

    case PROP_VALUE:
      g_value_set_int (value, self->v);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkColor *self = GTK_COLOR (object);
  double h, s, v;

  switch (property_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_COLOR:
      self->color = *(GdkRGBA *) g_value_get_boxed (value);
      rgb_to_hsv (&self->color, &h, &s, &v);
      self->h = round (360 * h);
      self->s = round (100 * s);
      self->v = round (100 * v);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_finalize (GObject *object)
{
  GtkColor *self = GTK_COLOR (object);

  g_free (self->name);

  G_OBJECT_CLASS (gtk_color_parent_class)->finalize (object);
}

static void
gtk_color_class_init (GtkColorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_color_get_property;
  gobject_class->set_property = gtk_color_set_property;
  gobject_class->finalize = gtk_color_finalize;

  color_properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  color_properties[PROP_COLOR] =
    g_param_spec_boxed ("color", NULL, NULL, GDK_TYPE_RGBA, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  color_properties[PROP_RED] =
    g_param_spec_float ("red", NULL, NULL, 0, 1, 0, G_PARAM_READABLE);
  color_properties[PROP_GREEN] =
    g_param_spec_float ("green", NULL, NULL, 0, 1, 0, G_PARAM_READABLE);
  color_properties[PROP_BLUE] =
    g_param_spec_float ("blue", NULL, NULL, 0, 1, 0, G_PARAM_READABLE);
  color_properties[PROP_HUE] =
    g_param_spec_int ("hue", NULL, NULL, 0, 360, 0, G_PARAM_READABLE);
  color_properties[PROP_SATURATION] =
    g_param_spec_int ("saturation", NULL, NULL, 0, 100, 0, G_PARAM_READABLE);
  color_properties[PROP_VALUE] =
    g_param_spec_int ("value", NULL, NULL, 0, 100, 0, G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_COLOR_PROPS, color_properties);
}

static void
gtk_color_init (GtkColor *self)
{
}

static GtkColor *
gtk_color_new (const char *name,
               float r, float g, float b)
{
  GtkColor *result;
  GdkRGBA color = { r, g, b, 1.0 };

  result = g_object_new (GTK_TYPE_COLOR,
                         "name", name,
                         "color", &color,
                         NULL);

  return result;
}

#define N_COLORS (256 * 256 * 256)

#define GTK_TYPE_COLOR_LIST (gtk_color_list_get_type ())
G_DECLARE_FINAL_TYPE (GtkColorList, gtk_color_list, GTK, COLOR_LIST, GObject)

enum {
  LIST_PROP_0,
  LIST_PROP_SIZE,

  N_LIST_PROPS
};

typedef struct _GtkColorList GtkColorList;
struct _GtkColorList
{
  GObject parent_instance;

  GtkColor **colors; /* Always N_COLORS */

  guint size; /* How many colors we allow */
};

static GType
gtk_color_list_get_item_type (GListModel *list)
{
  return GTK_TYPE_COLOR;
}

static guint
gtk_color_list_get_n_items (GListModel *list)
{
  GtkColorList *self = GTK_COLOR_LIST (list);

  return self->size;
}

static guint
position_to_color (guint position)
{
  static guint map[] = {
    0xFF0000, 0x00FF00, 0x0000FF,
    0x7F0000, 0x007F00, 0x00007F,
    0x3F0000, 0x003F00, 0x00003F,
    0x1F0000, 0x001F00, 0x00001F,
    0x0F0000, 0x000F00, 0x00000F,
    0x070000, 0x000700, 0x000007,
    0x030000, 0x000300, 0x000003,
    0x010000, 0x000100, 0x000001
  };
  guint result, i;

  result = 0;

  for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
      if (position & (1 << i))
        result ^= map[i];
    }

  return result;
}

static gpointer
gtk_color_list_get_item (GListModel *list,
                         guint       position)
{
  GtkColorList *self = GTK_COLOR_LIST (list);

  if (position >= self->size)
    return NULL;

  position = position_to_color (position);

  if (self->colors[position] == NULL)
    {
      guint red, green, blue;

      red = (position >> 16) & 0xFF;
      green = (position >> 8) & 0xFF;
      blue = position & 0xFF;

      self->colors[position] = gtk_color_new ("", red / 255., green / 255., blue / 255.);
    }

  return g_object_ref (self->colors[position]);
}

static void
gtk_color_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_color_list_get_item_type;
  iface->get_n_items = gtk_color_list_get_n_items;
  iface->get_item = gtk_color_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkColorList, gtk_color_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                gtk_color_list_model_init))

static GParamSpec *list_properties[N_LIST_PROPS] = { NULL, };

static void
gtk_color_list_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkColorList *self = GTK_COLOR_LIST (object);

  switch (property_id)
    {
    case LIST_PROP_SIZE:
      g_value_set_uint (value, self->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_list_set_size (GtkColorList *self,
                         guint         size)
{
  guint old_size = self->size;

  self->size = size;
  if (self->size > old_size)
    g_list_model_items_changed (G_LIST_MODEL (self), old_size, 0, self->size - old_size);
  else if (old_size > self->size)
    g_list_model_items_changed (G_LIST_MODEL (self), self->size, old_size - self->size, 0);

  g_object_notify_by_pspec (G_OBJECT (self), list_properties[LIST_PROP_SIZE]);
}

static void
gtk_color_list_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkColorList *self = GTK_COLOR_LIST (object);

  switch (property_id)
    {
    case LIST_PROP_SIZE:
      gtk_color_list_set_size (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_color_list_dispose (GObject *object)
{
  GtkColorList *self = GTK_COLOR_LIST (object);
  guint i;

  for (i = 0; i < N_COLORS; i++)
    {
      g_clear_object (&self->colors[i]);
    }
  g_free (self->colors);

  G_OBJECT_CLASS (gtk_color_parent_class)->finalize (object);
}

static void
gtk_color_list_class_init (GtkColorListClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_color_list_get_property;
  gobject_class->set_property = gtk_color_list_set_property;
  gobject_class->dispose = gtk_color_list_dispose;

  list_properties[LIST_PROP_SIZE] =
    g_param_spec_uint ("size", NULL, NULL, 0, N_COLORS, 0, G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_LIST_PROPS, list_properties);
}

static void
gtk_color_list_init (GtkColorList *self)
{
  GBytes *data;
  char **lines;
  guint i;

  self->colors = g_new0 (GtkColor *, N_COLORS);

  data = g_resources_lookup_data ("/listview_colors/color.names.txt", 0, NULL);
  lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

  for (i = 0; lines[i]; i++)
    {
      const char *name;
      char **fields;
      int red, green, blue;
      guint pos;

      if (lines[i][0] == '#' || lines[i][0] == '\0')
        continue;

      fields = g_strsplit (lines[i], " ", 0);
      name = fields[1];
      red = atoi (fields[3]);
      green = atoi (fields[4]);
      blue = atoi (fields[5]);

      pos = ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | blue;
      if (self->colors[pos] == NULL)
        self->colors[pos] = gtk_color_new (name, red / 255., green / 255., blue / 255.);

      g_strfreev (fields);
    }
  g_strfreev (lines);

  g_bytes_unref (data);
}

GListModel *
gtk_color_list_new (guint size)
{
  return g_object_new (GTK_TYPE_COLOR_LIST,
                       "size", size,
                       NULL);
}

static char *
get_rgb_markup (gpointer this,
                GtkColor *color)
{
  if (!color)
    return NULL;

  return g_strdup_printf ("<b>R:</b> %d <b>G:</b> %d <b>B:</b> %d",
                          (int)(color->color.red * 255), 
                          (int)(color->color.green * 255),
                          (int)(color->color.blue * 255));
}

static char *
get_hsv_markup (gpointer this,
                GtkColor *color)
{
  if (!color)
    return NULL;

  return g_strdup_printf ("<b>H:</b> %d <b>S:</b> %d <b>V:</b> %d",
                          color->h, 
                          color->s,
                          color->v);
}

static void
setup_simple_listitem_cb (GtkListItemFactory *factory,
                          GtkListItem        *list_item)
{
  GtkWidget *picture;
  GtkExpression *color_expression, *expression;

  expression = gtk_constant_expression_new (GTK_TYPE_LIST_ITEM, list_item);
  color_expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, expression, "item");

  picture = gtk_picture_new ();
  gtk_widget_set_size_request (picture, 32, 32);
  gtk_expression_bind (color_expression, picture, "paintable", NULL);

  gtk_list_item_set_child (list_item, picture);
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *box, *picture, *name_label, *rgb_label, *hsv_label;
  GtkExpression *color_expression, *expression;
  GtkExpression *params[1];

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_list_item_set_child (list_item, box);

  expression = gtk_constant_expression_new (GTK_TYPE_LIST_ITEM, list_item);
  color_expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, expression, "item");

  expression = gtk_property_expression_new (GTK_TYPE_COLOR,
                                            gtk_expression_ref (color_expression),
                                            "name");
  name_label = gtk_label_new (NULL);
  gtk_expression_bind (expression, name_label, "label", NULL);
  gtk_box_append (GTK_BOX (box), name_label);

  expression = gtk_expression_ref (color_expression);
  picture = gtk_picture_new ();
  gtk_expression_bind (expression, picture, "paintable", NULL);
  gtk_box_append (GTK_BOX (box), picture);

  params[0] = gtk_expression_ref (color_expression);
  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL,
                                            1, params,
                                            (GCallback)get_rgb_markup,
                                            NULL, NULL);

  rgb_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (rgb_label), TRUE);
  gtk_expression_bind (expression, rgb_label, "label", NULL);
  gtk_box_append (GTK_BOX (box), rgb_label);

  params[0] = gtk_expression_ref (color_expression);
  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL,
                                            1, params,
                                            (GCallback)get_hsv_markup,
                                            NULL, NULL);

  hsv_label = gtk_label_new (NULL);
  gtk_label_set_use_markup (GTK_LABEL (hsv_label), TRUE);
  gtk_expression_bind (expression, hsv_label, "label", NULL);
  gtk_box_append (GTK_BOX (box), hsv_label);

  gtk_expression_unref (color_expression);
}

static void
setup_selection_listitem_cb (GtkListItemFactory *factory,
                             GtkListItem        *list_item)
{
  GtkWidget *picture;
  GtkExpression *color_expression, *expression;

  expression = gtk_constant_expression_new (GTK_TYPE_LIST_ITEM, list_item);
  color_expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, expression, "item");

  picture = gtk_picture_new ();
  gtk_widget_set_size_request (picture, 8, 8);
  gtk_expression_bind (color_expression, picture, "paintable", NULL);

  gtk_list_item_set_child (list_item, picture);
}

static void
set_title (gpointer    item,
           const char *title)
{
  g_object_set_data (G_OBJECT (item), "title", (gpointer)title);
}

static char *
get_title (gpointer item)
{
  return g_strdup ((char *)g_object_get_data (G_OBJECT (item), "title"));
}

GtkWidget *
create_color_grid (void)
{
  GtkWidget *gridview;
  GtkListItemFactory *factory;

  gridview = gtk_grid_view_new (NULL, NULL);
  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (gridview), GTK_SCROLL_NATURAL);
  gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (gridview), GTK_SCROLL_NATURAL);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_simple_listitem_cb), NULL);
  gtk_grid_view_set_factory (GTK_GRID_VIEW (gridview), factory);
  g_object_unref (factory);

  gtk_grid_view_set_max_columns (GTK_GRID_VIEW (gridview), 24);
  gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (gridview), TRUE);

  return gridview;
}

static gboolean
add_colors (GtkWidget     *widget,
            GdkFrameClock *clock,
            gpointer       data)
{
  GtkColorList *colors = data;
  guint limit;

  limit = GPOINTER_TO_UINT (g_object_get_data (data, "limit"));
  gtk_color_list_set_size (colors, MIN (limit, colors->size + MAX (1, limit / 4096)));
  
  if (colors->size >= limit)
    return G_SOURCE_REMOVE;
  else
    return G_SOURCE_CONTINUE;
}

static void
refill (GtkWidget    *button,
        GtkColorList *colors)
{
  gtk_color_list_set_size (colors, 0);
  gtk_widget_add_tick_callback (button, add_colors, g_object_ref (colors), g_object_unref);
}
 
static void
limit_changed_cb (GtkDropDown  *dropdown,
                  GParamSpec   *pspec,
                  GtkColorList *colors)
{
  guint new_limit, old_limit;

  old_limit = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (colors), "limit"));
  new_limit = 1 << (3 * (gtk_drop_down_get_selected (dropdown) + 1));

  g_object_set_data (G_OBJECT (colors), "limit", GUINT_TO_POINTER (new_limit));

  if (old_limit == colors->size)
    gtk_color_list_set_size (colors, new_limit);
}

static void
limit_changed_cb2 (GtkDropDown  *dropdown,
                   GParamSpec   *pspec,
                   GtkLabel     *label)
{
  char *string;
  int len;
  guint limit;

  limit = 1 << (3 * (gtk_drop_down_get_selected (dropdown) + 1));

  string = g_strdup_printf ("%'u", limit);
  len = g_utf8_strlen (string, -1);
  g_free (string);

  gtk_label_set_width_chars (label, len + 2); /* for " /" */
}

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  GtkWidget  *label)
{
  guint n = g_list_model_get_n_items (model);
  char *text;

  text = g_strdup_printf ("%'u /", n);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
}

static void
setup_number_item (GtkSignalListItemFactory *factory,
                   GtkListItem              *item)
{
  GtkWidget *label;
  PangoAttrList *attrs;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 1);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_font_features_new ("tnum"));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  gtk_list_item_set_child (item, label);
}

static void
bind_number_item (GtkSignalListItemFactory *factory,
                  GtkListItem              *item)
{
  GtkWidget *label;
  guint limit;
  char *string;

  label = gtk_list_item_get_child (item);

  limit = 1 << (3 * (gtk_list_item_get_position (item) + 1));
  string = g_strdup_printf ("%'u", limit);
  gtk_label_set_label (GTK_LABEL (label), string);
  g_free (string);
}

static void
update_selection_count (GListModel *model,
                        guint       position,
                        guint       removed,
                        guint       added,
                        gpointer    data)
{
  char *text;
  text = g_strdup_printf ("%u", g_list_model_get_n_items (model));
  gtk_label_set_label (GTK_LABEL (data), text);
  g_free (text);
}

static void
update_selection_average (GListModel *model,
                          guint       position,
                          guint       removed,
                          guint       added,
                          gpointer    data)
{
  guint n = g_list_model_get_n_items (model);
  GdkRGBA c = { 0, 0, 0, 1 };
  guint i;
  GtkColor *color;

  for (i = 0; i < n; i++)
    {
      color = g_list_model_get_item (model, i);

      c.red += color->color.red;
      c.green += color->color.green;
      c.blue += color->color.blue;

      g_object_unref (color);
    }

  color = gtk_color_new ("", c.red / n, c.green / n, c.blue / n);
  gtk_picture_set_paintable (GTK_PICTURE (data), GDK_PAINTABLE (color));
  g_object_unref (color);
}

static void
update_progress_cb (GtkSortListModel *model,
                    GParamSpec       *pspec,
                    GtkProgressBar   *progress)
{
  guint total;
  guint pending;

  total = g_list_model_get_n_items (G_LIST_MODEL (model));
  total = MAX (total, 1); /* avoid div by 0 below */
  pending = gtk_sort_list_model_get_pending (model);

  gtk_widget_set_visible (GTK_WIDGET (progress), pending != 0);
  gtk_progress_bar_set_fraction (progress, (total - pending) / (double) total);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_colors (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkMultiSelection *selection;
      GtkSortListModel *sort_model;
      GtkWidget *header, *overlay, *gridview, *sw, *box, *dropdown;
      GtkListItemFactory *factory;
      GListStore *factories;
      GtkSorter *sorter;
      GtkSorter *multi_sorter;
      GListStore *sorters;
      GtkExpression *expression;
      GtkWidget *button;
      GtkWidget *label;
      PangoAttrList *attrs;
      char *string;
      guint len;
      GtkWidget *selection_view;
      GListModel *selection_filter;
      GtkSelectionModel *no_selection;
      GtkWidget *grid;
      GtkWidget *selection_size_label;
      GtkWidget *selection_average_picture;
      GtkWidget *selection_info_toggle;
      GtkWidget *selection_info_revealer;
      GtkWidget *progress;
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/listview_colors/listview_colors.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      sort_model = gtk_sort_list_model_new (gtk_color_list_new (0), NULL);
      gtk_sort_list_model_set_incremental (sort_model, TRUE);
      selection = gtk_multi_selection_new (G_LIST_MODEL (sort_model));

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Colors");
      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer*)&window);

      overlay = gtk_overlay_new ();
      gtk_window_set_child (GTK_WINDOW (window), overlay);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_overlay_set_child (GTK_OVERLAY (overlay), box);

      progress = gtk_progress_bar_new ();
      gtk_widget_set_hexpand (progress, TRUE);
      gtk_widget_set_valign (progress, GTK_ALIGN_START);
      g_signal_connect (sort_model, "notify::pending", G_CALLBACK (update_progress_cb), progress);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress);

      selection_info_revealer = gtk_revealer_new ();
      gtk_box_append (GTK_BOX (box), selection_info_revealer);

      grid = gtk_grid_new ();
      gtk_revealer_set_child (GTK_REVEALER (selection_info_revealer), grid);
      gtk_widget_set_margin_start (grid, 10);
      gtk_widget_set_margin_end (grid, 10);
      gtk_widget_set_margin_top (grid, 10);
      gtk_widget_set_margin_bottom (grid, 10);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

      label = gtk_label_new ("Selection");
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_add_css_class (label, "title-3");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 5, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Size:"), 0, 2, 1, 1);

      selection_size_label = gtk_label_new ("0");
      gtk_grid_attach (GTK_GRID (grid), selection_size_label, 1, 2, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Average:"), 2, 2, 1, 1);

      selection_average_picture = gtk_picture_new ();
      gtk_widget_set_size_request (selection_average_picture, 32, 32);
      gtk_grid_attach (GTK_GRID (grid), selection_average_picture, 3, 2, 1, 1);

      label = gtk_label_new ("");
      gtk_widget_set_hexpand (label, TRUE);
      gtk_grid_attach (GTK_GRID (grid), label, 4, 2, 1, 1);

      sw = gtk_scrolled_window_new ();
      gtk_widget_set_hexpand (sw, TRUE);

      gtk_grid_attach (GTK_GRID (grid), sw, 0, 1, 5, 1);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_selection_listitem_cb), NULL);
      selection_view = gtk_grid_view_new (NULL, factory);
      gtk_widget_add_css_class (selection_view, "compact");
      gtk_grid_view_set_max_columns (GTK_GRID_VIEW (selection_view), 200);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), selection_view);

      sw = gtk_scrolled_window_new ();
      gtk_box_append (GTK_BOX (box), sw);

      gridview = create_color_grid ();
      gtk_grid_view_set_model (GTK_GRID_VIEW (gridview), GTK_SELECTION_MODEL (selection));
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), gridview);
      gtk_widget_set_hexpand (sw, TRUE);
      gtk_widget_set_vexpand (sw, TRUE);

      selection_filter = G_LIST_MODEL (gtk_selection_filter_model_new (GTK_SELECTION_MODEL (selection)));
      g_signal_connect (selection_filter, "items-changed", G_CALLBACK (update_selection_count), selection_size_label);
      g_signal_connect (selection_filter, "items-changed", G_CALLBACK (update_selection_average), selection_average_picture);

      no_selection = GTK_SELECTION_MODEL (gtk_no_selection_new (selection_filter));
      gtk_grid_view_set_model (GTK_GRID_VIEW (selection_view), no_selection);
      g_object_unref (no_selection);

      selection_info_toggle = gtk_toggle_button_new ();
      gtk_button_set_icon_name (GTK_BUTTON (selection_info_toggle), "emblem-important-symbolic");
      gtk_widget_set_tooltip_text (selection_info_toggle, "Show selection info");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), selection_info_toggle);

      g_object_bind_property (selection_info_toggle, "active",
                              selection_info_revealer, "reveal-child",
                              G_BINDING_DEFAULT);

      button = gtk_button_new_with_mnemonic ("_Refill");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (refill),
                        gtk_sort_list_model_get_model (sort_model));

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      label = gtk_label_new ("0 /");
      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_font_features_new ("tnum"));
      gtk_label_set_attributes (GTK_LABEL (label), attrs);
      pango_attr_list_unref (attrs);
      string = g_strdup_printf ("%'u", 4096);
      len = g_utf8_strlen (string, -1);
      g_free (string);
      gtk_label_set_width_chars (GTK_LABEL (label), len + 2);
      gtk_label_set_xalign (GTK_LABEL (label), 1);

      g_signal_connect (selection, "items-changed", G_CALLBACK (items_changed_cb), label);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), label);

      dropdown = gtk_drop_down_new_from_strings  ((const char * const[]) { "8", "64", "512", "4096", "32768", "262144", "2097152", "16777216", NULL });
      g_signal_connect (dropdown, "notify::selected",
                        G_CALLBACK (limit_changed_cb), 
                        gtk_sort_list_model_get_model (sort_model));
      g_signal_connect (dropdown, "notify::selected",
                        G_CALLBACK (limit_changed_cb2), 
                        label);
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_number_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_number_item), NULL);
      gtk_drop_down_set_factory (GTK_DROP_DOWN (dropdown), factory);
      g_object_unref (factory);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), 3); /* 4096 */
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), dropdown);

      sorters = g_list_store_new (GTK_TYPE_SORTER);

      /* An empty multisorter doesn't do any sorting and the sortmodel is
       * smart enough to know that.
       */
      sorter = GTK_SORTER (gtk_multi_sorter_new ());
      set_title (sorter, "Unsorted");
      g_list_store_append (sorters, sorter);
      g_object_unref (sorter);

      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "name")));
      set_title (sorter, "Name");
      g_list_store_append (sorters, sorter);
      g_object_unref (sorter);

      multi_sorter = GTK_SORTER (gtk_multi_sorter_new ());

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "red")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Red");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "green")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Green");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "blue")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Blue");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      set_title (multi_sorter, "RGB");
      g_list_store_append (sorters, multi_sorter);
      g_object_unref (multi_sorter);

      multi_sorter = GTK_SORTER (gtk_multi_sorter_new ());

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "hue")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Hue");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "saturation")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Saturation");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_COLOR, NULL, "value")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      set_title (sorter, "Value");
      g_list_store_append (sorters, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi_sorter), sorter);

      set_title (multi_sorter, "HSV");
      g_list_store_append (sorters, multi_sorter);
      g_object_unref (multi_sorter);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                                NULL,
                                                0, NULL,
                                                (GCallback)get_title,
                                                NULL, NULL);

      dropdown = gtk_drop_down_new (G_LIST_MODEL (sorters), expression);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_append (GTK_BOX (box), gtk_label_new ("Sort by:"));
      gtk_box_append (GTK_BOX (box), dropdown);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), box);

      g_object_bind_property (dropdown, "selected-item", sort_model, "sorter", G_BINDING_SYNC_CREATE);

      factories = g_list_store_new (GTK_TYPE_LIST_ITEM_FACTORY);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_simple_listitem_cb), NULL);
      set_title (factory, "Colors");
      g_list_store_append (factories, factory);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
      set_title (factory, "Everything");
      g_list_store_append (factories, factory);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                                NULL,
                                                0, NULL,
                                                (GCallback)get_title,
                                                NULL, NULL);
      dropdown = gtk_drop_down_new (G_LIST_MODEL (factories), expression);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_append (GTK_BOX (box), gtk_label_new ("Show:"));
      gtk_box_append (GTK_BOX (box), dropdown);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), box);

      g_object_bind_property (dropdown, "selected-item", gridview, "factory", G_BINDING_SYNC_CREATE);

      g_object_unref (selection);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
