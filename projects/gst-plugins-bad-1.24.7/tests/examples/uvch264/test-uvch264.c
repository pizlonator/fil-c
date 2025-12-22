#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gst/video/video.h>

#define WINDOW_GLADE "window.glade"
#define INT_PROPERTY_GLADE "int_property.glade"
#define ENUM_PROPERTY_GLADE "enum_property.glade"
#define BOOL_PROPERTY_GLADE "boolean_property.glade"

#define PROPERTY_TO_VBOX                                                \
  properties[i].dynamic ? GTK_BOX (dynamic_vbox) : GTK_BOX (static_vbox)

#define GET_WIDGET(object, type, name)                          \
  type (gtk_builder_get_object ((object)->builder, name))

#define GET_PROP_WIDGET(type, name) GET_WIDGET (&(properties[i]), type, name)

static guint h264_xid, preview_xid;

typedef struct
{
  GtkBuilder *builder;
  GstElement *src;
  enum
  { NONE, INT, ENUM, BOOL } type;
  const gchar *property_name;
  gboolean readonly;
  gboolean dynamic;
} Prop;

typedef struct
{
  GtkBuilder *builder;
  GstElement *bin;
  GstElement *src;
  GstElement *identity;
  GstElement *vid_capsfilter;
  GstElement *vf_capsfilter;
} Main;

Prop properties[] = {
  {NULL, NULL, INT, "initial-bitrate", FALSE, FALSE},
  {NULL, NULL, INT, "slice-units", FALSE, FALSE},
  {NULL, NULL, ENUM, "slice-mode", FALSE, FALSE},
  {NULL, NULL, INT, "iframe-period", FALSE, FALSE},
  {NULL, NULL, ENUM, "usage-type", FALSE, FALSE},
  {NULL, NULL, ENUM, "entropy", FALSE, FALSE},
  {NULL, NULL, BOOL, "enable-sei", FALSE, FALSE},
  {NULL, NULL, INT, "num-reorder-frames", FALSE, FALSE},
  {NULL, NULL, BOOL, "preview-flipped", FALSE, FALSE},
  {NULL, NULL, INT, "leaky-bucket-size", FALSE, FALSE},
  {NULL, NULL, INT, "num-clock-samples", FALSE, TRUE},
  {NULL, NULL, ENUM, "rate-control", FALSE, TRUE},
  {NULL, NULL, BOOL, "fixed-framerate", FALSE, TRUE},
  {NULL, NULL, INT, "max-mbps", TRUE, TRUE},
  {NULL, NULL, INT, "level-idc", FALSE, TRUE},
  {NULL, NULL, INT, "peak-bitrate", FALSE, TRUE},
  {NULL, NULL, INT, "average-bitrate", FALSE, TRUE},
  {NULL, NULL, INT, "min-iframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "max-iframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "min-pframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "max-pframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "min-bframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "max-bframe-qp", FALSE, TRUE},
  {NULL, NULL, INT, "ltr-buffer-size", FALSE, TRUE},
  {NULL, NULL, INT, "ltr-encoder-control", FALSE, TRUE},
};

static void set_drop_probability (Main * self);
static void get_all_properties (void);
static void probe_all_properties (gboolean playing);

/* Callbacks */
void on_button_toggled (GtkToggleButton * button, gpointer user_data);
void on_get_button_clicked (GtkButton * button, gpointer user_data);
void on_set_button_clicked (GtkButton * button, gpointer user_data);
void on_button_ready_clicked (GtkButton * button, gpointer user_data);
void on_button_null_clicked (GtkButton * button, gpointer user_data);
void on_button_playing_clicked (GtkButton * button, gpointer user_data);
void on_iframe_button_clicked (GtkButton * button, gpointer user_data);
void on_renegotiate_button_clicked (GtkButton * button, gpointer user_data);
void on_start_capture_button_clicked (GtkButton * button, gpointer user_data);
void on_stop_capture_button_clicked (GtkButton * button, gpointer user_data);
void on_window_destroyed (GtkWindow * window, gpointer user_data);

static GstEvent *
new_upstream_force_key_unit (GstClockTime running_time,
    gboolean all_headers, guint count)
{
  GstEvent *force_key_unit_event;
  GstStructure *s;

  s = gst_structure_new ("GstForceKeyUnit",
      "running-time", GST_TYPE_CLOCK_TIME, running_time,
      "all-headers", G_TYPE_BOOLEAN, all_headers,
      "count", G_TYPE_UINT, count, NULL);
  force_key_unit_event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);

  return force_key_unit_event;
}

