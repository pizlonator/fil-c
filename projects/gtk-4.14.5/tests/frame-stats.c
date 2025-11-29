/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>
#include <stdlib.h>

#include "frame-stats.h"
#include "variable.h"

typedef struct FrameStats FrameStats;

struct FrameStats
{
  GdkFrameClock *frame_clock;

  int num_stats;
  double last_print_time;
  int frames_since_last_print;
  gint64 last_handled_frame;

  Variable latency;
};

static int max_stats = -1;
static double statistics_time = 5.;
static gboolean machine_readable = FALSE;

static GOptionEntry frame_sync_options[] = {
  { "max-statistics", 'm', 0, G_OPTION_ARG_INT, &max_stats, "Maximum statistics printed", NULL },
  { "machine-readable", 0, 0, G_OPTION_ARG_NONE, &machine_readable, "Print statistics in columns", NULL },
  { "statistics-time", 's', 0, G_OPTION_ARG_DOUBLE, &statistics_time, "Statistics accumulation time", "TIME" },
  { NULL }
};

void
frame_stats_add_options (GOptionGroup *group)
{
  g_option_group_add_entries (group, frame_sync_options);
}

static void
print_double (const char *description,
              double      value)
{
  if (machine_readable)
    g_print ("%g\t", value);
  else
    g_print ("%s: %g\n", description, value);
}

static void
print_variable (const char *description,
                Variable *variable)
{
  if (variable->weight != 0)
    {
      if (machine_readable)
        g_print ("%g\t%g\t",
                 variable_mean (variable),
                 variable_standard_deviation (variable));
      else
        g_print ("%s: %g +/- %g\n", description,
                 variable_mean (variable),
                 variable_standard_deviation (variable));
    }
  else
    {
      if (machine_readable)
        g_print ("-\t-\t");
      else
        g_print ("%s: <n/a>\n", description);
    }
}

static void
on_frame_clock_after_paint (GdkFrameClock *frame_clock,
                            FrameStats    *frame_stats)
{
  gint64 frame_counter;
  gint64 current_time;

  current_time = g_get_monotonic_time ();
  if (current_time >= frame_stats->last_print_time + 1000000 * statistics_time)
    {
      if (frame_stats->frames_since_last_print)
        {
          if (frame_stats->num_stats == 0 && machine_readable)
            {
              g_print ("# load_factor frame_rate latency\n");
            }

          frame_stats->num_stats++;
          print_double ("Frame rate ",
                        frame_stats->frames_since_last_print /
                        ((current_time - frame_stats->last_print_time) / 1000000.));

          print_variable ("Latency", &frame_stats->latency);

          g_print ("\n");
        }

      frame_stats->last_print_time = current_time;
      frame_stats->frames_since_last_print = 0;
      variable_init (&frame_stats->latency);

      if (frame_stats->num_stats == max_stats)
        exit (0);
    }

  frame_stats->frames_since_last_print++;

  for (frame_counter = frame_stats->last_handled_frame;
       frame_counter < gdk_frame_clock_get_frame_counter (frame_clock);
       frame_counter++)
    {
      GdkFrameTimings *timings = gdk_frame_clock_get_timings (frame_clock, frame_counter);
      GdkFrameTimings *previous_timings = gdk_frame_clock_get_timings (frame_clock, frame_counter - 1);

      if (!timings || gdk_frame_timings_get_complete (timings))
        frame_stats->last_handled_frame = frame_counter;

      if (timings && gdk_frame_timings_get_complete (timings) && previous_timings &&
          gdk_frame_timings_get_presentation_time (timings) != 0 &&
          gdk_frame_timings_get_presentation_time (previous_timings) != 0)
        {
          double display_time = (gdk_frame_timings_get_presentation_time (timings) - gdk_frame_timings_get_presentation_time (previous_timings)) / 1000.;
          double frame_latency = (gdk_frame_timings_get_presentation_time (previous_timings) - gdk_frame_timings_get_frame_time (previous_timings)) / 1000. + display_time / 2;

          variable_add_weighted (&frame_stats->latency, frame_latency, display_time);
        }
    }
}

static void
on_window_realize (GtkWidget  *window,
                   FrameStats *frame_stats)
{
  frame_stats->frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (window));
  g_signal_connect (frame_stats->frame_clock, "after-paint",
                    G_CALLBACK (on_frame_clock_after_paint), frame_stats);
}

static void
on_window_unrealize (GtkWidget  *window,
                     FrameStats *frame_stats)
{
  g_signal_handlers_disconnect_by_func (frame_stats->frame_clock,
                                        (gpointer) on_frame_clock_after_paint,
                                        frame_stats);
  frame_stats->frame_clock = NULL;
}

static void
on_window_destroy (GtkWidget  *window,
                   FrameStats *stats)
{
  g_free (stats);
}

void
frame_stats_ensure (GtkWindow *window)
{
  FrameStats *frame_stats;

  frame_stats = g_object_get_data (G_OBJECT (window), "frame-stats");
  if (frame_stats != NULL)
    return;

  frame_stats = g_new0 (FrameStats, 1);
  g_object_set_data (G_OBJECT (window), "frame-stats", frame_stats);

  variable_init (&frame_stats->latency);
  frame_stats->last_handled_frame = -1;

  g_signal_connect (window, "realize",
                    G_CALLBACK (on_window_realize), frame_stats);
  g_signal_connect (window, "unrealize",
                    G_CALLBACK (on_window_unrealize), frame_stats);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (on_window_destroy), frame_stats);

  if (gtk_widget_get_realized (GTK_WIDGET (window)))
    on_window_realize (GTK_WIDGET (window), frame_stats);
}
