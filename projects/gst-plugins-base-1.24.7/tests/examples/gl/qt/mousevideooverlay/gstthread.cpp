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

#include "gstthread.h"

GstThread::GstThread(const WId winId, const QString videoLocation, QObject *parent):
    QThread(parent),
    m_winId(winId),
    m_videoLocation(videoLocation)
{
}

GstThread::~GstThread()
{
}

void GstThread::exposeRequested()
{
    m_pipeline->exposeRequested();
}

void GstThread::onMouseMove()
{
    m_pipeline->rotateRequested();
}

void GstThread::show()
{
    emit showRequested();
}

void GstThread::stop()
{
    m_pipeline->stop();
}

void GstThread::run()
{
    m_pipeline = new Pipeline(m_winId, m_videoLocation);
    connect(m_pipeline, SIGNAL(showRequested()), this, SLOT(show()));
    m_pipeline->start(); //it runs the gmainloop on win32

#ifndef WIN32
    //works like the gmainloop on linux (GstEvent are handled)
    connect(m_pipeline, SIGNAL(stopRequested()), this, SLOT(quit()));
    exec();
#endif
    
    m_pipeline->unconfigure();
}
