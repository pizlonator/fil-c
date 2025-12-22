#include <stdlib.h>

#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/gst.h>
#include <gst/va/gstva.h>
#include <gst/video/video.h>

#define CHANGE_DIR_WITH_EVENT 0

static gint num_buffers = 50;
static gboolean camera = FALSE;
static gboolean randomcb = FALSE;
static gboolean randomdir = FALSE;
static gboolean randomsharpen = FALSE;
static gboolean randomcrop = FALSE;

static GOptionEntry entries[] = {
  {"num-buffers", 'n', 0, G_OPTION_ARG_INT, &num_buffers,
      "Number of buffers (<= 0 : forever)", "N"},
  {"camera", 'c', 0, G_OPTION_ARG_NONE, &camera,
      "Use default v4l2src as video source", NULL},
  {"random-cb", 'r', 0, G_OPTION_ARG_NONE, &randomcb,
      "Change colorbalance randomly every second (if supported)", NULL},
  {"random-dir", 'd', 0, G_OPTION_ARG_NONE, &randomdir,
      "Change video direction randomly every second (if supported)", NULL},
  {"random-sharpen", 's', 0, G_OPTION_ARG_NONE, &randomsharpen,
      "Change sharpen filter randomly every second (if supported)", NULL},
  {"random-crop", 'p', 0, G_OPTION_ARG_NONE, &randomcrop,
      "Change cropping randomly every 150 miliseconds", NULL},
  {NULL},
};

struct _app
{
  GMainLoop *loop;
  GstObject *display;
  GstElement *pipeline;
  GstElement *vpp;
  GstElement *crop;
  GMutex mutex;

  GstControlSource *sharpen;
  gint right, left, top, bottom;
  gint ldir, rdir, tdir, bdir;
};

static GstBusSyncReply
context_handler (GstBus * bus, GstMessage * msg, gpointer data)
{
  struct _app *app = data;
  const gchar *context_type;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_HAVE_CONTEXT:{
      GstContext *context = NULL;

      gst_message_parse_have_context (msg, &context);
      if (context) {
        context_type = gst_context_get_context_type (context);

        if (g_strcmp0 (context_type,
                GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) == 0) {
          const GstStructure *s = gst_context_get_structure (context);
          GstObject *display = NULL;

          gst_printerr ("got have context %s from %s: ", context_type,
              GST_MESSAGE_SRC_NAME (msg));

          gst_structure_get (s, "gst-display", GST_TYPE_OBJECT, &display, NULL);
          gst_printerrln ("%s", display ?
              GST_OBJECT_NAME (display) : "no gst display");
          gst_context_unref (context);

          if (display) {
            g_mutex_lock (&app->mutex);
            gst_object_replace (&app->display, display);
            gst_object_unref (display);
            g_mutex_unlock (&app->mutex);
          }
        }
      }

      gst_message_unref (msg);

      return GST_BUS_DROP;
    }
    case GST_MESSAGE_NEED_CONTEXT:
      gst_message_parse_context_type (msg, &context_type);

      if (g_strcmp0 (context_type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) == 0) {
        GstContext *context;
        GstStructure *s;

        gst_printerr ("got need context %s from %s: ", context_type,
            GST_MESSAGE_SRC_NAME (msg));

        g_mutex_lock (&app->mutex);
        if (!app->display) {
          g_mutex_unlock (&app->mutex);
          gst_printerrln ("no gst display yet");
          gst_message_unref (msg);
          return GST_BUS_DROP;
        }

        context =
            gst_context_new (GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR, TRUE);
        s = gst_context_writable_structure (context);
        gst_structure_set (s, "gst-display", GST_TYPE_OBJECT, app->display,
            NULL);
        gst_printerrln ("%s", GST_OBJECT_NAME (app->display));
        gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (msg)), context);
        gst_context_unref (context);
        g_mutex_unlock (&app->mutex);

      }

      gst_message_unref (msg);

      return GST_BUS_DROP;

    default:
      break;
  }

  return GST_BUS_PASS;
}

