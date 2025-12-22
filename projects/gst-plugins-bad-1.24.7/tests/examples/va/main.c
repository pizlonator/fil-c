#include <stdlib.h>

#include <X11/Xlib.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#else
#error "Only X11 is supported so far"
#endif

#include <gtk/gtk.h>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/va/gstva.h>
#include <gst/video/video.h>

#include <va/va_x11.h>

#define GST_MAP_VA (GST_MAP_FLAG_LAST << 1)

static gchar **INPUT_FILES = NULL;

struct _app
{
  GtkWidget *window;
  GtkWidget *video;
  GstElement *pipeline;
  GstSample *sample;
  GMutex mutex;
  VADisplay va_dpy;
};

static GstBusSyncReply
context_handler (GstBus * bus, GstMessage * msg, gpointer data)
{
  struct _app *app = data;
  const gchar *context_type;

  if (GST_MESSAGE_TYPE (msg) != GST_MESSAGE_NEED_CONTEXT)
    return GST_BUS_PASS;

  gst_message_parse_context_type (msg, &context_type);

  gst_println ("got need context %s", context_type);

  if (g_strcmp0 (context_type, GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR) == 0) {
    GstContext *context;
    GstStructure *s;

    context = gst_context_new (GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR, TRUE);
    s = gst_context_writable_structure (context);
    gst_structure_set (s, "va-display", G_TYPE_POINTER, app->va_dpy, NULL);
    gst_element_set_context (GST_ELEMENT (msg->src), context);
    gst_context_unref (context);
  }

  gst_message_unref (msg);

  return GST_BUS_DROP;
}

static void
delete_event_cb (GtkWidget * widget, GdkEvent * event, gpointer data)
{
  gtk_main_quit ();
}

static gboolean
draw_unlocked (GtkWidget * widget, struct _app *app)
{
  GstBuffer *buffer;
  GstCaps *caps;
  GstMapInfo map_info;
  GstVideoInfo vinfo;
  VASurfaceID surface;
  VAStatus va_status;
  GstVideoRectangle src, dst, res;
  gboolean ret = FALSE;

  buffer = gst_sample_get_buffer (app->sample);
  caps = gst_sample_get_caps (app->sample);

  if (!gst_video_info_from_caps (&vinfo, caps))
    return FALSE;

  src.x = 0;
  src.y = 0;
  src.w = GST_VIDEO_INFO_WIDTH (&vinfo);
  src.h = GST_VIDEO_INFO_HEIGHT (&vinfo);

  if (!gst_buffer_map (buffer, &map_info, GST_MAP_READ | GST_MAP_VA))
    return FALSE;

  surface = (*(VASurfaceID *) map_info.data);
  if (surface == VA_INVALID_ID)
    goto bail;

  dst.x = 0;
  dst.y = 0;
  dst.w = gtk_widget_get_allocated_width (widget);
  dst.h = gtk_widget_get_allocated_height (widget);

  gst_video_sink_center_rect (src, dst, &res, TRUE);

  va_status = vaPutSurface (app->va_dpy, surface,
      GDK_WINDOW_XID (gtk_widget_get_window (widget)),
      src.x, src.y, src.w, src.h, res.x, res.y, res.w, res.h, NULL, 0, 0);
  if (va_status != VA_STATUS_SUCCESS)
    gst_printerrln ("failed vaPutSurface: %s", vaErrorStr (va_status));
  else
    ret = TRUE;

bail:
  gst_buffer_unmap (buffer, &map_info);

  return ret;
}

static gboolean
redraw_cb (gpointer data)
{
  GtkWidget *video = data;

  gtk_widget_queue_draw (video);
  return G_SOURCE_REMOVE;
}

static gboolean
draw_cb (GtkWidget * widget, cairo_t * cr, gpointer data)
{
  struct _app *app = data;
  gboolean ret = TRUE;

  g_mutex_lock (&app->mutex);
  if (app->sample)
    ret = draw_unlocked (widget, app);
  g_mutex_unlock (&app->mutex);

  if (!ret)
    gst_printerrln ("failed to paint frame");

  return FALSE;
}

static void
build_ui (struct _app *app)
{
  app->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (app->window), "VA X11 render");
  g_signal_connect (app->window, "delete-event", G_CALLBACK (delete_event_cb),
      app);

  app->video = gtk_drawing_area_new ();
  g_signal_connect (app->video, "draw", G_CALLBACK (draw_cb), app);

  gtk_container_add (GTK_CONTAINER (app->window), app->video);

  gtk_widget_show_all (app->window);
}

