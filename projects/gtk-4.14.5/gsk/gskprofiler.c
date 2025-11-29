#include "config.h"

#include "gskprofilerprivate.h"

#define MAX_SAMPLES     32

typedef struct {
  GQuark id;
  char *description;
  gint64 value;
  gint64 n_samples;
  unsigned int can_reset : 1;
} NamedCounter;

typedef struct {
  GQuark id;
  char *description;
  gint64 value;
  gint64 start_time;
  gint64 min_value;
  gint64 max_value;
  gint64 avg_value;
  gint64 n_samples;
  unsigned int in_flight : 1;
  unsigned int can_reset : 1;
  unsigned int invert : 1;
} NamedTimer;

typedef struct {
  GQuark id;
  gint64 value;
} Sample;

struct _GskProfiler
{
  GObject parent_instance;

  GHashTable *counters;
  GHashTable *timers;

  Sample timer_samples[MAX_SAMPLES];
  guint last_sample;
};

G_DEFINE_TYPE (GskProfiler, gsk_profiler, G_TYPE_OBJECT)

static void
named_counter_free (gpointer data)
{
  NamedCounter *counter = data;

  if (data == NULL)
    return;

  g_free (counter->description);
  g_free (counter);
}

static void
named_timer_free (gpointer data)
{
  NamedTimer *timer = data;

  if (data == NULL)
    return;

  g_free (timer->description);
  g_free (timer);
}

static void
gsk_profiler_finalize (GObject *gobject)
{
  GskProfiler *self = GSK_PROFILER (gobject);

  g_clear_pointer (&self->counters, g_hash_table_unref);
  g_clear_pointer (&self->timers, g_hash_table_unref);

  G_OBJECT_CLASS (gsk_profiler_parent_class)->finalize (gobject);
}

static void
gsk_profiler_class_init (GskProfilerClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_profiler_finalize;
}

static void
gsk_profiler_init (GskProfiler *self)
{
  self->counters = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                          NULL,
                                          named_counter_free);
  self->timers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL,
                                        named_timer_free);
}

GskProfiler *
gsk_profiler_new (void)
{
  return g_object_new (GSK_TYPE_PROFILER, NULL);
}

static NamedCounter *
named_counter_new (GQuark      id,
                   const char *description,
                   gboolean    can_reset)
{
  NamedCounter *res = g_new0 (NamedCounter, 1);

  res->id = id;
  res->description = g_strdup (description);
  res->can_reset = can_reset;

  return res;
}

static NamedCounter *
gsk_profiler_get_counter (GskProfiler *profiler,
                          GQuark       id)
{
  return g_hash_table_lookup (profiler->counters, GINT_TO_POINTER (id));
}

GQuark
gsk_profiler_add_counter (GskProfiler *profiler,
                          const char  *counter_name,
                          const char  *description,
                          gboolean     can_reset)
{
  NamedCounter *counter;
  GQuark id;

  g_return_val_if_fail (GSK_IS_PROFILER (profiler), 0);

  id = g_quark_from_string (counter_name);
  counter = gsk_profiler_get_counter (profiler, id);
  if (counter != NULL)
    {
      g_critical ("Cannot add a counter '%s' as one already exists.", counter_name);
      return counter->id;
    }

  counter = named_counter_new (id, description, can_reset);
  g_hash_table_insert (profiler->counters, GINT_TO_POINTER (id), counter);

  return counter->id;
}

static NamedTimer *
named_timer_new (GQuark      id,
                 const char *description,
                 gboolean    invert,
                 gboolean    can_reset)
{
  NamedTimer *res = g_new0 (NamedTimer, 1);

  res->id = id;
  res->description = g_strdup (description);
  res->invert = invert;
  res->can_reset = can_reset;

  return res;
}

static NamedTimer *
gsk_profiler_get_timer (GskProfiler *profiler,
                        GQuark       id)
{
  return g_hash_table_lookup (profiler->timers, GINT_TO_POINTER (id));
}

GQuark
gsk_profiler_add_timer (GskProfiler *profiler,
                        const char  *timer_name,
                        const char  *description,
                        gboolean     invert,
                        gboolean     can_reset)
{
  NamedTimer *timer;
  GQuark id;

  g_return_val_if_fail (GSK_IS_PROFILER (profiler), 0);

  id = g_quark_from_string (timer_name);
  timer = gsk_profiler_get_timer (profiler, id);
  if (timer != NULL)
    {
      g_critical ("Cannot add a timer '%s' as one already exists.", timer_name);
      return timer->id;
    }

  timer = named_timer_new (id, description, invert, can_reset);
  g_hash_table_insert (profiler->timers, GINT_TO_POINTER (id), timer);

  return timer->id;
}