void
on_get_button_clicked (GtkButton * button, gpointer user_data)
{
  Prop *property = user_data;

  switch (property->type) {
    case INT:
    {
      gchar *val;
      gint val_int;
      g_object_get (property->src, property->property_name, &val_int, NULL);
      val = g_strdup_printf ("%d", val_int);
      gtk_entry_set_text (GET_WIDGET (property, GTK_ENTRY, "value"), val);
      g_free (val);
    }
      break;
    case ENUM:
    {
      GParamSpec *param;
      gint val;

      g_object_get (property->src, property->property_name, &val, NULL);
      param = g_object_class_find_property (G_OBJECT_GET_CLASS (property->src),
          property->property_name);
      if (G_IS_PARAM_SPEC_ENUM (param)) {
        GEnumValue *values;
        guint i = 0;

        values = G_ENUM_CLASS (g_type_class_ref (param->value_type))->values;

        while (values[i].value_name) {
          if (values[i].value == val) {
            gtk_combo_box_set_active (GET_WIDGET (property,
                    (GtkComboBox *), "value"), i);
            break;
          }
          i++;
        }
      }
    }
      break;
    case BOOL:
    {
      gboolean val;

      g_object_get (property->src, property->property_name, &val, NULL);
      gtk_toggle_button_set_active (GET_WIDGET (property,
              (GtkToggleButton *), "value"), val);
    }
      break;
    case NONE:
    default:
      break;
  }
}

void
on_set_button_clicked (GtkButton * button, gpointer user_data)
{
  Prop *property = user_data;

  switch (property->type) {
    case INT:
    {
      int val_int;
      const gchar *val;

      val = gtk_entry_get_text (GET_WIDGET (property, GTK_ENTRY, "value"));
      val_int = (int) g_ascii_strtoll (val, NULL, 0);
      g_object_set (property->src, property->property_name, val_int, NULL);
    }
      break;
    case ENUM:
    {
      GParamSpec *param;

      param = g_object_class_find_property (G_OBJECT_GET_CLASS (property->src),
          property->property_name);
      if (G_IS_PARAM_SPEC_ENUM (param)) {
        GEnumValue *values;
        guint val = 0;

        values = G_ENUM_CLASS (g_type_class_ref (param->value_type))->values;

        val = gtk_combo_box_get_active (GET_WIDGET (property,
                (GtkComboBox *), "value"));
        g_object_set (property->src, property->property_name,
            values[val].value, NULL);
      }
    }
      break;
    case BOOL:
    {
      gboolean val;

      val = gtk_toggle_button_get_active (GET_WIDGET (property,
              (GtkToggleButton *), "value"));
      g_object_set (property->src, property->property_name, val, NULL);
    }
      break;
    case NONE:
    default:
      break;
  }
  get_all_properties ();
}

void
on_button_toggled (GtkToggleButton * button, gpointer user_data)
{
  if (gtk_toggle_button_get_active (button))
    gtk_button_set_label (GTK_BUTTON (button), "   Enabled   ");
  else
    gtk_button_set_label (GTK_BUTTON (button), "  Disabled   ");
}

static gboolean
set_caps (Main * self, gboolean send_event)
{
  const gchar *h264_filter;
  const gchar *raw_filter;
  GstCaps *h264_caps = NULL;
  GstCaps *raw_caps = NULL;
  gboolean ret = TRUE;

  h264_filter = gtk_entry_get_text (GET_WIDGET (self, GTK_ENTRY, "h264_caps"));
  raw_filter =
      gtk_entry_get_text (GET_WIDGET (self, GTK_ENTRY, "preview_caps"));
  if (h264_filter)
    h264_caps = gst_caps_from_string (h264_filter);
  if (raw_filter)
    raw_caps = gst_caps_from_string (raw_filter);

  g_debug ("H264 caps : %s", gst_caps_to_string (h264_caps));
  g_debug ("Preview caps : %s", gst_caps_to_string (raw_caps));
  if (!h264_caps || !raw_caps) {
    g_debug ("Invalid caps");
    ret = FALSE;
    goto end;
  }

  g_object_set (self->vid_capsfilter, "caps", h264_caps, NULL);
  g_object_set (self->vf_capsfilter, "caps", raw_caps, NULL);

  if (send_event) {
    gst_element_send_event (GST_ELEMENT (self->src),
        gst_event_new_reconfigure ());
  }

end:
  if (h264_caps)
    gst_caps_unref (h264_caps);
  if (raw_caps)
    gst_caps_unref (raw_caps);

  return ret;
}

