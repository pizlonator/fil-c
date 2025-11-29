/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gsk/gskcairoblurprivate.h>

static void
init_surface (cairo_t *cr)
{
  int w = cairo_image_surface_get_width (cairo_get_target (cr));
  int h = cairo_image_surface_get_height (cairo_get_target (cr));

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_arc (cr, w/2, h/2, w/2, 0, 2*G_PI);
  cairo_fill (cr);
}

int
main (int argc, char **argv)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GTimer *timer;
  double msec;
  int i, j;
  int size;

  timer = g_timer_new ();

  size = 2000;

  surface = cairo_image_surface_create (CAIRO_FORMAT_A8, size, size);

  cr = cairo_create (surface);

  /* We do everything three times, first two as warmup */
  for (j = 0; j < 2; j++)
    {
      for (i = 1; i < 16; i++)
	{
	  init_surface (cr);
	  g_timer_start (timer);
	  gsk_cairo_blur_surface (surface, i, GSK_BLUR_X | GSK_BLUR_Y);
	  msec = g_timer_elapsed (timer, NULL) * 1000;
	  if (j == 1)
	    g_print ("Radius %2d: %.2f msec, %.2f kpixels/msec:\n", i, msec, size*size/(msec*1000));
	}
    }

  g_timer_destroy (timer);

  return 0;
}
