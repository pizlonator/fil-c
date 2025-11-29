/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Lieven van der Heide
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

#include "config.h"
#include "gtkkineticscrollingprivate.h"

#include <math.h>
#include <stdio.h>

/*
 * All our curves are second degree linear differential equations, and
 * so they can always be written as linear combinations of 2 base
 * solutions. c1 and c2 are the coefficients to these two base solutions,
 * and are computed from the initial position and velocity.
 *
 * In the case of simple deceleration, the differential equation is
 *
 *   y'' = -my'
 *
 * With m the resistance factor. For this we use the following 2
 * base solutions:
 *
 *   f1(x) = 1
 *   f2(x) = exp(-mx)
 *
 * In the case of overshoot, the differential equation is
 *
 *   y'' = -my' - ky
 *
 * With m the resistance, and k the spring stiffness constant. We let
 * k = m^2 / 4, so that the system is critically damped (ie, returns to its
 * equilibrium position as quickly as possible, without oscillating), and offset
 * the whole thing, such that the equilibrium position is at 0. This gives the
 * base solutions
 *
 *   f1(x) = exp(-mx / 2)
 *   f2(x) = t exp(-mx / 2)
*/

typedef enum {
  GTK_KINETIC_SCROLLING_PHASE_DECELERATING,
  GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING,
  GTK_KINETIC_SCROLLING_PHASE_FINISHED,
} GtkKineticScrollingPhase;

struct _GtkKineticScrolling
{
  GtkKineticScrollingPhase phase;
  double lower;
  double upper;
  double overshoot_width;
  double decel_friction;
  double overshoot_friction;

  double c1;
  double c2;
  double equilibrium_position;

  double t;
  double position;
  double velocity;
};

static void gtk_kinetic_scrolling_init_overshoot (GtkKineticScrolling *data,
                                                  double               equilibrium_position,
                                                  double               initial_position,
                                                  double               initial_velocity);

GtkKineticScrolling *
gtk_kinetic_scrolling_new (double lower,
                           double upper,
                           double overshoot_width,
                           double decel_friction,
                           double overshoot_friction,
                           double initial_position,
                           double initial_velocity)
{
  GtkKineticScrolling *data;

  data = g_new0 (GtkKineticScrolling, 1);
  data->lower = lower;
  data->upper = upper;
  data->decel_friction = decel_friction;
  data->overshoot_friction = overshoot_friction;
  if(initial_position < lower)
    {
      gtk_kinetic_scrolling_init_overshoot (data,
                                            lower,
                                            initial_position,
                                            initial_velocity);
    }
  else if(initial_position > upper)
    {
      gtk_kinetic_scrolling_init_overshoot (data,
                                            upper,
                                            initial_position,
                                            initial_velocity);
    }
  else
    {
      data->phase = GTK_KINETIC_SCROLLING_PHASE_DECELERATING;
      data->c1 = initial_velocity / decel_friction + initial_position;
      data->c2 = -initial_velocity / decel_friction;
      data->t = 0;
      data->position = initial_position;
      data->velocity = initial_velocity;
    }

  return data;
}

GtkKineticScrollingChange
gtk_kinetic_scrolling_update_size (GtkKineticScrolling *data,
                                   double               lower,
                                   double               upper)
{
  GtkKineticScrollingChange change = GTK_KINETIC_SCROLLING_CHANGE_NONE;

  if (lower != data->lower)
    {
      if (data->position <= lower)
        change |= GTK_KINETIC_SCROLLING_CHANGE_LOWER;

      data->lower = lower;
    }

  if (upper != data->upper)
    {
      if (data->position >= data->upper)
        change |= GTK_KINETIC_SCROLLING_CHANGE_UPPER;

      data->upper = upper;
    }

  if (data->phase == GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING)
    change |= GTK_KINETIC_SCROLLING_CHANGE_IN_OVERSHOOT;

  return change;
}

void
gtk_kinetic_scrolling_free (GtkKineticScrolling *kinetic)
{
  g_free (kinetic);
}

static void
gtk_kinetic_scrolling_init_overshoot (GtkKineticScrolling *data,
                                      double               equilibrium_position,
                                      double               initial_position,
                                      double               initial_velocity)
{
  data->phase = GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING;
  data->equilibrium_position = equilibrium_position;
  data->c1 = initial_position - equilibrium_position;
  data->c2 = initial_velocity + data->overshoot_friction / 2 * data->c1;
  data->t = 0;
}

gboolean
gtk_kinetic_scrolling_tick (GtkKineticScrolling *data,
                            double               time_delta,
                            double              *position,
                            double              *velocity)
{
  switch(data->phase)
    {
    case GTK_KINETIC_SCROLLING_PHASE_DECELERATING:
      {
        double exp_part;

        data->t += time_delta;

        exp_part = exp (-data->decel_friction * data->t);
        data->position = data->c1 + data->c2 * exp_part;
        data->velocity = -data->decel_friction * data->c2 * exp_part;

        if(data->position < data->lower)
          {
            gtk_kinetic_scrolling_init_overshoot(data,data->lower,data->position,data->velocity);
          }
        else if (data->position > data->upper)
          {
            gtk_kinetic_scrolling_init_overshoot(data, data->upper, data->position, data->velocity);
          }
        else if (fabs(data->velocity) < 0.1)
          {
            gtk_kinetic_scrolling_stop (data);
          }
        break;
      }

    case GTK_KINETIC_SCROLLING_PHASE_OVERSHOOTING:
      {
        double exp_part, pos;

        data->t += time_delta;
        exp_part = exp(-data->overshoot_friction / 2 * data->t);
        pos = exp_part * (data->c1 + data->c2 * data->t);

        if (pos < data->lower - 50 || pos > data->upper + 50)
          {
            pos = CLAMP (pos, data->lower - 50, data->upper + 50);
            gtk_kinetic_scrolling_init_overshoot (data, data->equilibrium_position, pos, 0);
          }
        else
          data->velocity = data->c2 * exp_part - data->overshoot_friction / 2 * pos;

        data->position = pos + data->equilibrium_position;

        if(fabs (pos) < 0.1)
          {
            data->phase = GTK_KINETIC_SCROLLING_PHASE_FINISHED;
            data->position = data->equilibrium_position;
            data->velocity = 0;
          }
        break;
      }

    case GTK_KINETIC_SCROLLING_PHASE_FINISHED:
    default:
      break;
    }

  if (position)
    *position = data->position;
  if (velocity)
    *velocity = data->velocity;

  return data->phase != GTK_KINETIC_SCROLLING_PHASE_FINISHED;
}

void
gtk_kinetic_scrolling_stop (GtkKineticScrolling *data)
{
  if (data->phase == GTK_KINETIC_SCROLLING_PHASE_DECELERATING)
    {
      data->phase = GTK_KINETIC_SCROLLING_PHASE_FINISHED;
      data->position = round (data->position);
    }
}
