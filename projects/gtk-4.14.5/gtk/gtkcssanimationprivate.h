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

#include "gtkcsskeyframesprivate.h"
#include "gtkprogresstrackerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssAnimation           GtkCssAnimation;
typedef struct _GtkCssAnimationClass      GtkCssAnimationClass;

struct _GtkCssAnimation
{
  GtkStyleAnimation parent;

  char            *name;
  GtkCssKeyframes *keyframes;
  GtkCssValue     *ease;
  GtkCssDirection  direction;
  GtkCssPlayState  play_state;
  GtkCssFillMode   fill_mode;
  GtkProgressTracker tracker;
};

struct _GtkCssAnimationClass
{
  GtkStyleAnimationClass parent_class;
};

GType                   _gtk_css_animation_get_type        (void) G_GNUC_CONST;

GtkStyleAnimation *     _gtk_css_animation_new             (const char         *name,
                                                            GtkCssKeyframes    *keyframes,
                                                            gint64              timestamp,
                                                            gint64              delay_us,
                                                            gint64              duration_us,
                                                            GtkCssValue        *ease,
                                                            GtkCssDirection     direction,
                                                            GtkCssPlayState     play_state,
                                                            GtkCssFillMode      fill_mode,
                                                            double              iteration_count);

GtkStyleAnimation *     _gtk_css_animation_advance_with_play_state (GtkCssAnimation   *animation,
                                                                    gint64             timestamp,
                                                                    GtkCssPlayState    play_state);

const char *            _gtk_css_animation_get_name        (GtkCssAnimation   *animation);
gboolean                _gtk_css_animation_is_animation    (GtkStyleAnimation *animation);

G_END_DECLS

