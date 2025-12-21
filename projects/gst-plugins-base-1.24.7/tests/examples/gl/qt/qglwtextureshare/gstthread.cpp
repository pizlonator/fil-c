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

#include "pipeline.h"
#include "gstthread.h"


GstThread::GstThread(GstGLDisplay *display,
        GstGLContext *context,
        const QString &videoLocation,
        const char *renderer_slot,
        QObject *parent):
    QThread(parent),
    m_videoLocation(videoLocation)
{
    m_pipeline = new Pipeline(display, context, m_videoLocation, this);
    QObject::connect(m_pipeline, SIGNAL(newFrameReady()), this->parent(), renderer_slot, Qt::QueuedConnection);
}

GstThread::~GstThread()
{
}

void GstThread::stop()
{
    if(m_pipeline)
      m_pipeline->stop();
}

void GstThread::run()
{
    qDebug("Starting gst pipeline");
    m_pipeline->start(); //it runs the gmainloop on win32

#ifndef Q_WS_WIN
    //works like the gmainloop on linux (GstEvent are handled)
    connect(m_pipeline, SIGNAL(stopRequested()), this, SLOT(quit()));
    exec();
#endif

    m_pipeline->unconfigure();

    m_pipeline = NULL;
    // This is not a memory leak. Pipeline will be deleted
    // when the parent object (this) will be destroyed.
    // We set m_pipeline to NULL to prevent further attempts
    // to stop already stopped pipeline
}