void
gsk_profiler_counter_inc (GskProfiler *profiler,
                          GQuark       counter_id)
{
  gsk_profiler_counter_add (profiler, counter_id, 1);
}

void
gsk_profiler_counter_set (GskProfiler *profiler,
                          GQuark       counter_id,
                          gint64       value)
{
  NamedCounter *counter;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  counter = gsk_profiler_get_counter (profiler, counter_id);
  if (counter == NULL)
    {
      g_critical ("No counter '%s' (id:%d) found; did you forget to call gsk_profiler_add_counter()?",
                  g_quark_to_string (counter_id), counter_id);
      return;
    }

  counter->value = value;
}

void
gsk_profiler_counter_add (GskProfiler *profiler,
                          GQuark       counter_id,
                          gint64       increment)
{
  NamedCounter *counter;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  counter = gsk_profiler_get_counter (profiler, counter_id);
  if (counter == NULL)
    {
      g_critical ("No counter '%s' (id:%d) found; did you forget to call gsk_profiler_add_counter()?",
                  g_quark_to_string (counter_id), counter_id);
      return;
    }

  counter->value += increment;
}

void
gsk_profiler_timer_begin (GskProfiler *profiler,
                          GQuark       timer_id)
{
  NamedTimer *timer;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  timer = gsk_profiler_get_timer (profiler, timer_id);
  if (timer == NULL)
    return;

  if (timer->in_flight)
    return;

  timer->in_flight = TRUE;
  timer->start_time = g_get_monotonic_time ();
}

gint64
gsk_profiler_timer_end (GskProfiler *profiler,
                        GQuark       timer_id)
{
  NamedTimer *timer;
  gint64 diff;

  g_return_val_if_fail (GSK_IS_PROFILER (profiler), 0);

  timer = gsk_profiler_get_timer (profiler, timer_id);
  if (timer == NULL)
    {
      g_critical ("No timer '%s' (id:%d) found; did you forget to call gsk_profiler_add_timer()?",
                  g_quark_to_string (timer_id), timer_id);
      return 0;
    }

  if (!timer->in_flight)
    {
      g_critical ("Timer '%s' (id:%d) is not running; did you forget to call gsk_profiler_timer_begin()?",
                  g_quark_to_string (timer->id), timer->id);
      return 0;
    }

  diff = g_get_monotonic_time () - timer->start_time;

  timer->in_flight = FALSE;
  timer->value += diff; 

  return diff;
}

void
gsk_profiler_timer_set (GskProfiler *profiler,
                        GQuark       timer_id,
                        gint64       value)
{
  NamedTimer *timer;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  timer = gsk_profiler_get_timer (profiler, timer_id);
  if (timer == NULL)
    {
      g_critical ("No timer '%s' (id:%d) found; did you forget to call gsk_profiler_add_timer()?",
                  g_quark_to_string (timer_id), timer_id);
      return;
    }

  if (timer->in_flight)
    {
      g_critical ("Timer '%s' (id:%d) is running; are you sure you don't want to call "
                  "gsk_profiler_timer_end() instead of gsk_profiler_timer_set()?",
                  g_quark_to_string (timer_id), timer_id);
    }

  timer->value = value;
}

gint64
gsk_profiler_counter_get (GskProfiler *profiler,
                          GQuark       counter_id)
{
  NamedCounter *counter;

  g_return_val_if_fail (GSK_IS_PROFILER (profiler), 0);

  counter = gsk_profiler_get_counter (profiler, counter_id);
  if (counter == NULL)
    {
      g_critical ("No counter '%s' (id:%d) found; did you forget to call gsk_profiler_add_counter()?",
                  g_quark_to_string (counter_id), counter_id);
      return 0;
    }

  return counter->value;
}

