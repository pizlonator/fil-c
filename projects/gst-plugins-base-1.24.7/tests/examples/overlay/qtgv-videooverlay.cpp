/* GStreamer
 * Copyright (C) <2010> Alexander Bokovoy <ab@samba.org>
 *
 * qtgv-xoverlay: demonstrate overlay handling using qt graphics view
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qtgv-videooverlay.h"

#include <QApplication>
#include <QTimer>

#include <gst/video/videooverlay.h>

SinkPipeline::SinkPipeline(QGraphicsView *parent) : QObject(parent)
{
  GstStateChangeReturn sret;
  
  pipeline = gst_pipeline_new ("xvoverlay");
  src = gst_element_factory_make ("videotestsrc", NULL);

  if ((sink = gst_element_factory_make ("xvimagesink", NULL))) {
    sret = gst_element_set_state (sink, GST_STATE_READY);
    if (sret != GST_STATE_CHANGE_SUCCESS) {
      gst_element_set_state (sink, GST_STATE_NULL);
      gst_object_unref (sink);
    }
  } else if ((sink = gst_element_factory_make ("ximagesink", NULL))) {
    sret = gst_element_set_state (sink, GST_STATE_READY);
    if (sret != GST_STATE_CHANGE_SUCCESS) {
      gst_element_set_state (sink, GST_STATE_NULL);
      gst_object_unref (sink);
    }
  } else if (strcmp (DEFAULT_VIDEOSINK, "xvimagesink") != 0 &&
             strcmp (DEFAULT_VIDEOSINK, "ximagesink") != 0) {
    if ((sink = gst_element_factory_make (DEFAULT_VIDEOSINK, NULL))) {
      if (!GST_IS_BIN (sink)) {
        sret = gst_element_set_state (sink, GST_STATE_READY);
        if (sret != GST_STATE_CHANGE_SUCCESS) {
          gst_element_set_state (sink, GST_STATE_NULL);
          gst_object_unref (sink);
          sink = NULL;
        }
      } else {
        gst_object_unref (sink);
        sink = NULL;
      }
    }
  }

  if (sink == NULL)
    g_error ("Couldn't find a working video sink.");

  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link (src, sink);
  xwinid = parent->winId();
}

SinkPipeline::~SinkPipeline()
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

void SinkPipeline::startPipeline()
{
  GstStateChangeReturn sret;

  /* we know what the video sink is in this case (xvimagesink), so we can
   * just set it directly here now (instead of waiting for a
   * prepare-window-handle element message in a sync bus handler and setting
   * it there) */
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink), xwinid);

  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    /* Exit application */
    QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
  }
}

int main( int argc, char **argv )
{
    QApplication app(argc, argv);

    QGraphicsScene scene;
    scene.setSceneRect( -100.0, -100.0, 200.0, 200.0 );

    QGraphicsView view( &scene );
    view.resize(320, 240);
    view.setWindowTitle("GstVideoOverlay Qt GraphicsView demo");
    view.show();

    gst_init (&argc, &argv);
    SinkPipeline pipeline(&view);
    pipeline.startPipeline();

    int ret = app.exec();

    view.hide();
    
    return ret;
}
