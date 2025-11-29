/* Lists/Weather
 *
 * This demo shows a few of the rarer features of GtkListView and
 * how they can be used to display weather information.
 *
 * The hourly weather info uses a horizontal listview. This is easy
 * to achieve because GtkListView implements the GtkOrientable interface.
 * To make the items in the list stand out more, the listview uses
 * separators.
 *
 * A GtkNoSelectionModel is used to make sure no item in the list can be
 * selected. All other interactions with the items is still possible.
 *
 * The dataset used here has 70 000 items.
 */

#include <gtk/gtk.h>

#define GTK_TYPE_WEATHER_INFO (gtk_weather_info_get_type ())

G_DECLARE_FINAL_TYPE (GtkWeatherInfo, gtk_weather_info, GTK, WEATHER_INFO, GObject)

typedef enum {
  GTK_WEATHER_CLEAR,
  GTK_WEATHER_FEW_CLOUDS,
  GTK_WEATHER_FOG,
  GTK_WEATHER_OVERCAST,
  GTK_WEATHER_SCATTERED_SHOWERS,
  GTK_WEATHER_SHOWERS,
  GTK_WEATHER_SNOW,
  GTK_WEATHER_STORM
} GtkWeatherType;

struct _GtkWeatherInfo
{
  GObject parent_instance;

  gint64 timestamp;
  int temperature;
  GtkWeatherType weather_type;
};

struct _GtkWeatherInfoClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkWeatherInfo, gtk_weather_info, G_TYPE_OBJECT)

static void
gtk_weather_info_class_init (GtkWeatherInfoClass *klass)
{
}

static void
gtk_weather_info_init (GtkWeatherInfo *self)
{
}

static GtkWeatherInfo *
gtk_weather_info_new (GDateTime      *timestamp,
                      GtkWeatherInfo *copy_from)
{
  GtkWeatherInfo *result;

  result = g_object_new (GTK_TYPE_WEATHER_INFO, NULL);

  result->timestamp = g_date_time_to_unix (timestamp);
  if (copy_from)
    {
      result->temperature = copy_from->temperature;
      result->weather_type = copy_from->weather_type;
    }

  return result;
}

static GDateTime *
parse_timestamp (const char *string,
                 GTimeZone  *_tz)
{
  char *with_seconds;
  GDateTime *result;

  with_seconds = g_strconcat (string, ":00", NULL);
  result = g_date_time_new_from_iso8601 (with_seconds, _tz);
  g_free (with_seconds);

  return result;
}

static GtkWeatherType
parse_weather_type (const char     *clouds,
                    const char     *precip,
                    GtkWeatherType  fallback)
{
  if (strstr (precip, "SN"))
    return GTK_WEATHER_SNOW;

  if (strstr (precip, "TS"))
    return GTK_WEATHER_STORM;

  if (strstr (precip, "DZ"))
    return GTK_WEATHER_SCATTERED_SHOWERS;

  if (strstr (precip, "SH") || strstr (precip, "RA"))
    return GTK_WEATHER_SHOWERS;

  if (strstr (precip, "FG"))
    return GTK_WEATHER_FOG;

  if (g_str_equal (clouds, "M") ||
      g_str_equal (clouds, ""))
    return fallback;

  if (strstr (clouds, "OVC") ||
      strstr (clouds, "BKN"))
    return GTK_WEATHER_OVERCAST;

  if (strstr (clouds, "BKN") ||
      strstr (clouds, "SCT"))
    return GTK_WEATHER_FEW_CLOUDS;

  if (strstr (clouds, "VV"))
    return GTK_WEATHER_FOG;

  return GTK_WEATHER_CLEAR;
}

static double
parse_temperature (const char *s,
                   double      fallback)
{
  char *endptr;
  double d;

  d = g_ascii_strtod (s, &endptr);
  if (*endptr != '\0')
    return fallback;

  return d;
}

