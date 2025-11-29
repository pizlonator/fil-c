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

#include "gtkstyleanimationprivate.h"
#include "gtkprogresstrackerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssTransitionClass      GtkCssTransitionClass;
typedef struct _GtkCssTransition           GtkCssTransition;

struct _GtkCssTransitionClass
{
  GtkStyleAnimationClass parent_class;
};

GType                   _gtk_css_transition_get_type        (void) G_GNUC_CONST;

GtkStyleAnimation *     _gtk_css_transition_new             (guint               property,
                                                             GtkCssValue        *start,
                                                             GtkCssValue        *ease,
                                                             gint64              timestamp,
                                                             gint64              duration_us,
                                                             gint64              delay_us);

guint                   _gtk_css_transition_get_property    (GtkCssTransition   *transition);
gboolean                _gtk_css_transition_is_transition   (GtkStyleAnimation  *animation);

G_END_DECLS

