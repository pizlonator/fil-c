#include <math.h>
#include <gtk/gtk.h>

#include "variable.h"

typedef struct {
  double angle;
  gint64 stream_time;
  gint64 clock_time;
  gint64 frame_counter;
} FrameData;

static FrameData *displayed_frame;
static GtkWidget *window;
static GList *past_frames;
static Variable latency_error = VARIABLE_INIT;
static Variable time_factor_stats = VARIABLE_INIT;
static int dropped_frames = 0;
static int n_frames = 0;

static gboolean pll;
static int fps = 24;

/* Thread-safe frame queue */

#define MAX_QUEUE_LENGTH 5

static GQueue *frame_queue;
static GMutex frame_mutex;
static GCond frame_cond;

static void
queue_frame (FrameData *frame_data)
{
  g_mutex_lock (&frame_mutex);

  while (frame_queue->length == MAX_QUEUE_LENGTH)
    g_cond_wait (&frame_cond, &frame_mutex);

  g_queue_push_tail (frame_queue, frame_data);

  g_mutex_unlock (&frame_mutex);
}

static FrameData *
unqueue_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->length > 0)
    {
      frame_data = g_queue_pop_head (frame_queue);
      g_cond_signal (&frame_cond);
    }
  else
    {
      frame_data = NULL;
    }

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

static FrameData *
peek_pending_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->head)
    frame_data = frame_queue->head->data;
  else
    frame_data = NULL;

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

static FrameData *
peek_next_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->head && frame_queue->head->next)
    frame_data = frame_queue->head->next->data;
  else
    frame_data = NULL;

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

/* Frame producer thread */

static gpointer
create_frames_thread (gpointer data)
{
  int frame_count = 0;

  while (TRUE)
    {
      FrameData *frame_data = g_new0 (FrameData, 1);
      frame_data->angle = 2 * M_PI * (frame_count % fps) / (double)fps;
      frame_data->stream_time = (G_GINT64_CONSTANT (1000000) * frame_count) / fps;

      queue_frame (frame_data);
      frame_count++;
    }

  return NULL;
}

/* Clock management:
 *
 * The logic here, which is activated by the --pll argument
 * demonstrates adjusting the playback rate so that the frames exactly match
 * when they are displayed both frequency and phase. If there was an
 * accompanying audio track, you would need to resample the audio to match
 * the clock.
 *
 * The algorithm isn't exactly a PLL - I wrote it first that way, but
 * it oscillicated before coming into sync and this approach was easier than
 * fine-tuning the PLL filter.
 *
 * A more complicated algorithm could also establish sync when the playback
 * rate isn't exactly an integral divisor of the VBlank rate, such as 24fps
 * video on a 60fps display.
 */
#define PRE_BUFFER_TIME 500000

static gint64 stream_time_base;
static gint64 clock_time_base;
static double time_factor = 1.0;
static double frequency_time_factor = 1.0;
static double phase_time_factor = 1.0;

static gint64
stream_time_to_clock_time (gint64 stream_time)
{
  return clock_time_base + (stream_time - stream_time_base) * time_factor;
}

static void
adjust_clock_for_phase (gint64 frame_clock_time,
                        gint64 presentation_time)
{
  static int count = 0;
  static gint64 previous_frame_clock_time;
  static gint64 previous_presentation_time;
  gint64 phase = presentation_time - frame_clock_time;

  count++;
  if (count >= fps) /* Give a second of warmup */
    {
      gint64 time_delta = frame_clock_time - previous_frame_clock_time;
      gint64 previous_phase = previous_presentation_time - previous_frame_clock_time;

      double expected_phase_delta;

      stream_time_base += (frame_clock_time - clock_time_base) / time_factor;
      clock_time_base = frame_clock_time;

      expected_phase_delta = time_delta * (1 - phase_time_factor);

      /* If the phase is increasing that means the computed clock times are
       * increasing too slowly. We increase the frequency time factor to compensate,
       * but decrease the compensation so that it takes effect over 1 second to
       * avoid jitter */
      frequency_time_factor += (phase - previous_phase - expected_phase_delta) / (double)time_delta / fps;

      /* We also want to increase or decrease the frequency to bring the phase
       * into sync. We do that again so that the phase should sync up over 1 seconds
       */
      phase_time_factor = 1 + phase / 2000000.;

      time_factor = frequency_time_factor * phase_time_factor;
    }

  previous_frame_clock_time = frame_clock_time;
  previous_presentation_time = presentation_time;
}

/* Drawing */

static void
on_draw (GtkDrawingArea *da,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        data)
{
  double cx, cy, r;

  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0., 0., 0.);

  cx = width / 2.;
  cy = height / 2.;
  r = MIN (width, height) / 2.;

  cairo_arc (cr, cx, cy, r,
             0, 2 * M_PI);
  cairo_stroke (cr);
  if (displayed_frame)
    {
      cairo_move_to (cr, cx, cy);
      cairo_line_to (cr,
                     cx + r * cos(displayed_frame->angle - M_PI / 2),
                     cy + r * sin(displayed_frame->angle - M_PI / 2));
      cairo_stroke (cr);

      if (displayed_frame->frame_counter == 0)
        {
          GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (window);
          displayed_frame->frame_counter = gdk_frame_clock_get_frame_counter (frame_clock);
        }
    }
}

