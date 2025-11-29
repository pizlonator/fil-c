/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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

/**
 * GtkEventControllerLegacy:
 *
 * `GtkEventControllerLegacy` is an event controller that provides raw
 * access to the event stream.
 *
 * It should only be used as a last resort if none of the other event
 * controllers or gestures do the job.
 */

#include "config.h"

#include "gtkeventcontrollerlegacy.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkmarshalers.h"
#include "gdk/gdkmarshalers.h"
#include "gtkprivate.h"

struct _GtkEventControllerLegacy
{
  GtkEventController parent_instance;
};

struct _GtkEventControllerLegacyClass
{
  GtkEventControllerClass parent_class;
};

enum {
  EVENT,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (GtkEventControllerLegacy, gtk_event_controller_legacy,
               GTK_TYPE_EVENT_CONTROLLER)

static gboolean
gtk_event_controller_legacy_handle_event (GtkEventController *controller,
                                          GdkEvent           *event,
                                          double              x,
                                          double              y)
{
  gboolean handled;

  g_signal_emit (controller, signals[EVENT], 0, event, &handled);

  return handled;
}

static void
gtk_event_controller_legacy_class_init (GtkEventControllerLegacyClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  controller_class->handle_event = gtk_event_controller_legacy_handle_event;

  /**
   * GtkEventControllerLegacy::event:
   * @controller: the object which received the signal
   * @event: the `GdkEvent` which triggered this signal
   *
   * Emitted for each GDK event delivered to @controller.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event
   *   and the emission of this signal. %FALSE to propagate the event further.
   */
  signals[EVENT] =
    g_signal_new (I_("event"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, _gtk_boolean_handled_accumulator, NULL,
                  _gdk_marshal_BOOLEAN__POINTER,
                  G_TYPE_BOOLEAN, 1,
                  GDK_TYPE_EVENT);

  g_signal_set_va_marshaller (signals[EVENT], G_TYPE_FROM_CLASS (klass),
                              _gdk_marshal_BOOLEAN__POINTERv);
}

static void
gtk_event_controller_legacy_init (GtkEventControllerLegacy *controller)
{
}

/**
 * gtk_event_controller_legacy_new:
 *
 * Creates a new legacy event controller.
 *
 * Returns: the newly created event controller.
 */
GtkEventController *
gtk_event_controller_legacy_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_LEGACY,
                       NULL);
}
