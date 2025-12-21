/*
 * GStreamer
 * Copyright (C) 2008-2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef PIPELINE_H
#define PIPELINE_H

#include <QtGui>
#include <gst/gst.h>
//#include <QtCore/private/qeventdispatcher_glib_p.h>

class Pipeline : public QObject
{
    Q_OBJECT

public:
    Pipeline(const WId windId, const QString videoLocation);
    ~Pipeline();
    void start();
    void exposeRequested();
    void stop();
    void unconfigure() const;
    void show();
    GstElement* getVideoSink() { return m_glimagesink; } ;

signals:
    void showRequested();
    void stopRequested();

private:
    const WId m_winId;
    const QString m_videoLocation;
    GMainLoop* m_loop;
    GstBus* m_bus;
    GstElement* m_pipeline;
    GstElement* m_glimagesink;
    static float m_xrot;
    static float m_yrot;
    static float m_zrot;

    void create();
    WId winId() const { return m_winId; }
    void doExpose() const;

    static gboolean reshapeCallback (GstElement *sink, void *context, guint width, guint height, gpointer data);
    static gboolean drawCallback (GstElement *sink, void *context, GstSample * sample, gpointer data);
    static gboolean bus_call (GstBus *bus, GstMessage *msg, Pipeline* p);
    static void cb_new_pad (GstElement* decodebin, GstPad* pad, Pipeline* p);
    static gboolean cb_expose (gpointer data);
    static GstBusSyncReply create_window (GstBus* bus, GstMessage* message, const Pipeline* pipeline);
};

#endif
