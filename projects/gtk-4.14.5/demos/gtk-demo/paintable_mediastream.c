/* Paintable/Media Stream
 *
 * GdkPaintable is also used by the GtkMediaStream class.
 *
 * This demo code turns the nuclear media_stream into the object
 * GTK uses for videos. This allows treating the icon like a
 * regular video, so we can for example attach controls to it.
 *
 * After all, what good is a media_stream if one cannot pause
 * it.
 */

#include <gtk/gtk.h>

#include "paintable.h"

static GtkWidget *window = NULL;

/* First, add the boilerplate for the object itself.
 * This part would normally go in the header.
 */
#define GTK_TYPE_NUCLEAR_MEDIA_STREAM (gtk_nuclear_media_stream_get_type ())
G_DECLARE_FINAL_TYPE (GtkNuclearMediaStream, gtk_nuclear_media_stream, GTK, NUCLEAR_MEDIA_STREAM, GtkMediaStream)

/* Do a full rotation in 5 seconds.
 *
 * We do not save steps here but real timestamps.
 * GtkMediaStream uses microseconds, so we will do so, too.
 */
#define DURATION (5 * G_USEC_PER_SEC)

/* Declare the struct. */
struct _GtkNuclearMediaStream
{
  /* We now inherit from the media stream object. */
  GtkMediaStream parent_instance;

  /* This variable stores the progress of our video.
   */
  gint64 progress;

  /* This variable stores the timestamp of the last
   * time we updated the progress variable when the
   * video is currently playing.
   * This is so that we can always accurately compute the
   * progress we've had, even if the timeout does not
   * exactly work.
   */
  gint64 last_time;

  /* This variable again holds the ID of the timer that 
   * updates our progress variable. Nothing changes about
   * how this works compared to the previous example.
   */
  guint source_id;
};

struct _GtkNuclearMediaStreamClass
{
  GObjectClass parent_class;
};

/* GtkMediaStream is a GdkPaintable. So when we want to display video,
 * we have to implement the interface, just like in the animation example.
 */
static void
gtk_nuclear_media_stream_snapshot (GdkPaintable *paintable,
                                   GdkSnapshot  *snapshot,
                                   double        width,
                                   double        height)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (paintable);

  /* We call the function from the previous example here. */
  gtk_nuclear_snapshot (snapshot,
                        &(GdkRGBA) { 0, 0, 0, 1 }, /* black */
                        &(GdkRGBA) { 0.9, 0.75, 0.15, 1.0 }, /* yellow */
                        width, height,
                        360 * nuclear->progress / DURATION);
}

static GdkPaintable *
gtk_nuclear_media_stream_get_current_image (GdkPaintable *paintable)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (paintable);

  /* Same thing as with the animation */
  return gtk_nuclear_icon_new (360 * nuclear->progress / DURATION);
}

static GdkPaintableFlags
gtk_nuclear_media_stream_get_flags (GdkPaintable *paintable)
{
  /* And same thing as with the animation over here, too. */
  return GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_nuclear_media_stream_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_nuclear_media_stream_snapshot;
  iface->get_current_image = gtk_nuclear_media_stream_get_current_image;
  iface->get_flags = gtk_nuclear_media_stream_get_flags;
}

/* This time, we inherit from GTK_TYPE_MEDIA_STREAM */
G_DEFINE_TYPE_WITH_CODE (GtkNuclearMediaStream, gtk_nuclear_media_stream, GTK_TYPE_MEDIA_STREAM,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_nuclear_media_stream_paintable_init))

static gboolean
gtk_nuclear_media_stream_step (gpointer data)
{
  GtkNuclearMediaStream *nuclear = data;
  gint64 current_time;

  /* Compute the time that has elapsed since the last time we were called
   * and add it to our current progress.
   */
  current_time = g_source_get_time (g_main_current_source ());
  nuclear->progress += current_time - nuclear->last_time;

  /* Check if we've ended */
  if (nuclear->progress > DURATION)
    {
      if (gtk_media_stream_get_loop (GTK_MEDIA_STREAM (nuclear)))
        {
          /* We're looping. So make the progress loop using modulo */
          nuclear->progress %= DURATION;
        }
      else
        {
          /* Just make sure we don't overflow */
          nuclear->progress = DURATION;
        }
    }

  /* Update the last time to the current timestamp. */
  nuclear->last_time = current_time;

  /* Update the timestamp of the media stream */
  gtk_media_stream_update (GTK_MEDIA_STREAM (nuclear), nuclear->progress);

  /* We also need to invalidate our contents again.
   * After all, we are a video and not just an audio stream.
   */
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (nuclear));

  /* Now check if we have finished playing and if so,
   * tell the media stream. The media stream will then
   * call our pause function to pause the stream.
   */
  if (nuclear->progress >= DURATION)
    gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (nuclear));

  /* The timeout function is removed by the pause function,
   * so we can just always return this value.
   */
  return G_SOURCE_CONTINUE;
}

