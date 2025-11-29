#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GSK_TYPE_PROFILER (gsk_profiler_get_type ())
G_DECLARE_FINAL_TYPE (GskProfiler, gsk_profiler, GSK, PROFILER, GObject)

GskProfiler *   gsk_profiler_new                (void);

GQuark          gsk_profiler_add_counter        (GskProfiler *profiler,
                                                 const char  *counter_name,
                                                 const char  *description,
                                                 gboolean     can_reset);
GQuark          gsk_profiler_add_timer          (GskProfiler *profiler,
                                                 const char  *timer_name,
                                                 const char  *description,
                                                 gboolean     invert,
                                                 gboolean     can_reset);

void            gsk_profiler_counter_inc        (GskProfiler *profiler,
                                                 GQuark       counter_id);
void            gsk_profiler_counter_add        (GskProfiler *profiler,
                                                 GQuark       counter_id,
                                                 gint64       increment);
void            gsk_profiler_counter_set        (GskProfiler *profiler,
                                                 GQuark       counter_id,
                                                 gint64       value);
void            gsk_profiler_timer_begin        (GskProfiler *profiler,
                                                 GQuark       timer_id);
gint64          gsk_profiler_timer_end          (GskProfiler *profiler,
                                                 GQuark       timer_id);
void            gsk_profiler_timer_set          (GskProfiler *profiler,
                                                 GQuark       timer_id,
                                                 gint64       value);

gint64          gsk_profiler_counter_get        (GskProfiler *profiler,
                                                 GQuark       counter_id);
gint64          gsk_profiler_timer_get          (GskProfiler *profiler,
                                                 GQuark       timer_id);
gint64          gsk_profiler_timer_get_start    (GskProfiler *profiler,
                                                 GQuark       timer_id);

void            gsk_profiler_reset              (GskProfiler *profiler);

void            gsk_profiler_push_samples       (GskProfiler *profiler);
void            gsk_profiler_append_counters    (GskProfiler *profiler,
                                                 GString     *buffer);
void            gsk_profiler_append_timers      (GskProfiler *profiler,
                                                 GString     *buffer);

G_END_DECLS

