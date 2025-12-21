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

#ifndef GSTTHREAD_H
#define GSTTHREAD_H

#include <QThread>

#include <gst/gl/gstglcontext.h>


class Pipeline;

class GstThread : public QThread
{
    Q_OBJECT

public:
    GstThread(GstGLDisplay *display,
            GstGLContext *context,
            const QString &videoLocation,
            const char *renderer_slot,
            QObject *parent = 0);

    ~GstThread();

    Pipeline *getPipeline() {return this->m_pipeline;}

public Q_SLOTS:
    void stop();

protected:
    void run();

private:
    const QString m_videoLocation;
    Pipeline* m_pipeline;
};

#endif
