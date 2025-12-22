/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>

#include <gst/gst.h>

class PipelineRunner : public QRunnable
{
public:
  PipelineRunner(GstElement * p);
  ~PipelineRunner();

  void run ();

private:
  GstElement *pipeline;
};

PipelineRunner::PipelineRunner (GstElement * p)
{
  pipeline = (GstElement *) gst_object_ref (p);
}

PipelineRunner::~PipelineRunner ()
{
  gst_object_unref (this->pipeline);
}

void
PipelineRunner::run ()
{
  gst_element_set_state (this->pipeline, GST_STATE_PLAYING);
}

struct AppData
{
  QGuiApplication *app;
  GstElement *pipeline;
  GMainLoop *loop;
  GMainContext *context;
};

static gboolean
message_cb (GstBus * bus, GstMessage * msg, AppData * data)
{
  gboolean do_exit = FALSE;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_println ("Got pipeline error");
      do_exit = TRUE;
      break;
    case GST_MESSAGE_EOS:
      gst_println ("Got pipeline EOS");
      do_exit = TRUE;
      break;
    default:
      break;
  }

  if (do_exit) {
    g_main_loop_quit (data->loop);
    data->app->quit ();
  }

  return G_SOURCE_CONTINUE;
}

static gpointer
pipeline_watch_thread (AppData * data)
{
  GstBus *bus;

  g_main_context_push_thread_default (data->context);

  bus = gst_pipeline_get_bus (GST_PIPELINE (data->pipeline));
  gst_bus_add_watch (bus, (GstBusFunc) message_cb, data);

  g_main_loop_run (data->loop);

  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  g_main_context_pop_thread_default (data->context);

  return nullptr;
}

static int
run_application (int argc, char ** argv, GstElement * pipeline,
    GstElement * sink)
{
  QGuiApplication app (argc, argv);
  GThread *bus_thread;
  int ret;

  /* NOTE: Default API on Windows is D3D11 already */
  QQuickWindow::setGraphicsApi (QSGRendererInterface::Direct3D11);

  QQmlApplicationEngine engine;
  engine.load (QUrl (QStringLiteral ("qrc:/qml6d3d11sink.qml")));

  QQuickItem *videoItem;
  QQuickWindow *rootObject;
  AppData app_data;

  app_data.app = &app;
  app_data.pipeline = pipeline;
  app_data.context = g_main_context_new ();
  app_data.loop = g_main_loop_new (app_data.context, FALSE);

  /* find and set the videoItem on the sink */
  rootObject = static_cast<QQuickWindow *> (engine.rootObjects().first());
  videoItem = rootObject->findChild<QQuickItem *> ("videoItem");
  g_assert (videoItem);
  g_object_set (sink, "widget", videoItem, nullptr);

  rootObject->scheduleRenderJob (new PipelineRunner (pipeline),
      QQuickWindow::BeforeSynchronizingStage);

  bus_thread = g_thread_new ("pipeline-watch-thread",
      (GThreadFunc) pipeline_watch_thread, &app_data);

  ret = app.exec();

  g_main_loop_quit (app_data.loop);
  g_thread_join (bus_thread);
  g_main_loop_unref (app_data.loop);
  g_main_context_unref (app_data.context);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  return ret;
}

int
main(int argc, char *argv[])
{
  GOptionContext *option_ctx;
  gchar *uri = nullptr;
  GError *err = nullptr;
  gboolean ret;
  GOptionEntry options[] = {
    {"uri", 0, 0, G_OPTION_ARG_STRING, &uri, "URI to play", nullptr},
    {nullptr, }
  };
  GstElement *pipeline = nullptr;
  GstElement *sink = nullptr;
  int exit_code;

  option_ctx = g_option_context_new ("Qt6 Direct3D11 QML render");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &err);
  g_option_context_free (option_ctx);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", err->message);
    g_clear_error (&err);
    return 1;
  }

  /* the plugin must be loaded before loading the qml file to register the
   * GstD3D11Qt6VideoItem */
  sink = gst_element_factory_make ("qml6d3d11sink", nullptr);

  if (!uri) {
    pipeline = gst_pipeline_new (nullptr);

    GstElement *src = gst_element_factory_make ("videotestsrc", nullptr);
    GstElement *upload = gst_element_factory_make ("d3d11upload", nullptr);
    GstElement *capsfilter = gst_element_factory_make ("capsfilter", nullptr);
    GstCaps *caps;

    caps = gst_caps_from_string ("video/x-raw(memory:D3D11Memory),format=RGBA");
    g_object_set (capsfilter, "caps", caps, nullptr);
    gst_caps_unref (caps);

    gst_bin_add_many (GST_BIN (pipeline),
        src, upload, capsfilter, sink, nullptr);
    gst_element_link_many (src, upload, capsfilter, sink, nullptr);
  } else {
    pipeline = gst_element_factory_make ("playbin", nullptr);

    g_object_set (pipeline, "uri", uri, "video-sink", sink, nullptr);
  }

  exit_code = run_application (argc, argv, pipeline, sink);

  gst_object_unref (pipeline);

  gst_deinit ();

  return exit_code;
}