void
on_button_ready_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  set_caps (self, FALSE);
  gst_element_set_state (self->bin, GST_STATE_READY);
  probe_all_properties (FALSE);
  get_all_properties ();
}

void
on_button_null_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  gst_element_set_state (self->bin, GST_STATE_NULL);
  probe_all_properties (FALSE);
  get_all_properties ();
}

void
on_button_playing_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  if (gst_element_set_state (self->bin, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    g_debug ("Unable to go to state PLAYING");
  }
  set_caps (self, FALSE);
  probe_all_properties (TRUE);
  get_all_properties ();

  set_drop_probability (self);
}

void
on_iframe_button_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;
  GstEvent *event;
  gboolean pps_sps;

  set_drop_probability (self);
  pps_sps = gtk_toggle_button_get_active (GET_WIDGET (self, (GtkToggleButton *),
          "pps_sps"));

  event = new_upstream_force_key_unit (GST_CLOCK_TIME_NONE, pps_sps, 0);
  gst_element_send_event (GST_ELEMENT (self->src), event);
}

void
on_renegotiate_button_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  set_caps (self, TRUE);
  probe_all_properties (GST_STATE (self->bin) >= GST_STATE_PAUSED);
  get_all_properties ();
}

void
on_start_capture_button_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  set_caps (self, FALSE);
  g_signal_emit_by_name (G_OBJECT (self->src), "start-capture", NULL);
  probe_all_properties (GST_STATE (self->bin) >= GST_STATE_PAUSED);
  get_all_properties ();
}

void
on_stop_capture_button_clicked (GtkButton * button, gpointer user_data)
{
  Main *self = user_data;

  set_caps (self, FALSE);
  g_signal_emit_by_name (G_OBJECT (self->src), "stop-capture", NULL);
  probe_all_properties (GST_STATE (self->bin) >= GST_STATE_PAUSED);
  get_all_properties ();
}

void
on_window_destroyed (GtkWindow * window, gpointer user_data)
{
  gtk_main_quit ();
}

static gboolean
_bus_callback (GstBus * bus, GstMessage * message, gpointer user_data)
{
  const GstStructure *s = gst_message_get_structure (message);
  GstObject *source = NULL;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT &&
      gst_structure_has_name (s, "prepare-window-handle")) {
    source = GST_MESSAGE_SRC (message);
    if (!g_strcmp0 (gst_object_get_name (source), "h264_sink"))
      gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (source),
          h264_xid);
    else
      gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (source),
          preview_xid);
  }

  return TRUE;
}

static void
set_drop_probability (Main * self)
{
  const gchar *drop;
  gdouble drop_probability = 0.0;

  drop = gtk_entry_get_text (GET_WIDGET (self, GTK_ENTRY, "drop"));
  drop_probability = g_ascii_strtod (drop, NULL);
  g_debug ("Setting drop probability to : %f", drop_probability);
  g_object_set (self->identity, "drop-probability", drop_probability, NULL);
}

static void
get_all_properties (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (properties); i++)
    on_get_button_clicked (NULL, &properties[i]);

}

