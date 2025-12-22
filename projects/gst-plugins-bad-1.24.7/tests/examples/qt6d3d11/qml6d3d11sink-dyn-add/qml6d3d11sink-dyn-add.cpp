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
#include <QTimer>
#include <gst/gst.h>

static int
run_application (int argc, char ** argv, GstElement * pipeline)
{
  QGuiApplication app(argc, argv);
  int ret;

  /* NOTE: Default API on Windows is D3D11 already */
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);

  QQmlApplicationEngine engine;
  engine.load (QUrl (QStringLiteral ("qrc:/qml6d3d11sink-dyn-add.qml")));

  QQuickWindow *rootObject;

  rootObject = static_cast<QQuickWindow *> (engine.rootObjects().first());

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GstElement *tee = gst_bin_get_by_name (GST_BIN (pipeline), "t");
  /* Add the qml6d3d11sink element on timer callback */
  QTimer::singleShot(5000, [&]()
    {
      GstElement *queue, *sink;
      QQuickItem *videoItem = rootObject->findChild<QQuickItem *> ("videoItem");

      queue = gst_element_factory_make ("queue", nullptr);
      sink = gst_element_factory_make ("qml6d3d11sink", nullptr);

      g_object_set (sink, "widget", videoItem, nullptr);

      gst_println ("Adding new qml6d3d11sink to pipeline");

      gst_bin_add_many (GST_BIN (pipeline), queue, sink, nullptr);

      gst_element_sync_state_with_parent (sink);
      gst_element_sync_state_with_parent (queue);

      gst_element_link_many (tee, queue, sink, nullptr);

      gst_object_unref (tee);
    });

  ret = app.exec();

  gst_element_set_state (pipeline, GST_STATE_NULL);

  return ret;
}

int
main(int argc, char *argv[])
{
  GstElement *pipeline = nullptr;
  GstElement *sink = nullptr;
  int exit_code;

  gst_init (&argc, &argv);

  pipeline = gst_parse_launch ("d3d11testsrc ! "
    "video/x-raw(memory:D3D11Memory),format=RGBA ! "
    "tee name=t allow-not-linked=true ! queue ! fakesink sync=true", nullptr);

  /* the plugin must be loaded before loading the qml file to register the
   * GstD3D11Qt6VideoItem */
  sink = gst_element_factory_make ("qml6d3d11sink", nullptr);
  gst_object_unref (sink);

  exit_code = run_application (argc, argv, pipeline);

  gst_object_unref (pipeline);

  gst_deinit ();

  return exit_code;
}
