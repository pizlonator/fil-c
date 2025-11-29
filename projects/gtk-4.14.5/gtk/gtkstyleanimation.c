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

#include "gtkstyleanimationprivate.h"

GtkStyleAnimation *
_gtk_style_animation_advance (GtkStyleAnimation *animation,
                              gint64             timestamp)
{
  g_assert (animation != NULL);

  return animation->class->advance (animation, timestamp);
}

void
_gtk_style_animation_apply_values (GtkStyleAnimation    *animation,
                                   GtkCssAnimatedStyle  *style)
{
  animation->class->apply_values (animation, style);
}

gboolean
_gtk_style_animation_is_finished (GtkStyleAnimation *animation)
{
  return animation->class->is_finished (animation);
}


GtkStyleAnimation *
gtk_style_animation_ref (GtkStyleAnimation *animation)
{
  animation->ref_count++;
  return animation;
}

GtkStyleAnimation *
gtk_style_animation_unref (GtkStyleAnimation *animation)
{
  animation->ref_count--;

  if (animation->ref_count == 0)
    {
      animation->class->free (animation);
      return NULL;
    }
  return animation;
}

/**
 * _gtk_style_animation_is_static:
 * @animation: The animation to query
 * @at_time_us: The timestamp to query for
 *
 * Checks if @animation will not change its values anymore after
 * @at_time_us. This happens for example when the animation has reached its
 * final value or when it has been paused.
 *
 * Returns: %TRUE if @animation will not change anymore after @at_time_us
 **/
gboolean
_gtk_style_animation_is_static (GtkStyleAnimation *animation)
{
  return animation->class->is_static (animation);
}
