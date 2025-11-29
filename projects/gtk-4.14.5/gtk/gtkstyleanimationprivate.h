/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include "gtkcssanimatedstyleprivate.h"

G_BEGIN_DECLS

typedef struct _GtkStyleAnimation           GtkStyleAnimation;
typedef struct _GtkStyleAnimationClass      GtkStyleAnimationClass;

struct _GtkStyleAnimation
{
  const GtkStyleAnimationClass *class;
  guint ref_count;
};

struct _GtkStyleAnimationClass
{
  const char *type_name;
  void          (* free)                                (GtkStyleAnimation      *animation);
  gboolean      (* is_finished)                         (GtkStyleAnimation      *animation);
  gboolean      (* is_static)                           (GtkStyleAnimation      *animation);
  void          (* apply_values)                        (GtkStyleAnimation      *animation,
                                                         GtkCssAnimatedStyle    *style);
  GtkStyleAnimation *  (* advance)                      (GtkStyleAnimation      *animation,
                                                         gint64                  timestamp);
};

GType           _gtk_style_animation_get_type           (void) G_GNUC_CONST;

GtkStyleAnimation * _gtk_style_animation_advance        (GtkStyleAnimation      *animation,
                                                         gint64                  timestamp);
void            _gtk_style_animation_apply_values       (GtkStyleAnimation      *animation,
                                                         GtkCssAnimatedStyle    *style);
gboolean        _gtk_style_animation_is_finished        (GtkStyleAnimation      *animation);
gboolean        _gtk_style_animation_is_static          (GtkStyleAnimation      *animation);

GtkStyleAnimation * gtk_style_animation_ref             (GtkStyleAnimation      *animation);
GtkStyleAnimation * gtk_style_animation_unref           (GtkStyleAnimation      *animation);


G_END_DECLS