gint64
gsk_profiler_timer_get (GskProfiler *profiler,
                        GQuark       timer_id)
{
  NamedTimer *timer;

  g_return_val_if_fail (GSK_IS_PROFILER (profiler), 0);

  timer = gsk_profiler_get_timer (profiler, timer_id);
  if (timer == NULL)
    {
      g_critical ("No timer '%s' (id:%d) found; did you forget to call gsk_profiler_add_timer()?",
                  g_quark_to_string (timer_id), timer_id);
      return 0;
    }

  if (timer->invert)
    return (gint64) (1000000.0 / (double) timer->value);

  return timer->value;
}

gint64
gsk_profiler_timer_get_start (GskProfiler *profiler,
                              GQuark       timer_id)
{
  NamedTimer *timer;

  timer = gsk_profiler_get_timer (profiler, timer_id);
  if (timer == NULL)
    return 0;

  return timer->start_time;
}

void
gsk_profiler_reset (GskProfiler *profiler)
{
  GHashTableIter iter;
  gpointer value_p = NULL;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  g_hash_table_iter_init (&iter, profiler->counters);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedCounter *counter = value_p;

      if (counter->can_reset)
        counter->value = 0;
    }

  g_hash_table_iter_init (&iter, profiler->timers);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedTimer *timer = value_p;

      if (timer->can_reset)
        {
          timer->value = 0;
          timer->min_value = 0;
          timer->max_value = 0;
          timer->avg_value = 0;
          timer->n_samples = 0;
        }
    }

  profiler->last_sample = 0;
}

void
gsk_profiler_push_samples (GskProfiler *profiler)
{
  GHashTableIter iter;
  gpointer value_p = NULL;
  guint last_sample;

  g_return_if_fail (GSK_IS_PROFILER (profiler));

  g_hash_table_iter_init (&iter, profiler->timers);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedTimer *timer = value_p;
      Sample *s;

      last_sample = profiler->last_sample;
      profiler->last_sample += 1;
      if (profiler->last_sample == MAX_SAMPLES)
        profiler->last_sample = 0;

      s = &(profiler->timer_samples[last_sample]);
      s->id = timer->id;

      if (timer->invert)
        s->value = (gint64) (1000000.0 / (double) timer->value);
      else
        s->value = timer->value;
    }
}

void
gsk_profiler_append_counters (GskProfiler *profiler,
                              GString     *buffer)
{
  GHashTableIter iter;
  gpointer value_p = NULL;

  g_return_if_fail (GSK_IS_PROFILER (profiler));
  g_return_if_fail (buffer != NULL);

  g_hash_table_iter_init (&iter, profiler->counters);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedCounter *counter = value_p;

      g_string_append_printf (buffer, "%s: %" G_GINT64_FORMAT "\n",
                              counter->description,
                              counter->value);
    }
}

void
gsk_profiler_append_timers (GskProfiler *profiler,
                            GString     *buffer)
{
  GHashTableIter iter;
  gpointer value_p = NULL;
  int i;

  g_return_if_fail (GSK_IS_PROFILER (profiler));
  g_return_if_fail (buffer != NULL);

  g_hash_table_iter_init (&iter, profiler->timers);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedTimer *timer = value_p;

      timer->min_value = G_MAXINT64;
      timer->max_value = G_MININT64;
      timer->avg_value = 0;
      timer->n_samples = 0;
    }

  for (i = 0; i < profiler->last_sample; i++)
    {
      Sample *s = &(profiler->timer_samples[i]);
      NamedTimer *timer;

      if (s->id == 0)
        continue;

      timer = gsk_profiler_get_timer (profiler, s->id);
      timer->min_value = MIN (timer->min_value, s->value);
      timer->max_value = MAX (timer->max_value, s->value);
      timer->avg_value += s->value;
      timer->n_samples += 1;
    }

  g_hash_table_iter_init (&iter, profiler->timers);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      NamedTimer *timer = value_p;
      const char *unit = timer->invert ? "" : "usec";

      g_string_append_printf (buffer, "%s (%s): %.2f",
                              timer->description,
                              unit,
                              (double) timer->value);

      if (timer->n_samples > 1)
        {
          timer->avg_value = timer->avg_value / timer->n_samples;
          g_string_append_printf (buffer, " Min: %.2f Avg: %.2f Max: %.2f (%" G_GINT64_FORMAT " samples)",
                                  (double) timer->min_value,
                                  (double) timer->avg_value,
                                  (double) timer->max_value,
                                  timer->n_samples);
        }

      g_string_append (buffer, "\n");
    }
}