static void
probe_all_properties (gboolean playing)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    gboolean return_value, changeable, default_bool;
    guint mask, minimum, maximum, default_int;
    GParamSpec *param;

    /* When playing, ignore static controls */
    if (playing && !properties[i].dynamic)
      continue;

    switch (properties[i].type) {
      case INT:
        g_signal_emit_by_name (G_OBJECT (properties[i].src), "get-int-setting",
            properties[i].property_name, &minimum, &default_int, &maximum,
            &return_value, NULL);
        if (return_value) {
          gchar *min, *def, *max;

          min = g_strdup_printf ("%d", minimum);
          def = g_strdup_printf ("%d", default_int);
          max = g_strdup_printf ("%d", maximum);
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "minimum"), min);
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "default"), def);
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "maximum"), max);
          g_free (min);
          g_free (def);
          g_free (max);
        } else {
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "minimum"), "");
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "default"), "");
          gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "maximum"), "");
        }
        break;
      case ENUM:
        g_signal_emit_by_name (G_OBJECT (properties[i].src), "get-enum-setting",
            properties[i].property_name, &mask, &default_int, &return_value,
            NULL);
        param =
            g_object_class_find_property (G_OBJECT_GET_CLASS (properties
                [i].src), properties[i].property_name);
        if (G_IS_PARAM_SPEC_ENUM (param)) {
          GEnumValue *values;
          guint j = 0;

          values = G_ENUM_CLASS (g_type_class_ref (param->value_type))->values;

          if (return_value) {
            while (values[j].value_name) {
              if (values[j].value == default_int) {
                gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "default"),
                    values[j].value_name);
                break;
              }
              j++;
            }
          } else {
            gtk_entry_set_text (GET_PROP_WIDGET (GTK_ENTRY, "default"), "");
          }

          j = 0;
          while (values[j].value_name) {
#if !GTK_CHECK_VERSION (2, 24, 0)
            gtk_combo_box_remove_text (GET_PROP_WIDGET ((GtkComboBox *),
                    "value"), 0);
#else
            gtk_combo_box_text_remove (GET_PROP_WIDGET ((GtkComboBoxText *),
                    "value"), 0);
#endif
            j++;
          }

          j = 0;
          while (values[j].value_name) {
            gchar *val;
            if (return_value && (mask & (1 << values[j].value)) != 0)
              val = g_strdup_printf ("**%s**", values[j].value_name);
            else
              val = g_strdup (values[j].value_name);

#if !GTK_CHECK_VERSION (2, 24, 0)
            gtk_combo_box_append_text (GET_PROP_WIDGET ((GtkComboBox *),
                    "value"), val);
#else
            gtk_combo_box_text_append_text (GET_PROP_WIDGET ((GtkComboBoxText
                        *), "value"), val);
#endif
            g_free (val);
            j++;
          }
        }
        break;
      case BOOL:
        g_signal_emit_by_name (G_OBJECT (properties[i].src),
            "get-boolean-setting", properties[i].property_name,
            &changeable, &default_bool, &return_value, NULL);
        if (return_value) {
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "value"),
              changeable);
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "get"),
              changeable);
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "set"),
              changeable);
          gtk_toggle_button_set_active (GET_PROP_WIDGET ((GtkToggleButton *),
                  "default"), default_bool);
        }
        break;
      case NONE:
      default:
        break;
    }
  }
}

int
main (int argc, char *argv[])
{
  Main self = { NULL, NULL, NULL, NULL };
  GstBus *bus = NULL;
  GtkWidget *window, *static_vbox, *dynamic_vbox, *da;
  gchar *drop;
  gdouble drop_probability;
  GdkWindow *gdk_win = NULL;
  const char *device = "/dev/video0";
  GError *error = NULL;
  int i;

  /* FIXME: add support for Gdk Wayland backend, code currently assumes X11 */
  if (g_getenv ("GDK_BACKEND") == NULL)
    g_setenv ("GDK_BACKEND", "x11", TRUE);

  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);

  if (argc > 1)
    device = argv[1];
  else
    g_print ("Usage : %s [device]\nUsing default device : %s\n",
        argv[0], device);


  self.bin = gst_parse_launch ("uvch264src name=src src.vidsrc ! queue ! "
      "capsfilter name=vid_cf ! identity name=identity ! decodebin ! "
      "xvimagesink name=h264_sink async=false "
      "src.vfsrc ! queue ! capsfilter name=vf_cf ! "
      "xvimagesink name=preview_sink async=false", NULL);

  if (!self.bin)
    return -1;

  /* Listen to the bus for messages */
  bus = gst_element_get_bus (self.bin);
  gst_bus_add_watch (bus, _bus_callback, self.bin);
  gst_object_unref (bus);

  self.src = gst_bin_get_by_name (GST_BIN (self.bin), "src");
  self.identity = gst_bin_get_by_name (GST_BIN (self.bin), "identity");
  self.vid_capsfilter = gst_bin_get_by_name (GST_BIN (self.bin), "vid_cf");
  self.vf_capsfilter = gst_bin_get_by_name (GST_BIN (self.bin), "vf_cf");

  self.builder = gtk_builder_new ();
  gtk_builder_add_from_file (self.builder, WINDOW_GLADE, &error);
  if (error) {
    g_debug ("Unable to load glade file : %s", error->message);
    goto end;
  }
  gtk_builder_connect_signals (self.builder, &self);

  g_object_get (self.identity, "drop-probability", &drop_probability, NULL);
  drop = g_strdup_printf ("%f", drop_probability);
  gtk_entry_set_text (GET_WIDGET (&self, GTK_ENTRY, "drop"), drop);
  g_free (drop);
  window = GET_WIDGET (&self, GTK_WIDGET, "window");
  static_vbox = GET_WIDGET (&self, GTK_WIDGET, "static");
  dynamic_vbox = GET_WIDGET (&self, GTK_WIDGET, "dynamic");
  da = GET_WIDGET (&self, GTK_WIDGET, "h264");
  gtk_widget_realize (da);
  gdk_win = gtk_widget_get_window (da);
  h264_xid = GDK_WINDOW_XID (gdk_win);
  da = GET_WIDGET (&self, GTK_WIDGET, "preview");
  gtk_widget_realize (da);
  gdk_win = gtk_widget_get_window (da);
  preview_xid = GDK_WINDOW_XID (gdk_win);

  set_caps (&self, FALSE);

  g_object_set (self.src, "device", device, NULL);
  if (gst_element_set_state (self.bin, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE) {
    g_debug ("Unable to go to state READY");
    goto end;
  }

  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    switch (properties[i].type) {
      case INT:
        properties[i].src = self.src;
        properties[i].builder = gtk_builder_new ();
        gtk_builder_add_from_file (properties[i].builder, INT_PROPERTY_GLADE,
            NULL);
        gtk_builder_connect_signals (properties[i].builder, &properties[i]);
        gtk_box_pack_start (PROPERTY_TO_VBOX,
            GET_PROP_WIDGET (GTK_WIDGET, "int-property"), TRUE, TRUE, 2);
        gtk_label_set_label (GET_PROP_WIDGET (GTK_LABEL, "label"),
            properties[i].property_name);
        if (properties[i].readonly)
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "set"), FALSE);
        break;
      case ENUM:
        properties[i].src = self.src;
        properties[i].builder = gtk_builder_new ();
