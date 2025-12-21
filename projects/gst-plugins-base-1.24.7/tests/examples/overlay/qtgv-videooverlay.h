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

#ifndef QTGV_XOVERLAY_H
#define QTGV_XOVERLAY_H

#include <QGraphicsView>
#include <gst/gst.h>


class SinkPipeline : public QObject
{
    Q_OBJECT
public:
    SinkPipeline(QGraphicsView *parent = 0);
    ~SinkPipeline();

    void startPipeline();

private:
    GstElement *pipeline;
    GstElement *sink;
    GstElement *src;
    WId xwinid;
};

#endif // QTGV_XOVERLAY_H