static GstFlowReturn
new_sample_cb (GstAppSink * sink, gpointer data)
{
  struct _app *app = data;

  g_mutex_lock (&app->mutex);
  if (app->sample)
    gst_sample_unref (app->sample);
  app->sample = gst_app_sink_pull_sample (sink);
  g_mutex_unlock (&app->mutex);

  g_idle_add (redraw_cb, app->video);

  return GST_FLOW_OK;
}

static gboolean
message_handler (GstBus * bus, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      gtk_main_quit ();
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);
      gst_printerrln ("GStreamer error: %s\n%s", err->message,
          debug ? debug : "");
      if (debug)
        g_free (debug);
      if (err)
        g_error_free (err);

      gtk_main_quit ();
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
build_pipeline (struct _app *app)
{
  GstElement *src, *sink;
  GstCaps *caps;
  GstBus *bus;
  GstAppSinkCallbacks callbacks = {
    .new_sample = new_sample_cb,
  };
  GError *err = NULL;

  app->pipeline = gst_parse_launch ("filesrc name=src ! "
      "parsebin ! vah264dec ! appsink name=sink", &err);
  if (err) {
    gst_printerrln ("Couldn't create pipeline: %s", err->message);
    g_error_free (err);
    return FALSE;
  }

  src = gst_bin_get_by_name (GST_BIN (app->pipeline), "src");
  g_object_set (src, "location", INPUT_FILES[0], NULL);
  gst_object_unref (src);

  sink = gst_bin_get_by_name (GST_BIN (app->pipeline), "sink");
  caps = gst_caps_from_string ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_VA ")");
  g_object_set (sink, "caps", caps, NULL);
  gst_caps_unref (caps);
  gst_app_sink_set_callbacks (GST_APP_SINK (sink), &callbacks, app, NULL);
  gst_object_unref (sink);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  gst_bus_set_sync_handler (bus, context_handler, app, NULL);
  gst_bus_add_watch (bus, message_handler, app);
  gst_object_unref (bus);

  return TRUE;
}

static gboolean
parse_arguments (int *argc, char ***argv)
{
  GOptionContext *ctxt;
  GError *err = NULL;
  const GOptionEntry options[] = {
    {G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME_ARRAY,
        &INPUT_FILES, "H.264 video files to play", NULL},
    {NULL,},
  };

  ctxt = g_option_context_new ("— VA X11 render");
  g_option_context_add_main_entries (ctxt, options, NULL);
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));
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
  GdkDisplay *gdk_dpy;
  GstBus *bus;
  VAStatus va_status;
  struct _app app = { NULL, };
  int maj, min, ret = EXIT_FAILURE;

#if defined (GDK_WINDOWING_X11)
  XInitThreads ();
#endif

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  if (!(INPUT_FILES && INPUT_FILES[0]))
    goto gtk_failed;

  gdk_dpy = gdk_display_get_default ();
  if (!GDK_IS_X11_DISPLAY (gdk_dpy)) {
    gst_printerrln ("This example is only for native X11");
    goto gtk_failed;
  }

  g_mutex_init (&app.mutex);

  if (!build_pipeline (&app))
    goto gst_failed;

  app.va_dpy = vaGetDisplay (GDK_DISPLAY_XDISPLAY (gdk_dpy));
  va_status = vaInitialize (app.va_dpy, &maj, &min);
  if (va_status != VA_STATUS_SUCCESS) {
    gst_printerrln ("failed to initialize VA: %s", vaErrorStr (va_status));
    goto va_failed;
  }

  build_ui (&app);

  gst_element_set_state (app.pipeline, GST_STATE_PLAYING);

  gtk_main ();

  if (app.sample)
    gst_sample_unref (app.sample);

  gst_element_set_state (app.pipeline, GST_STATE_NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (app.pipeline));
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);

  ret = EXIT_SUCCESS;

va_failed:
  gst_object_unref (app.pipeline);
  vaTerminate (app.va_dpy);
gst_failed:
  g_mutex_clear (&app.mutex);
gtk_failed:
  g_strfreev (INPUT_FILES);
  gst_deinit ();

  return ret;
}
