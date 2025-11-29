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

#include "config.h"

#include "gtkcsstransitionprivate.h"

#include "gtkcsseasevalueprivate.h"
#include "gtkprogresstrackerprivate.h"

struct _GtkCssTransition
{
  GtkStyleAnimation parent;

  guint               property;
  guint               finished;
  GtkCssValue        *start;
  GtkCssValue        *ease;
  GtkProgressTracker  tracker;
};


static GtkStyleAnimation *   gtk_css_transition_advance  (GtkStyleAnimation    *style_animation,
                                                          gint64                timestamp);



static void
gtk_css_transition_apply_values (GtkStyleAnimation   *style_animation,
                                 GtkCssAnimatedStyle *style)
{
  GtkCssTransition *transition = (GtkCssTransition *)style_animation;
  GtkCssValue *value, *end;
  double progress;
  GtkProgressState state;

  if (transition->finished)
    return;

  end = gtk_css_animated_style_get_intrinsic_value (style, transition->property);
  state = gtk_progress_tracker_get_state (&transition->tracker);

  if (state == GTK_PROGRESS_STATE_BEFORE)
    value = _gtk_css_value_ref (transition->start);
  else if (state == GTK_PROGRESS_STATE_DURING)
    {
      progress = gtk_progress_tracker_get_progress (&transition->tracker, FALSE);
      progress = _gtk_css_ease_value_transform (transition->ease, progress);

      value = _gtk_css_value_transition (transition->start,
                                         end,
                                         transition->property,
                                         progress);
    }
  else
    return;

  if (value == NULL)
    value = _gtk_css_value_ref (end);

  gtk_css_animated_style_set_animated_value (style, transition->property, value);
}

static gboolean
gtk_css_transition_is_finished (GtkStyleAnimation *animation)
{
  GtkCssTransition *transition = (GtkCssTransition *)animation;

  return transition->finished;
}

static gboolean
gtk_css_transition_is_static (GtkStyleAnimation *animation)
{
  GtkCssTransition *transition = (GtkCssTransition *)animation;

  return transition->finished;
}

static void
gtk_css_transition_free (GtkStyleAnimation *animation)
{
  GtkCssTransition *self = (GtkCssTransition *)animation;

  gtk_css_value_unref (self->start);
  gtk_css_value_unref (self->ease);
  g_free (self);
}

static const GtkStyleAnimationClass GTK_CSS_TRANSITION_CLASS = {
  "GtkCssTransition",
  gtk_css_transition_free,
  gtk_css_transition_is_finished,
  gtk_css_transition_is_static,
  gtk_css_transition_apply_values,
  gtk_css_transition_advance,
};

static GtkStyleAnimation *
gtk_css_transition_advance (GtkStyleAnimation    *style_animation,
                            gint64                timestamp)
{
  GtkCssTransition *source = (GtkCssTransition *)style_animation;
  GtkCssTransition *transition;

  transition = g_new (GtkCssTransition, 1);
  transition->parent.class = &GTK_CSS_TRANSITION_CLASS;
  transition->parent.ref_count = 1;

  transition->property = source->property;
  transition->start = _gtk_css_value_ref (source->start);
  transition->ease = _gtk_css_value_ref (source->ease);

  gtk_progress_tracker_init_copy (&source->tracker, &transition->tracker);
  gtk_progress_tracker_advance_frame (&transition->tracker, timestamp);
  transition->finished = gtk_progress_tracker_get_state (&transition->tracker) == GTK_PROGRESS_STATE_AFTER;

  return (GtkStyleAnimation *)transition;
}
GtkStyleAnimation *
_gtk_css_transition_new (guint        property,
                         GtkCssValue *start,
                         GtkCssValue *ease,
                         gint64       timestamp,
                         gint64       duration_us,
                         gint64       delay_us)
{
  GtkCssTransition *transition;

  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (ease != NULL, NULL);

  transition = g_new (GtkCssTransition, 1);
  transition->parent.class = &GTK_CSS_TRANSITION_CLASS;
  transition->parent.ref_count = 1;

  transition->property = property;
  transition->start = _gtk_css_value_ref (start);
  transition->ease = _gtk_css_value_ref (ease);
  gtk_progress_tracker_start (&transition->tracker, duration_us, delay_us, 1.0);
  gtk_progress_tracker_advance_frame (&transition->tracker, timestamp);
  transition->finished = gtk_progress_tracker_get_state (&transition->tracker) == GTK_PROGRESS_STATE_AFTER;

  return (GtkStyleAnimation*)transition;
}

guint
_gtk_css_transition_get_property (GtkCssTransition *transition)
{
  return transition->property;
}

gboolean
_gtk_css_transition_is_transition (GtkStyleAnimation  *animation)
{
  return animation->class == &GTK_CSS_TRANSITION_CLASS;
}