static GListModel *
create_weather_model (void)
{
  GListStore *store;
  GTimeZone *utc;
  GDateTime *timestamp;
  GtkWeatherInfo *info;
  GBytes *data;
  char **lines;
  guint i;

  store = g_list_store_new (GTK_TYPE_WEATHER_INFO);
  data = g_resources_lookup_data ("/listview_weather/listview_weather.txt", 0, NULL);
  lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

  utc = g_time_zone_new_utc ();
  timestamp = g_date_time_new (utc, 2011, 1, 1, 0, 0, 0);
  info = gtk_weather_info_new (timestamp, NULL);
  g_list_store_append (store, info);
  g_object_unref (info);

  for (i = 0; lines[i] != NULL && *lines[i]; i++)
    {
      char **fields;
      GDateTime *date;

      fields = g_strsplit (lines[i], ",", 0);
      date = parse_timestamp (fields[0], utc);
      while (g_date_time_difference (date, timestamp) > 30 * G_TIME_SPAN_MINUTE)
        {
          GDateTime *new_timestamp = g_date_time_add_hours (timestamp, 1);
          g_date_time_unref (timestamp);
          timestamp = new_timestamp;
          info = gtk_weather_info_new (timestamp, info);
          g_list_store_append (store, info);
          g_object_unref (info);
        }

      info->temperature = parse_temperature (fields[1], info->temperature);
      info->weather_type = parse_weather_type (fields[2], fields[3], info->weather_type);
      g_date_time_unref (date);
      g_strfreev (fields);
    }

  g_date_time_unref (timestamp);
  g_strfreev (lines);
  g_bytes_unref (data);
  g_time_zone_unref (utc);

  return G_LIST_MODEL (store);
}

static void
setup_widget (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *box, *child;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_list_item_set_child (list_item, box);

  child = gtk_label_new (NULL);
  gtk_label_set_width_chars (GTK_LABEL (child), 5);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_image_new ();
  gtk_image_set_icon_size (GTK_IMAGE (child), GTK_ICON_SIZE_LARGE);
  gtk_box_append (GTK_BOX (box), child);

  child = gtk_label_new (NULL);
  gtk_widget_set_vexpand (child, TRUE);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_label_set_width_chars (GTK_LABEL (child), 4);
  gtk_box_append (GTK_BOX (box), child);
}

static void
bind_widget (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *box, *child;
  GtkWeatherInfo *info;
  GDateTime *timestamp;
  char *s;

  box = gtk_list_item_get_child (list_item);
  info = gtk_list_item_get_item (list_item);

  child = gtk_widget_get_first_child (box);
  timestamp = g_date_time_new_from_unix_utc (info->timestamp);
  s = g_date_time_format (timestamp, "%R");
  gtk_label_set_text (GTK_LABEL (child), s);
  g_free (s);
  g_date_time_unref (timestamp);

  child = gtk_widget_get_next_sibling (child);
  switch (info->weather_type)
  {
  case GTK_WEATHER_CLEAR:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-clear-symbolic");
    break;
  case GTK_WEATHER_FEW_CLOUDS:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-few-clouds-symbolic");
    break;
  case GTK_WEATHER_FOG:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-fog-symbolic");
    break;
  case GTK_WEATHER_OVERCAST:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-overcast-symbolic");
    break;
  case GTK_WEATHER_SCATTERED_SHOWERS:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-showers-scattered-symbolic");
    break;
  case GTK_WEATHER_SHOWERS:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-showers-symbolic");
    break;
  case GTK_WEATHER_SNOW:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-snow-symbolic");
    break;
  case GTK_WEATHER_STORM:
    gtk_image_set_from_icon_name (GTK_IMAGE (child), "weather-storm-symbolic");
    break;
  default:
    gtk_image_clear (GTK_IMAGE (child));
    break;
  }


  child = gtk_widget_get_next_sibling (child);
  s = g_strdup_printf ("%d°", info->temperature);
  gtk_label_set_text (GTK_LABEL (child), s);
  g_free (s);
}

static GtkWidget *window = NULL;

GtkWidget *
create_weather_view (void)
{
  GtkWidget *listview;
  GtkSelectionModel *model;
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_widget), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_widget), NULL);
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (create_weather_model ()));
  listview = gtk_list_view_new (model, factory);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (listview), GTK_ORIENTATION_HORIZONTAL);
  gtk_list_view_set_show_separators (GTK_LIST_VIEW (listview), TRUE);

  return listview;
}

GtkWidget *
do_listview_weather (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *sw;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_title (GTK_WINDOW (window), "Weather");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Weather");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);
      listview = create_weather_view ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
