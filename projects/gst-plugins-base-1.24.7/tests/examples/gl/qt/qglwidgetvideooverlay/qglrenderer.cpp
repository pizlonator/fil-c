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

#include "qglrenderer.h"

QGLRenderer::QGLRenderer(const QString videoLocation, QWidget *parent)
    : QGLWidget(parent),
    m_gt(winId(), videoLocation)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setVisible(false);
    move(20, 10);
    resize(640, 480);

    QObject::connect(&m_gt, SIGNAL(finished()), this, SLOT(close()));
    QObject::connect(this, SIGNAL(exposeRequested()), &m_gt, SLOT(exposeRequested()));
    QObject::connect(this, SIGNAL(closeRequested()), &m_gt, SLOT(stop()), Qt::DirectConnection);
    QObject::connect(&m_gt, SIGNAL(showRequested()), this, SLOT(show()));
    m_gt.start();
}

QGLRenderer::~QGLRenderer()
{
}

void QGLRenderer::paintEvent(QPaintEvent* event)
{
    emit exposeRequested();
}

void QGLRenderer::closeEvent(QCloseEvent* event)
{
    emit closeRequested();
    m_gt.wait();
}