static gboolean
message_handler (GstBus * bus, GstMessage * msg, gpointer data)
{
  struct _app *app = data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (app->loop);
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);

      if (err) {
        gst_printerrln ("GStreamer error: %s\n%s", err->message,
            debug ? debug : "");
        g_error_free (err);
      }

      if (debug)
        g_free (debug);

      g_main_loop_quit (app->loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static void
config_simple (struct _app *app)
{
  GParamSpec *pspec;
  GObjectClass *g_class = G_OBJECT_GET_CLASS (app->vpp);
  const static gchar *props[] = { "brightness", "hue", "saturation",
    "contrast"
  };
  gfloat max;
  guint i;

  if (camera && (pspec = g_object_class_find_property (g_class, "skin-tone"))) {
    if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_BOOLEAN) {
      g_object_set (app->vpp, "skin-tone", TRUE, NULL);
    } else {
      max = ((GParamSpecFloat *) pspec)->maximum;
      g_object_set (app->vpp, "skin-tone", max, NULL);
    }

    return;
  }

  for (i = 0; i < G_N_ELEMENTS (props); i++) {
    pspec = g_object_class_find_property (g_class, props[i]);
    if (!pspec)
      continue;

    max = ((GParamSpecFloat *) pspec)->maximum;
    g_object_set (app->vpp, props[i], max, NULL);
  }
}

static gboolean
build_pipeline (struct _app *app)
{
  GstElement *src;
  GstBus *bus;
  GError *err = NULL;
  GString *cmd = g_string_new (NULL);
  const gchar *source = camera ? "v4l2src" : "videotestsrc";

  g_string_printf (cmd, "%s name=src ! tee name=t "
      "t. ! queue ! videocrop name=crop ! vapostproc name=vpp ! "
      "fpsdisplaysink video-sink=autovideosink "
      "t. ! queue ! vapostproc ! timeoverlay ! autovideosink", source);

  app->pipeline = gst_parse_launch (cmd->str, &err);
  g_string_free (cmd, TRUE);
  if (err) {
    gst_printerrln ("Couldn't create pipeline: %s", err->message);
    g_error_free (err);
    return FALSE;
  }

  if (num_buffers > 0) {
    src = gst_bin_get_by_name (GST_BIN (app->pipeline), "src");
    g_object_set (src, "num-buffers", num_buffers, NULL);
    gst_object_unref (src);
  }

  app->vpp = gst_bin_get_by_name (GST_BIN (app->pipeline), "vpp");
  if (!randomcb && !randomdir && !randomsharpen && !randomcrop)
    config_simple (app);

  app->crop = gst_bin_get_by_name (GST_BIN (app->pipeline), "crop");

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  gst_bus_set_sync_handler (bus, context_handler, app, NULL);
  gst_bus_add_watch (bus, message_handler, app);
  gst_object_unref (bus);

  return TRUE;
}

static gboolean
change_cb_randomly (gpointer data)
{
  struct _app *app = data;
  GstColorBalance *cb;
  GList *channels;

  if (!GST_COLOR_BALANCE_GET_INTERFACE (app->vpp))
    return G_SOURCE_REMOVE;

  cb = GST_COLOR_BALANCE (app->vpp);
  channels = (GList *) gst_color_balance_list_channels (cb);
  for (; channels && channels->data; channels = channels->next) {
    GstColorBalanceChannel *channel = channels->data;
    gint value =
        g_random_int_range (channel->min_value, channel->max_value + 1);

    gst_color_balance_set_value (cb, channel, value);
  }

  return G_SOURCE_CONTINUE;
}

static gboolean
change_dir_randomly (gpointer data)
{
  struct _app *app = data;
  GObjectClass *g_class = G_OBJECT_GET_CLASS (app->vpp);
  GParamSpec *pspec;

  pspec = g_object_class_find_property (g_class, "video-direction");
  if (!pspec)
    return G_SOURCE_REMOVE;

  /* choose either sent direction by property or by event */
#if !CHANGE_DIR_WITH_EVENT
  {
    GEnumClass *enumclass;
    guint idx, value;

    enumclass = G_PARAM_SPEC_ENUM (pspec)->enum_class;
    idx = g_random_int_range (0, enumclass->n_values);
    value = enumclass->values[idx].value;

    g_object_set (app->vpp, "video-direction", value, NULL);
  }
#else
  {
    GstEvent *event;
    guint idx;
    static const gchar *orientation[] = {
      "rotate-0", "rotate-90", "rotate-180", "rotate-270",
      "flip-rotate-0", "flip-rotate-90", "flip-rotate-180", "flip-rotate-270",
      "undefined",
    };

    idx = g_random_int_range (0, G_N_ELEMENTS (orientation));

    event = gst_event_new_tag (gst_tag_list_new (GST_TAG_IMAGE_ORIENTATION,
            orientation[idx], NULL));
    gst_element_send_event (app->pipeline, event);
  }
#endif

  return G_SOURCE_CONTINUE;
}

