/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#include <gdk/gdk.h>
#include <gtk/gtkstyleproviderprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_CASCADE           (_gtk_style_cascade_get_type ())
#define GTK_STYLE_CASCADE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_STYLE_CASCADE, GtkStyleCascade))
#define GTK_STYLE_CASCADE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_STYLE_CASCADE, GtkStyleCascadeClass))
#define GTK_IS_STYLE_CASCADE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_STYLE_CASCADE))
#define GTK_IS_STYLE_CASCADE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_STYLE_CASCADE))
#define GTK_STYLE_CASCADE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE_CASCADE, GtkStyleCascadeClass))

typedef struct _GtkStyleCascade           GtkStyleCascade;
typedef struct _GtkStyleCascadeClass      GtkStyleCascadeClass;

struct _GtkStyleCascade
{
  GObject object;

  GtkStyleCascade *parent;
  GArray *providers;
  int scale;
};

struct _GtkStyleCascadeClass
{
  GObjectClass  parent_class;
};

GType                 _gtk_style_cascade_get_type               (void) G_GNUC_CONST;

GtkStyleCascade *     _gtk_style_cascade_new                    (void);

void                  _gtk_style_cascade_set_parent             (GtkStyleCascade     *cascade,
                                                                 GtkStyleCascade     *parent);
void                  _gtk_style_cascade_set_scale              (GtkStyleCascade     *cascade,
                                                                 int                  scale);
int                   _gtk_style_cascade_get_scale              (GtkStyleCascade     *cascade);

void                  _gtk_style_cascade_add_provider           (GtkStyleCascade     *cascade,
                                                                 GtkStyleProvider    *provider,
                                                                 guint                priority);
void                  _gtk_style_cascade_remove_provider        (GtkStyleCascade     *cascade,
                                                                 GtkStyleProvider    *provider);


G_END_DECLS

