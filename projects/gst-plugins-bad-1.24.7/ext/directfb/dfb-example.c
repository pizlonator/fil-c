
#include <directfb.h>

#ifdef __no_instrument_function__
#undef __no_instrument_function__
#endif

#include <stdio.h>
#include <gst/gst.h>

static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static GMainLoop *loop;

#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }

static gboolean
get_me_out (gpointer data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

int
main (int argc, char *argv[])
{
  DFBSurfaceDescription dsc;
  GstElement *pipeline, *src, *sink;

  /* Init both GStreamer and DirectFB */
  DFBCHECK (DirectFBInit (&argc, &argv));
  gst_init (&argc, &argv);

  /* Creates DirectFB main context and set it to fullscreen layout */
  DFBCHECK (DirectFBCreate (&dfb));
  DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));

  /* We want a double buffered primary surface */
  dsc.flags = DSDESC_CAPS;
  dsc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

  DFBCHECK (dfb->CreateSurface (dfb, &dsc, &primary));

  /* Creating our pipeline : videotestsrc ! dfbvideosink */
  pipeline = gst_pipeline_new (NULL);
  g_assert (pipeline);
  src = gst_element_factory_make ("videotestsrc", NULL);
  g_assert (src);
  sink = gst_element_factory_make ("dfbvideosink", NULL);
  g_assert (sink);
  /* That's the interesting part, giving the primary surface to dfbvideosink */
  g_object_set (sink, "surface", primary, NULL);

  /* Adding elements to the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  if (!gst_element_link (src, sink))
    g_error ("Couldn't link videotestsrc and dfbvideosink");

  /* Let's play ! */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* we need to run a GLib main loop to get out of here */
  loop = g_main_loop_new (NULL, FALSE);
  /* Get us out after 20 seconds */
  g_timeout_add (20000, get_me_out, NULL);
  g_main_loop_run (loop);

  /* Release elements and stop playback */
  gst_element_set_state (pipeline, GST_STATE_NULL);

  /* Free the main loop */
  g_main_loop_unref (loop);

  /* Release DirectFB context and surface */
  primary->Release (primary);
  dfb->Release (dfb);

  return 0;
}