static inline GParamSpec *
vpp_has_sharpen (GstElement * vpp)
{
  GObjectClass *g_class = G_OBJECT_GET_CLASS (vpp);
  return g_object_class_find_property (g_class, "sharpen");
}

static gboolean
change_sharpen_randomly (gpointer data)
{
  struct _app *app = data;
  GParamSpec *pspec;
  gdouble value;

  pspec = vpp_has_sharpen (app->vpp);
  if (!pspec)
    return G_SOURCE_REMOVE;
  value = g_random_double_range (G_PARAM_SPEC_FLOAT (pspec)->minimum,
      G_PARAM_SPEC_FLOAT (pspec)->maximum);

  gst_timed_value_control_source_set (GST_TIMED_VALUE_CONTROL_SOURCE
      (app->sharpen), GST_SECOND, value);

  return G_SOURCE_CONTINUE;
}

static gboolean
change_crop_randomly (gpointer data)
{
  struct _app *app = data;

  g_object_set (app->crop, "bottom", app->bottom, "top", app->top, "left",
      app->left, "right", app->right, NULL);

  app->top += app->tdir;
  if (app->top >= 80)
    app->tdir = -10;
  else if (app->top < 10)
    app->tdir = 10;

  app->bottom += app->bdir;
  if (app->bottom >= 60)
    app->bdir = -10;
  else if (app->bottom < 10)
    app->bdir = 10;

  app->left += app->ldir;
  if (app->left >= 100)
    app->ldir = -10;
  else if (app->left < 10)
    app->ldir = 10;

  app->right += app->rdir;
  if (app->right >= 80)
    app->rdir = -10;
  else if (app->right < 10)
    app->rdir = 10;

  return G_SOURCE_CONTINUE;
}

static gboolean
parse_arguments (int *argc, char ***argv)
{
  GOptionContext *ctxt;
  GError *err = NULL;

  ctxt = g_option_context_new ("— Multiple VA postprocessors");
  g_option_context_add_main_entries (ctxt, entries, NULL);
  g_option_context_add_group (ctxt, gst_init_get_option_group ());

  if (!g_option_context_parse (ctxt, argc, argv, &err)) {
    gst_printerrln ("option parsing failed: %s", err->message);
    g_error_free (err);
    return FALSE;
  }

  g_option_context_free (ctxt);
  return TRUE;
}

int
main (int argc, char **argv)
{
  GstBus *bus;
  struct _app app = { NULL, };
  int ret = EXIT_FAILURE;

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  g_mutex_init (&app.mutex);

  app.loop = g_main_loop_new (NULL, TRUE);

  if (!build_pipeline (&app))
    goto gst_failed;

  if (randomcb)
    g_timeout_add_seconds (1, change_cb_randomly, &app);

  if (randomdir) {
#if CHANGE_DIR_WITH_EVENT
    gst_util_set_object_arg (G_OBJECT (app.vpp), "video-direction", "auto");
#endif
    g_timeout_add_seconds (1, change_dir_randomly, &app);
  }

  if (randomsharpen && vpp_has_sharpen (app.vpp)) {
    GstControlBinding *bind;

    app.sharpen = gst_interpolation_control_source_new ();
    bind = gst_direct_control_binding_new_absolute (GST_OBJECT (app.vpp),
        "sharpen", app.sharpen);
    gst_object_add_control_binding (GST_OBJECT (app.vpp), bind);
    g_object_set (app.sharpen, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

    change_sharpen_randomly (&app);
    g_timeout_add_seconds (1, change_sharpen_randomly, &app);
  }

  if (randomcrop) {
    app.bdir = app.ldir = app.rdir = app.tdir = 10;
    g_timeout_add (150, change_crop_randomly, &app);
  }

  gst_element_set_state (app.pipeline, GST_STATE_PLAYING);

  g_main_loop_run (app.loop);

  gst_element_set_state (app.pipeline, GST_STATE_NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app.pipeline));
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);

  gst_clear_object (&app.display);

  ret = EXIT_SUCCESS;

  gst_clear_object (&app.vpp);
  gst_clear_object (&app.pipeline);
  gst_clear_object (&app.sharpen);
  gst_clear_object (&app.crop);

gst_failed:
  g_mutex_clear (&app.mutex);
  g_main_loop_unref (app.loop);

  gst_deinit ();

  return ret;
}