#if !GTK_CHECK_VERSION (2, 24, 0)
        gtk_builder_add_from_file (properties[i].builder,
            "enum_property_gtk2.glade", NULL);
#else
        gtk_builder_add_from_file (properties[i].builder, ENUM_PROPERTY_GLADE,
            NULL);
#endif
        gtk_builder_connect_signals (properties[i].builder, &properties[i]);
        gtk_box_pack_start (PROPERTY_TO_VBOX,
            GET_PROP_WIDGET (GTK_WIDGET, "enum-property"), TRUE, TRUE, 2);
        gtk_label_set_label (GET_PROP_WIDGET (GTK_LABEL, "label"),
            properties[i].property_name);
#if !GTK_CHECK_VERSION (2, 24, 0)
        {
          GtkComboBox *combo_box;
          GtkCellRenderer *cell;
          GtkListStore *store;

          combo_box = GET_PROP_WIDGET ((GtkComboBox *), "value");
          store = gtk_list_store_new (1, G_TYPE_STRING);
          gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
          g_object_unref (store);

          cell = gtk_cell_renderer_text_new ();
          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
          gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
              "text", 0, NULL);
        }
#endif
        if (properties[i].readonly)
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "set"), FALSE);
        break;
      case BOOL:
        properties[i].src = self.src;
        properties[i].builder = gtk_builder_new ();
        gtk_builder_add_from_file (properties[i].builder, BOOL_PROPERTY_GLADE,
            NULL);
        gtk_builder_connect_signals (properties[i].builder, &properties[i]);
        gtk_box_pack_start (PROPERTY_TO_VBOX,
            GET_PROP_WIDGET (GTK_WIDGET, "boolean-property"), TRUE, TRUE, 2);
        gtk_label_set_label (GET_PROP_WIDGET (GTK_LABEL, "label"),
            properties[i].property_name);
        if (properties[i].readonly)
          gtk_widget_set_sensitive (GET_PROP_WIDGET (GTK_WIDGET, "set"), FALSE);
        break;
      case NONE:
      default:
        break;
    }
  }
  probe_all_properties (FALSE);
  get_all_properties ();

  gtk_widget_show (window);
  gtk_main ();

end:
  g_object_unref (G_OBJECT (self.builder));
  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    if (properties[i].builder)
      g_object_unref (G_OBJECT (properties[i].builder));
  }
  gst_element_set_state (self.bin, GST_STATE_NULL);
  gst_object_unref (self.src);
  gst_object_unref (self.identity);
  gst_object_unref (self.vid_capsfilter);
  gst_object_unref (self.vf_capsfilter);
  gst_object_unref (self.bin);

  return 0;
}