static void
collect_old_frames (void)
{
  GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (window);
  GList *l, *l_next;

  for (l = past_frames; l; l = l_next)
    {
      FrameData *frame_data = l->data;
      gboolean remove = FALSE;
      l_next = l->next;

      GdkFrameTimings *timings = gdk_frame_clock_get_timings (frame_clock,
                                                              frame_data->frame_counter);
      if (timings == NULL)
        {
          remove = TRUE;
        }
      else if (gdk_frame_timings_get_complete (timings))
        {
          gint64 presentation_time = gdk_frame_timings_get_predicted_presentation_time (timings);
          gint64 refresh_interval = gdk_frame_timings_get_refresh_interval (timings);

          if (pll &&
              presentation_time && refresh_interval &&
              presentation_time > frame_data->clock_time - refresh_interval / 2 &&
              presentation_time < frame_data->clock_time + refresh_interval / 2)
            adjust_clock_for_phase (frame_data->clock_time, presentation_time);

          if (presentation_time)
            variable_add (&latency_error,
                          presentation_time - frame_data->clock_time);

          remove = TRUE;
        }

      if (remove)
        {
          past_frames = g_list_delete_link (past_frames, l);
          g_free (frame_data);
        }
    }
}

static void
print_statistics (void)
{
  gint64 now = g_get_monotonic_time ();
  static gint64 last_print_time = 0;

  if (last_print_time == 0)
    last_print_time = now;
  else if (now -last_print_time > 5000000)
    {
      g_print ("dropped_frames: %d/%d\n",
               dropped_frames, n_frames);
      g_print ("collected_frames: %g/%d\n",
               latency_error.weight, n_frames);
      g_print ("latency_error: %g +/- %g\n",
               variable_mean (&latency_error),
               variable_standard_deviation (&latency_error));
      if (pll)
        g_print ("playback rate adjustment: %g +/- %g %%\n",
                 (variable_mean (&time_factor_stats) - 1) * 100,
                 variable_standard_deviation (&time_factor_stats) * 100);
      variable_init (&latency_error);
      variable_init (&time_factor_stats);
      dropped_frames = 0;
      n_frames = 0;
      last_print_time = now;
    }
}

static void
on_update (GdkFrameClock *frame_clock,
           gpointer       data)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (frame_clock);
  gint64 frame_time = gdk_frame_timings_get_frame_time (timings);
  gint64 predicted_presentation_time = gdk_frame_timings_get_predicted_presentation_time (timings);
  gint64 refresh_interval;
  FrameData *pending_frame;

  if (clock_time_base == 0)
    clock_time_base = frame_time + PRE_BUFFER_TIME;

  gdk_frame_clock_get_refresh_info (frame_clock, frame_time,
                                    &refresh_interval, NULL);

  pending_frame = peek_pending_frame ();
  g_assert (pending_frame);

  if (stream_time_to_clock_time (pending_frame->stream_time)
      < predicted_presentation_time + refresh_interval / 2)
    {
      while (TRUE)
        {
          FrameData *next_frame = peek_next_frame ();
          if (next_frame &&
              stream_time_to_clock_time (next_frame->stream_time)
              < predicted_presentation_time + refresh_interval / 2)
            {
              g_free (unqueue_frame ());
              n_frames++;
              dropped_frames++;
              pending_frame = next_frame;
            }
          else
            break;
        }

      if (displayed_frame)
        past_frames = g_list_prepend (past_frames, displayed_frame);

      n_frames++;
      displayed_frame = unqueue_frame ();
      g_assert (displayed_frame);
      displayed_frame->clock_time = stream_time_to_clock_time (displayed_frame->stream_time);

      displayed_frame->frame_counter = gdk_frame_timings_get_frame_counter (timings);
      variable_add (&time_factor_stats, time_factor);

      collect_old_frames ();
      print_statistics ();

      gtk_widget_queue_draw (window);
    }
}

static GOptionEntry options[] = {
  { "pll", 'p', 0, G_OPTION_ARG_NONE, &pll, "Sync frame rate to refresh", NULL },
  { "fps", 'f', 0, G_OPTION_ARG_INT, &fps, "Frame rate", "FPS" },
  { NULL }
};

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main(int argc, char **argv)
{
  GtkWidget *da;
  GError *error = NULL;
  GdkFrameClock *frame_clock;
  GOptionContext *context;
  gboolean done = FALSE;

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  g_option_context_free (context);

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (quit_cb), &done);

  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), on_draw, NULL, NULL);
  gtk_window_set_child (GTK_WINDOW (window), da);

  gtk_window_present (GTK_WINDOW (window));

  frame_queue = g_queue_new ();
  g_mutex_init (&frame_mutex);
  g_cond_init (&frame_cond);

  g_thread_new ("Create Frames", create_frames_thread, NULL);

  frame_clock = gtk_widget_get_frame_clock (window);
  g_signal_connect (frame_clock, "update",
                    G_CALLBACK (on_update), NULL);
  gdk_frame_clock_begin_updating (frame_clock);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
