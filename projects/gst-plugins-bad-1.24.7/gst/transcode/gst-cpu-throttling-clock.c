
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETRUSAGE
#include "gst-cpu-throttling-clock.h"

#include <unistd.h>
#include <sys/resource.h>

#include "gst-cpu-throttling-clock.h"

/**
 * SECTION: gst-cpu-throttling-clock
 * @title: GstCpuThrottlingClock
 * @short_description: TODO
 *
 * TODO
 */

/* *INDENT-OFF* */
GST_DEBUG_CATEGORY_STATIC (gst_cpu_throttling_clock_debug);
#define GST_CAT_DEFAULT gst_cpu_throttling_clock_debug

struct _GstCpuThrottlingClockPrivate
{
  guint wanted_cpu_usage;

  GstClock *sclock;
  GstClockTime current_wait_time;
  GstPoll *timer;
  struct rusage last_usage;

  GstClockID evaluate_wait_time;
  GstClockTime time_between_evals;
};

#define parent_class gst_cpu_throttling_clock_parent_class
G_DEFINE_TYPE_WITH_CODE (GstCpuThrottlingClock, gst_cpu_throttling_clock, GST_TYPE_CLOCK, G_ADD_PRIVATE(GstCpuThrottlingClock))

enum
{
  PROP_FIRST,
  PROP_CPU_USAGE,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
/* *INDENT-ON* */

static void
gst_cpu_throttling_clock_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GstCpuThrottlingClock *self = GST_CPU_THROTTLING_CLOCK (object);

  switch (property_id) {
    case PROP_CPU_USAGE:
      g_value_set_uint (value, self->priv->wanted_cpu_usage);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_cpu_throttling_clock_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  GstCpuThrottlingClock *self = GST_CPU_THROTTLING_CLOCK (object);

  switch (property_id) {
    case PROP_CPU_USAGE:
      self->priv->wanted_cpu_usage = g_value_get_uint (value);
      if (self->priv->wanted_cpu_usage == 0)
        self->priv->wanted_cpu_usage = 100;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
gst_transcoder_adjust_wait_time (GstClock * sync_clock, GstClockTime time,
    GstClockID id, GstCpuThrottlingClock * self)
{
  struct rusage ru;
  float delta_usage, usage, coef;

  GstCpuThrottlingClockPrivate *priv = self->priv;

  getrusage (RUSAGE_SELF, &ru);
  delta_usage = GST_TIMEVAL_TO_TIME (ru.ru_utime) -
      GST_TIMEVAL_TO_TIME (self->priv->last_usage.ru_utime);
  usage =
      ((float) delta_usage / self->priv->time_between_evals * 100) /
      g_get_num_processors ();

  self->priv->last_usage = ru;

  coef = GST_MSECOND / 10;
  if (usage < (gfloat) priv->wanted_cpu_usage) {
    coef = -coef;
  }

  priv->current_wait_time = CLAMP (0,
      (GstClockTime) priv->current_wait_time + coef, GST_SECOND);

  GST_DEBUG_OBJECT (self,
      "Avg is %f (wanted %d) => %" GST_TIME_FORMAT, usage,
      self->priv->wanted_cpu_usage, GST_TIME_ARGS (priv->current_wait_time));

  return TRUE;
}

static GstClockReturn
_wait (GstClock * clock, GstClockEntry * entry, GstClockTimeDiff * jitter)
{
  GstCpuThrottlingClock *self = GST_CPU_THROTTLING_CLOCK (clock);

  if (!self->priv->evaluate_wait_time) {
    if (!(self->priv->sclock)) {
      GST_ERROR_OBJECT (clock, "Could not find any system clock"
          " to start the wait time evaluation task");
    } else {
      self->priv->evaluate_wait_time =
          gst_clock_new_periodic_id (self->priv->sclock,
          gst_clock_get_time (self->priv->sclock),
          self->priv->time_between_evals);

      gst_clock_id_wait_async (self->priv->evaluate_wait_time,
          (GstClockCallback) gst_transcoder_adjust_wait_time,
          (gpointer) self, NULL);
    }
  }

  if (G_UNLIKELY (GST_CLOCK_ENTRY_STATUS (entry) == GST_CLOCK_UNSCHEDULED))
    return GST_CLOCK_UNSCHEDULED;

  if (gst_poll_wait (self->priv->timer, self->priv->current_wait_time)) {
    GST_INFO_OBJECT (self, "Something happened on the poll");
  }

  return GST_CLOCK_ENTRY_STATUS (entry);
}

static GstClockTime
_get_internal_time (GstClock * clock)
{
  GstCpuThrottlingClock *self = GST_CPU_THROTTLING_CLOCK (clock);

  return gst_clock_get_internal_time (self->priv->sclock);
}

static void
gst_cpu_throttling_clock_dispose (GObject * object)
{
  GstCpuThrottlingClock *self = GST_CPU_THROTTLING_CLOCK (object);

  if (self->priv->evaluate_wait_time) {
    gst_clock_id_unschedule (self->priv->evaluate_wait_time);
    gst_clock_id_unref (self->priv->evaluate_wait_time);
    self->priv->evaluate_wait_time = 0;
  }
  if (self->priv->timer) {
    gst_poll_free (self->priv->timer);
    self->priv->timer = NULL;
  }
}

static void
gst_cpu_throttling_clock_class_init (GstCpuThrottlingClockClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstClockClass *clock_klass = GST_CLOCK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_cpu_throttling_clock_debug, "cpuclock", 0,
      "UriTranscodebin element");

  oclass->get_property = gst_cpu_throttling_clock_get_property;
  oclass->set_property = gst_cpu_throttling_clock_set_property;
  oclass->dispose = gst_cpu_throttling_clock_dispose;

  /**
   * GstCpuThrottlingClock:cpu-usage:
   *
   * Since: UNRELEASED
   */
  param_specs[PROP_CPU_USAGE] = g_param_spec_uint ("cpu-usage", "cpu-usage",
      "The percentage of CPU to try to use with the processus running the "
      "pipeline driven by the clock", 0, 100,
      100, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, PROP_LAST, param_specs);

  clock_klass->wait = GST_DEBUG_FUNCPTR (_wait);
  clock_klass->get_internal_time = _get_internal_time;
}

static void
gst_cpu_throttling_clock_init (GstCpuThrottlingClock * self)
{
  self->priv = gst_cpu_throttling_clock_get_instance_private (self);

  self->priv->current_wait_time = GST_MSECOND;
  self->priv->wanted_cpu_usage = 100;
  self->priv->timer = gst_poll_new_timer ();
  self->priv->time_between_evals = GST_SECOND / 4;
  self->priv->sclock = GST_CLOCK (gst_system_clock_obtain ());


  getrusage (RUSAGE_SELF, &self->priv->last_usage);
}

GstCpuThrottlingClock *
gst_cpu_throttling_clock_new (guint cpu_usage)
{
  return g_object_new (GST_TYPE_CPU_THROTTLING_CLOCK, "cpu-usage",
      cpu_usage, NULL);
}
#endif
