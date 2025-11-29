/*
 * bluroverlay.h
 * This file is part of gtk
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro, Mike Krüger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BLUR_TYPE_OVERLAY             (blur_overlay_get_type ())
#define BLUR_OVERLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BLUR_TYPE_OVERLAY, BlurOverlay))
#define BLUR_OVERLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BLUR_TYPE_OVERLAY, BlurOverlayClass))
#define BLUR_IS_OVERLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BLUR_TYPE_OVERLAY))
#define BLUR_IS_OVERLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BLUR_TYPE_OVERLAY))
#define BLUR_OVERLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BLUR_TYPE_OVERLAY, BlurOverlayClass))

typedef struct _BlurOverlay         BlurOverlay;
typedef struct _BlurOverlayClass    BlurOverlayClass;

struct _BlurOverlay
{
  GtkWidget parent_instance;

  GtkWidget *main_widget;
};

struct _BlurOverlayClass
{
  GtkWidgetClass parent_class;

  gboolean (*get_child_position) (BlurOverlay   *overlay,
                                  GtkWidget     *widget,
                                  GtkAllocation *allocation);
};

GType      blur_overlay_get_type    (void) G_GNUC_CONST;
GtkWidget *blur_overlay_new         (void);
void       blur_overlay_add_overlay (BlurOverlay *overlay,
                                     GtkWidget   *widget,
                                     double       blur);
void       blur_overlay_set_child   (BlurOverlay *overlay,
                                     GtkWidget   *widget);

G_END_DECLS
