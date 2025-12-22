/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
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
/*
 * This is a demo application to test the camerabin element.
 * If you have question don't hesitate in contact me edgard.lima@gmail.com
 */

#ifndef __GST_CAMERA_BIN_H__
#define __GST_CAMERA_BIN_H__

#include <gtk/gtk.h>

void
on_mainWindow_delete_event (GtkWidget * widget, GdkEvent * event, gpointer data);

void
on_captureButton_clicked (GtkButton * button, gpointer user_data);

void
on_stopCaptureButton_clicked (GtkButton * button, gpointer user_data);

void
on_imageRButton_toggled (GtkToggleButton * button, gpointer user_data);

void
on_videoRButton_toggled (GtkToggleButton * button, gpointer user_data);

void
on_viewfinderArea_realize (GtkWidget * widget, gpointer data);

void
on_formatComboBox_changed (GtkWidget * widget, gpointer data);

void
on_zoomScale_value_changed (GtkWidget * widget, gpointer data);

#endif /* __GST_CAMERA_BIN_H__ */
