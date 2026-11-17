/*
 * Copyright © 2024 Christian Hergert
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "scheduler.h"

/* The scheduler API drives updates using GdkFrameClock when possible
 * and runs at 10hz when not.
 *
 * The GdkFrameClock may not advance in certain situations and that is
 * largely display/compositor specific. On some systems, when minimizing a
 * window to a taskbar we may not get updates. Additionally, when moving a
 * window to another workspace, some display systems may not advance the
 * GdkFrameClock.
 */

#define NEXT_UPDATE_USEC (G_USEC_PER_SEC/10)

typedef struct _Scheduled
{
        GList                 link;
        GtkWidget            *widget;
        VteSchedulerCallback  callback;
        gpointer              user_data;
        guint                 handler;
        gint64                ready_time;
} Scheduled;

static GQueue scheduled = G_QUEUE_INIT;
static GSource *scheduled_source;

static void
unarm_fallback_scheduler (void)
{
        if (scheduled_source != nullptr) {
                g_source_destroy (scheduled_source);
                g_source_unref (scheduled_source);
                scheduled_source = nullptr;
        }
}

static gboolean
fallback_scheduler_dispatch (GSource     *gsource,
                             GSourceFunc  callback,
                             gpointer     user_data)
{
        const GList *iter = scheduled.head;
        gint64 now = g_source_get_time (gsource);
        gint64 next = now + NEXT_UPDATE_USEC;

        if (now < g_source_get_ready_time (gsource)) {
                return G_SOURCE_CONTINUE;
        }

        while (iter != nullptr) {
                Scheduled *state = (Scheduled *)iter->data;

                iter = iter->next;

                if (state->ready_time <= now) {
                        state->ready_time = next;
                        state->callback (state->widget, state->user_data);
                } else if (state->ready_time < next) {
                        next = state->ready_time;
                }
        }

        g_source_set_ready_time (gsource, next);

        if (scheduled.length == 0) {
                unarm_fallback_scheduler ();
        }

        return G_SOURCE_CONTINUE;
}

static GSourceFuncs scheduled_source_funcs = {
        .dispatch = fallback_scheduler_dispatch,
};

static void
arm_fallback_scheduler (void)
{
        GSource *gsource;

        gsource = g_source_new (&scheduled_source_funcs, sizeof (GSource));
        g_source_set_static_name (gsource, "[vte-scheduler]");
        g_source_set_ready_time (gsource, g_get_monotonic_time () + NEXT_UPDATE_USEC);
        g_source_set_priority (gsource, G_PRIORITY_LOW);
        g_source_attach (gsource, nullptr);

        scheduled_source = gsource;
}

static gboolean
scheduler_tick_callback (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
        Scheduled *state = (Scheduled *)user_data;

        state->ready_time = g_get_monotonic_time () + NEXT_UPDATE_USEC;
        state->callback (widget, state->user_data);

        return G_SOURCE_CONTINUE;
}

gpointer
_vte_scheduler_add_callback (GtkWidget            *widget,
                             VteSchedulerCallback  callback,
                             gpointer              user_data)
{
        Scheduled *state = g_new0 (Scheduled, 1);

        state->link.data = state;
        state->ready_time = g_get_monotonic_time () + NEXT_UPDATE_USEC;
        state->callback = callback;
        state->user_data = user_data;
        state->widget = widget;
        state->handler = gtk_widget_add_tick_callback (widget, scheduler_tick_callback, state, nullptr);

        g_queue_push_tail_link (&scheduled, &state->link);

        if (scheduled_source == nullptr)
                arm_fallback_scheduler ();

        return (gpointer)state;
}

void
_vte_scheduler_remove_callback (GtkWidget *widget,
                                gpointer   handler)
{
        Scheduled *state = (Scheduled *)handler;

        g_queue_unlink (&scheduled, &state->link);
        gtk_widget_remove_tick_callback (widget, state->handler);
        g_free (state);

        if (scheduled.length == 0) {
                unarm_fallback_scheduler ();
        }
}
