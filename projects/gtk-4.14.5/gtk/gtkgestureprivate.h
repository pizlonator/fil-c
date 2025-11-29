/* GTK - The GIMP Toolkit
 * Copyright (C) 2014, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */
#pragma once

#include "gtkeventcontrollerprivate.h"
#include "gtkgesture.h"

struct _GtkGesture
{
  GtkEventController parent_instance;
};

struct _GtkGestureClass
{
  GtkEventControllerClass parent_class;

  gboolean (* check)  (GtkGesture       *gesture);

  void     (* begin)  (GtkGesture       *gesture,
                       GdkEventSequence *sequence);
  void     (* update) (GtkGesture       *gesture,
                       GdkEventSequence *sequence);
  void     (* end)    (GtkGesture       *gesture,
                       GdkEventSequence *sequence);

  void     (* cancel) (GtkGesture       *gesture,
                       GdkEventSequence *sequence);

  void     (* sequence_state_changed) (GtkGesture            *gesture,
                                       GdkEventSequence      *sequence,
                                       GtkEventSequenceState  state);

  /*< private >*/
  gpointer padding[10];
};


G_BEGIN_DECLS

gboolean _gtk_gesture_check                  (GtkGesture       *gesture);

gboolean _gtk_gesture_handled_sequence_press (GtkGesture       *gesture,
                                              GdkEventSequence *sequence);

gboolean _gtk_gesture_get_pointer_emulating_sequence
                                                (GtkGesture        *gesture,
                                                 GdkEventSequence **sequence);

gboolean _gtk_gesture_cancel_sequence        (GtkGesture       *gesture,
                                              GdkEventSequence *sequence);

gboolean _gtk_gesture_get_last_update_time   (GtkGesture       *gesture,
                                              GdkEventSequence *sequence,
                                              guint32          *evtime);

GtkWidget  *gtk_gesture_get_last_target      (GtkGesture       *gesture,
                                              GdkEventSequence *sequence);


G_END_DECLS