static gboolean
gtk_nuclear_media_stream_play (GtkMediaStream *stream)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (stream);

  /* If we're already at the end of the stream, we don't want
   * to start playing and exit early.
   */
  if (nuclear->progress >= DURATION)
    return FALSE;

  /* This time, we add the source only when we start playing.
   */
  nuclear->source_id = g_timeout_add (10,
                                      gtk_nuclear_media_stream_step,
                                      nuclear);

  /* We also want to initialize our time, so that we can
   * do accurate updates.
   */
  nuclear->last_time = g_get_monotonic_time ();
  
  /* We successfully started playing, so we return TRUE here. */
  return TRUE;
}

static void
gtk_nuclear_media_stream_pause (GtkMediaStream *stream)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (stream);

  /* This function will be called when a playing stream
   * gets paused.
   * So we remove the updating source here and set it
   * back to 0 so that the finalize function doesn't try
   * to remove it again.
   */
  g_source_remove (nuclear->source_id);
  nuclear->source_id = 0;
  nuclear->last_time = 0;
}

static void
gtk_nuclear_media_stream_seek (GtkMediaStream *stream,
                               gint64          timestamp)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (stream);

  /* This is optional functionality for media streams,
   * but not being able to seek is kinda boring.
   * And it's trivial to implement, so let's go for it.
   */
  nuclear->progress = timestamp;

  /* Media streams are asynchronous, so seeking can take a while.
   * We however don't need that functionality, so we can just
   * report success.
   */
  gtk_media_stream_seek_success (stream);

  /* We also have to update our timestamp and tell the
   * paintable interface about the seek
   */
  gtk_media_stream_update (stream, nuclear->progress);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (nuclear));
}

/* Again, we need to implement the finalize function.
 */
static void
gtk_nuclear_media_stream_finalize (GObject *object)
{
  GtkNuclearMediaStream *nuclear = GTK_NUCLEAR_MEDIA_STREAM (object);

  /* This time, we need to check if the source exists before
   * removing it as it only exists while we are playing.
   */
  if (nuclear->source_id > 0)
    g_source_remove (nuclear->source_id);

  /* Don't forget to chain up to the parent class' implementation
   * of the finalize function.
   */
  G_OBJECT_CLASS (gtk_nuclear_media_stream_parent_class)->finalize (object);
}

/* In the class declaration, we need to implement the media stream */
static void
gtk_nuclear_media_stream_class_init (GtkNuclearMediaStreamClass *klass)
{
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  stream_class->play = gtk_nuclear_media_stream_play;
  stream_class->pause = gtk_nuclear_media_stream_pause;
  stream_class->seek = gtk_nuclear_media_stream_seek;

  gobject_class->finalize = gtk_nuclear_media_stream_finalize;
}

static void
gtk_nuclear_media_stream_init (GtkNuclearMediaStream *nuclear)
{
  /* This time, we don't have to add a timer here, because media
   * streams start paused.
   *
   * However, media streams need to tell GTK once they are initialized,
   * so we do that here.
   */
  gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (nuclear),
                                    FALSE,
                                    TRUE,
                                    TRUE,
                                    DURATION);
}

/* And finally, we add the simple constructor we declared in the header. */
GtkMediaStream *
gtk_nuclear_media_stream_new (void)
{
  return g_object_new (GTK_TYPE_NUCLEAR_MEDIA_STREAM, NULL);
}

GtkWidget *
do_paintable_mediastream (GtkWidget *do_widget)
{
  GtkMediaStream *nuclear;
  GtkWidget *video;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Nuclear MediaStream");
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      nuclear = gtk_nuclear_media_stream_new ();
      gtk_media_stream_set_loop (GTK_MEDIA_STREAM (nuclear), TRUE);

      video = gtk_video_new_for_media_stream (nuclear);
      gtk_window_set_child (GTK_WINDOW (window), video);

      g_object_unref (nuclear);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
