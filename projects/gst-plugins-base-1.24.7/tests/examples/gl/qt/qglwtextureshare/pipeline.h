/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2009 Andrey Nechypurenko <andreynech@gmail.com>
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

#include <QObject>

#include <gst/gl/gstglcontext.h>

#include "AsyncQueue.h"


class Pipeline : public QObject
{
    Q_OBJECT

public:
    Pipeline(GstGLDisplay *display, GstGLContext *context,
        const QString &videoLocation,
        QObject *parent);
    ~Pipeline();

    void configure();
    void start();
    void notifyNewFrame() {emit newFrameReady();}
    void stop();
    void unconfigure();

    AsyncQueue<GstBuffer*> queue_input_buf;
    AsyncQueue<GstBuffer*> queue_output_buf;

Q_SIGNALS:
    void newFrameReady();
    void stopRequested();

private:
    GstGLDisplay *display;
    GstGLContext *context;
    const QString m_videoLocation;
    GMainLoop* m_loop;
    GstBus* m_bus;
    GstPipeline* m_pipeline;
    static float m_xrot;
    static float m_yrot;
    static float m_zrot;

    static void on_gst_buffer(GstElement * element, GstBuffer * buf, GstPad * pad, Pipeline* p);
    static gboolean bus_call (GstBus *bus, GstMessage *msg, Pipeline* p);
    static gboolean sync_bus_call (GstBus *bus, GstMessage *msg, Pipeline* p);
};

#endif
