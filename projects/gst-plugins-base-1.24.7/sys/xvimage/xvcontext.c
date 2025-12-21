/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
 *               <2009>,<2010> Stefan Kost <stefan.kost@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* for developers: there are two useful tools : xvinfo and xvattr */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Object header */
#include "xvcontext.h"

/* Debugging category */
#include <gst/gstinfo.h>

/* for getpid */
#include <unistd.h>

/* for XkbKeycodeToKeysym */
#include <X11/XKBlib.h>

/* for touchscreen events */
#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#endif

GST_DEBUG_CATEGORY (gst_debug_xv_context);
#define GST_CAT_DEFAULT gst_debug_xv_context

void
gst_xvcontext_config_clear (GstXvContextConfig * config)
{
  if (config->display_name) {
    g_free (config->display_name);
    config->display_name = NULL;
  }
  config->adaptor_nr = -1;
}

GST_DEFINE_MINI_OBJECT_TYPE (GstXvContext, gst_xvcontext);

typedef struct
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
}
MotifWmHints, MwmHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)


static void
gst_lookup_xv_port_from_adaptor (GstXvContext * context,
    XvAdaptorInfo * adaptors, guint adaptor_nr)
{
  gint j;
  gint res;

  /* Do we support XvImageMask ? */
  if (!(adaptors[adaptor_nr].type & XvImageMask)) {
    GST_DEBUG ("XV Adaptor %s has no support for XvImageMask",
        adaptors[adaptor_nr].name);
    return;
  }

  /* We found such an adaptor, looking for an available port */
  for (j = 0; j < adaptors[adaptor_nr].num_ports && !context->xv_port_id; j++) {
    /* We try to grab the port */
    res = XvGrabPort (context->disp, adaptors[adaptor_nr].base_id + j, 0);
    if (Success == res) {
      context->xv_port_id = adaptors[adaptor_nr].base_id + j;
      GST_DEBUG ("XV Adaptor %s with %ld ports", adaptors[adaptor_nr].name,
          adaptors[adaptor_nr].num_ports);
    } else {
      GST_DEBUG ("GrabPort %d for XV Adaptor %s failed: %d", j,
          adaptors[adaptor_nr].name, res);
    }
  }
}

/* This function generates a caps with all supported format by the first
   Xv grabable port we find. We store each one of the supported formats in a
   format list and append the format to a newly created caps that we return
   If this function does not return NULL because of an error, it also grabs
   the port via XvGrabPort */
static GstCaps *
gst_xvcontext_get_xv_support (GstXvContext * context,
    const GstXvContextConfig * config, GError ** error)
{
  gint i;
  XvAdaptorInfo *adaptors;
  gint nb_formats;
  XvImageFormatValues *formats = NULL;
  guint nb_encodings;
  XvEncodingInfo *encodings = NULL;
  gulong max_w = G_MAXINT, max_h = G_MAXINT;
  GstCaps *caps = NULL;
  GstCaps *rgb_caps = NULL;

  g_return_val_if_fail (context != NULL, NULL);

  /* First let's check that XVideo extension is available */
  if (!XQueryExtension (context->disp, "XVideo", &i, &i, &i))
    goto no_xv;

  /* Then we get adaptors list */
  if (Success != XvQueryAdaptors (context->disp, context->root,
          &context->nb_adaptors, &adaptors))
    goto no_adaptors;

  context->xv_port_id = 0;

  GST_DEBUG ("Found %u XV adaptor(s)", context->nb_adaptors);

  context->adaptors =
      (gchar **) g_malloc0 (context->nb_adaptors * sizeof (gchar *));

  /* Now fill up our adaptor name array */
  for (i = 0; i < context->nb_adaptors; i++) {
    context->adaptors[i] = g_strdup (adaptors[i].name);
  }

  if (config->adaptor_nr != -1 && config->adaptor_nr < context->nb_adaptors) {
    /* Find xv port from user defined adaptor */
    gst_lookup_xv_port_from_adaptor (context, adaptors, config->adaptor_nr);
  }

  if (!context->xv_port_id) {
    /* Now search for an adaptor that supports XvImageMask */
    for (i = 0; i < context->nb_adaptors && !context->xv_port_id; i++) {
      gst_lookup_xv_port_from_adaptor (context, adaptors, i);
      context->adaptor_nr = i;
    }
  }

  XvFreeAdaptorInfo (adaptors);

  if (!context->xv_port_id)
    goto no_ports;

  /* Set XV_AUTOPAINT_COLORKEY and XV_DOUBLE_BUFFER and XV_COLORKEY */
  {
    int count, todo = 4;
    XvAttribute *const attr = XvQueryPortAttributes (context->disp,
        context->xv_port_id, &count);
    static const char autopaint[] = "XV_AUTOPAINT_COLORKEY";
    static const char dbl_buffer[] = "XV_DOUBLE_BUFFER";
    static const char colorkey[] = "XV_COLORKEY";
    static const char iturbt709[] = "XV_ITURBT_709";
    static const char *xv_colorspace = "XV_COLORSPACE";

    GST_DEBUG ("Checking %d Xv port attributes", count);

    context->have_autopaint_colorkey = FALSE;
    context->have_double_buffer = FALSE;
    context->have_colorkey = FALSE;
    context->have_iturbt709 = FALSE;
    context->have_xvcolorspace = FALSE;

    for (i = 0; ((i < count) && todo); i++) {
      GST_DEBUG ("Got attribute %s", attr[i].name);

      if (!strcmp (attr[i].name, autopaint)) {
        const Atom atom = XInternAtom (context->disp, autopaint, False);

        /* turn on autopaint colorkey */
        XvSetPortAttribute (context->disp, context->xv_port_id, atom,
            (config->autopaint_colorkey ? 1 : 0));
        todo--;
        context->have_autopaint_colorkey = TRUE;
      } else if (!strcmp (attr[i].name, dbl_buffer)) {
        const Atom atom = XInternAtom (context->disp, dbl_buffer, False);

        XvSetPortAttribute (context->disp, context->xv_port_id, atom,
            (config->double_buffer ? 1 : 0));
        todo--;
        context->have_double_buffer = TRUE;
      } else if (!strcmp (attr[i].name, colorkey)) {
        /* Set the colorkey, default is something that is dark but hopefully
         * won't randomly appear on the screen elsewhere (ie not black or greys)
         * can be overridden by setting "colorkey" property
         */
        const Atom atom = XInternAtom (context->disp, colorkey, False);
        guint32 ckey = 0;
        gboolean set_attr = TRUE;
        guint cr, cg, cb;

        /* set a colorkey in the right format RGB565/RGB888
         * We only handle these 2 cases, because they're the only types of
         * devices we've encountered. If we don't recognise it, leave it alone
         */
        cr = (config->colorkey >> 16);
        cg = (config->colorkey >> 8) & 0xFF;
        cb = (config->colorkey) & 0xFF;
        switch (context->depth) {
          case 16:             /* RGB 565 */
            cr >>= 3;
            cg >>= 2;
            cb >>= 3;
            ckey = (cr << 11) | (cg << 5) | cb;
            break;
          case 24:
          case 32:             /* RGB 888 / ARGB 8888 */
            ckey = (cr << 16) | (cg << 8) | cb;
            break;
          default:
            GST_DEBUG ("Unknown bit depth %d for Xv Colorkey - not adjusting",
                context->depth);
            set_attr = FALSE;
            break;
        }

        if (set_attr) {
          ckey = CLAMP (ckey, (guint32) attr[i].min_value,
              (guint32) attr[i].max_value);
          GST_LOG ("Setting color key for display depth %d to 0x%x",
              context->depth, ckey);

          XvSetPortAttribute (context->disp, context->xv_port_id, atom,
              (gint) ckey);
        }
        todo--;
        context->have_colorkey = TRUE;
      } else if (!strcmp (attr[i].name, iturbt709)) {
        todo--;
        context->have_iturbt709 = TRUE;
      } else if (!strcmp (attr[i].name, xv_colorspace)) {
        context->have_xvcolorspace = TRUE;
        todo--;
      }
    }

    XFree (attr);
  }

  /* Get the list of encodings supported by the adapter and look for the
   * XV_IMAGE encoding so we can determine the maximum width and height
   * supported */
  XvQueryEncodings (context->disp, context->xv_port_id, &nb_encodings,
      &encodings);

  for (i = 0; i < nb_encodings; i++) {
    GST_LOG ("Encoding %d, name %s, max wxh %lux%lu rate %d/%d",
        i, encodings[i].name, encodings[i].width, encodings[i].height,
        encodings[i].rate.numerator, encodings[i].rate.denominator);
    if (strcmp (encodings[i].name, "XV_IMAGE") == 0) {
      max_w = encodings[i].width;
      max_h = encodings[i].height;
    }
  }

  XvFreeEncodingInfo (encodings);

  /* We get all image formats supported by our port */
  formats = XvListImageFormats (context->disp,
      context->xv_port_id, &nb_formats);
  caps = gst_caps_new_empty ();
  for (i = 0; i < nb_formats; i++) {
    GstCaps *format_caps = NULL;
    gboolean is_rgb_format = FALSE;
    GstVideoFormat vformat;

    /* We set the image format of the context to an existing one. This
       is just some valid image format for making our xshm calls check before
       caps negotiation really happens. */
    context->im_format = formats[i].id;

    switch (formats[i].type) {
      case XvRGB:
      {
        XvImageFormatValues *fmt = &(formats[i]);
        gint endianness;

        endianness =
            (fmt->byte_order == LSBFirst ? G_LITTLE_ENDIAN : G_BIG_ENDIAN);

        vformat = gst_video_format_from_masks (fmt->depth, fmt->bits_per_pixel,
            endianness, fmt->red_mask, fmt->green_mask, fmt->blue_mask, 0);
        if (vformat == GST_VIDEO_FORMAT_UNKNOWN)
          break;

        format_caps = gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (vformat),
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

        is_rgb_format = TRUE;
        break;
      }
      case XvYUV:
      {
        vformat = gst_video_format_from_fourcc (formats[i].id);
        if (vformat == GST_VIDEO_FORMAT_UNKNOWN)
          break;

        format_caps = gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (vformat),
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
        break;
      }
      default:
        vformat = GST_VIDEO_FORMAT_UNKNOWN;
        g_assert_not_reached ();
        break;
    }

    if (format_caps) {
      GstXvImageFormat *format = NULL;

      format = g_new0 (GstXvImageFormat, 1);
      if (format) {
        format->format = formats[i].id;
        format->vformat = vformat;
        format->caps = gst_caps_copy (format_caps);
        context->formats_list = g_list_append (context->formats_list, format);
      }

      if (is_rgb_format) {
        if (rgb_caps == NULL)
          rgb_caps = format_caps;
        else
          gst_caps_append (rgb_caps, format_caps);
      } else
        gst_caps_append (caps, format_caps);
    }
  }

  /* Collected all caps into either the caps or rgb_caps structures.
   * Append rgb_caps on the end of YUV, so that YUV is always preferred */
  if (rgb_caps)
    gst_caps_append (caps, rgb_caps);

  if (formats)
    XFree (formats);

  GST_DEBUG ("Generated the following caps: %" GST_PTR_FORMAT, caps);

  if (gst_caps_is_empty (caps))
    goto no_caps;

  return caps;

  /* ERRORS */
no_xv:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_SETTINGS,
        ("XVideo extension is not available"));
    return NULL;
  }
no_adaptors:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_SETTINGS,
        ("Failed getting XV adaptors list"));
    return NULL;
  }
no_ports:
  {
    context->adaptor_nr = -1;
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_BUSY,
        ("No Xv Port available"));
    return NULL;
  }
no_caps:
  {
    gst_caps_unref (caps);
    g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE,
        ("No supported format found"));
    return NULL;
  }
}

/* This function calculates the pixel aspect ratio based on the properties
 * in the context structure and stores it there. */
static void
gst_xvcontext_calculate_pixel_aspect_ratio (GstXvContext * context)
{
  static const gint par[][2] = {
    {1, 1},                     /* regular screen */
    {16, 15},                   /* PAL TV */
    {11, 10},                   /* 525 line Rec.601 video */
    {54, 59},                   /* 625 line Rec.601 video */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on 4:3 display */
    {4, 3}                      /*  800x600 on 16:9 display */
  };
  gint i;
  gint index;
  gdouble ratio;
  gdouble delta;

#define DELTA(idx) (ABS (ratio - ((gdouble) par[idx][0] / par[idx][1])))

  /* first calculate the "real" ratio based on the X values;
   * which is the "physical" w/h divided by the w/h in pixels of the display */
  ratio = (gdouble) (context->widthmm * context->height)
      / (context->heightmm * context->width);

  /* DirectFB's X in 720x576 reports the physical dimensions wrong, so
   * override here */
  if (context->width == 720 && context->height == 576) {
    ratio = 4.0 * 576 / (3.0 * 720);
  }
  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);

  /* now find the one from par[][2] with the lowest delta to the real one */
  delta = DELTA (0);
  index = 0;

  for (i = 1; i < sizeof (par) / (sizeof (gint) * 2); ++i) {
    gdouble this_delta = DELTA (i);

    if (this_delta < delta) {
      index = i;
      delta = this_delta;
    }
  }

  GST_DEBUG ("Decided on index %d (%d/%d)", index,
      par[index][0], par[index][1]);

  g_free (context->par);
  context->par = g_new0 (GValue, 1);
  g_value_init (context->par, GST_TYPE_FRACTION);
  gst_value_set_fraction (context->par, par[index][0], par[index][1]);
  GST_DEBUG ("set context PAR to %d/%d",
      gst_value_get_fraction_numerator (context->par),
      gst_value_get_fraction_denominator (context->par));
}

#ifdef HAVE_XSHM
/* X11 stuff */
static gboolean error_caught = FALSE;

static int
gst_xvimage_handle_xerror (Display * display, XErrorEvent * xevent)
{
  char error_msg[1024];

  XGetErrorText (display, xevent->error_code, error_msg, 1024);
  GST_DEBUG ("xvimage triggered an XError. error: %s", error_msg);
  error_caught = TRUE;
  return 0;
}

/* This function checks that it is actually really possible to create an image
   using XShm */
static gboolean
gst_xvcontext_check_xshm_calls (GstXvContext * context)
{
  XvImage *xvimage;
  XShmSegmentInfo SHMInfo;
  size_t size;
  int (*handler) (Display *, XErrorEvent *);
  gboolean result = FALSE;
  gboolean did_attach = FALSE;

  g_return_val_if_fail (context != NULL, FALSE);

  /* Sync to ensure any older errors are already processed */
  XSync (context->disp, FALSE);

  /* Set defaults so we don't free these later unnecessarily */
  SHMInfo.shmaddr = ((void *) -1);
  SHMInfo.shmid = -1;

  /* Setting an error handler to catch failure */
  error_caught = FALSE;
  handler = XSetErrorHandler (gst_xvimage_handle_xerror);

  /* Trying to create a 1x1 picture */
  GST_DEBUG ("XvShmCreateImage of 1x1");
  xvimage = XvShmCreateImage (context->disp, context->xv_port_id,
      context->im_format, NULL, 1, 1, &SHMInfo);

  /* Might cause an error, sync to ensure it is noticed */
  XSync (context->disp, FALSE);
  if (!xvimage || error_caught) {
    GST_WARNING ("could not XvShmCreateImage a 1x1 image");
    goto beach;
  }
  size = xvimage->data_size;
  SHMInfo.shmid = shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
  if (SHMInfo.shmid == -1) {
    GST_WARNING ("could not get shared memory of %" G_GSIZE_FORMAT " bytes",
        size);
    goto beach;
  }

  SHMInfo.shmaddr = shmat (SHMInfo.shmid, NULL, 0);
  if (SHMInfo.shmaddr == ((void *) -1)) {
    GST_WARNING ("Failed to shmat: %s", g_strerror (errno));
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  xvimage->data = SHMInfo.shmaddr;
  SHMInfo.readOnly = FALSE;

  if (XShmAttach (context->disp, &SHMInfo) == 0) {
    GST_WARNING ("Failed to XShmAttach");
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  /* Sync to ensure we see any errors we caused */
  XSync (context->disp, FALSE);

  /* Delete the shared memory segment as soon as everyone is attached.
   * This way, it will be deleted as soon as we detach later, and not
   * leaked if we crash. */
  shmctl (SHMInfo.shmid, IPC_RMID, NULL);

  if (!error_caught) {
    GST_DEBUG ("XServer ShmAttached to 0x%x, id 0x%lx", SHMInfo.shmid,
        SHMInfo.shmseg);

    did_attach = TRUE;
    /* store whether we succeeded in result */
    result = TRUE;
  } else {
    GST_WARNING ("MIT-SHM extension check failed at XShmAttach. "
        "Not using shared memory.");
  }

beach:
  /* Sync to ensure we swallow any errors we caused and reset error_caught */
  XSync (context->disp, FALSE);

  error_caught = FALSE;
  XSetErrorHandler (handler);

  if (did_attach) {
    GST_DEBUG ("XServer ShmDetaching from 0x%x id 0x%lx",
        SHMInfo.shmid, SHMInfo.shmseg);
    XShmDetach (context->disp, &SHMInfo);
    XSync (context->disp, FALSE);
  }
  if (SHMInfo.shmaddr != ((void *) -1))
    shmdt (SHMInfo.shmaddr);
  if (xvimage)
    XFree (xvimage);
  return result;
}
#endif /* HAVE_XSHM */

static GstXvContext *
gst_xvcontext_copy (GstXvContext * context)
{
  return NULL;
}

static void
gst_xvcontext_free (GstXvContext * context)
{
  GList *formats_list, *channels_list;
  gint i = 0;

  GST_LOG ("free %p", context);

  formats_list = context->formats_list;

  while (formats_list) {
    GstXvImageFormat *format = formats_list->data;

    gst_caps_unref (format->caps);
    g_free (format);
    formats_list = g_list_next (formats_list);
  }

  if (context->formats_list)
    g_list_free (context->formats_list);

  channels_list = context->channels_list;

  while (channels_list) {
    GstColorBalanceChannel *channel = channels_list->data;

    g_object_unref (channel);
    channels_list = g_list_next (channels_list);
  }

  if (context->channels_list)
    g_list_free (context->channels_list);

  if (context->caps)
    gst_caps_unref (context->caps);
  if (context->last_caps)
    gst_caps_unref (context->last_caps);

  for (i = 0; i < context->nb_adaptors; i++) {
    g_free (context->adaptors[i]);
  }

  g_free (context->adaptors);

  g_free (context->par);

  GST_DEBUG ("Closing display and freeing X Context");

  if (context->xv_port_id)
    XvUngrabPort (context->disp, context->xv_port_id, 0);

  if (context->disp)
    XCloseDisplay (context->disp);

  g_mutex_clear (&context->lock);

  g_free (context);
}


/* This function gets the X Display and global info about it. Everything is
   stored in our object and will be cleaned when the object is disposed. Note
   here that caps for supported format are generated without any window or
   image creation */
GstXvContext *
gst_xvcontext_new (GstXvContextConfig * config, GError ** error)
{
  GstXvContext *context = NULL;
  XPixmapFormatValues *px_formats = NULL;
  gint nb_formats = 0, i, j, N_attr;
  XvAttribute *xv_attr;
  Atom prop_atom;
  const char *channels[4] = { "XV_HUE", "XV_SATURATION",
    "XV_BRIGHTNESS", "XV_CONTRAST"
  };
  int opcode, event, err;
  int xkb_major = XkbMajorVersion;
  int xkb_minor = XkbMinorVersion;
#ifdef HAVE_XI2
  int xi2_major = XI_2_Major;
  int xi2_minor = XI_2_Minor;
#endif

  g_return_val_if_fail (config != NULL, NULL);

  context = g_new0 (GstXvContext, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (context), 0,
      gst_xvcontext_get_type (),
      (GstMiniObjectCopyFunction) gst_xvcontext_copy,
      (GstMiniObjectDisposeFunction) NULL,
      (GstMiniObjectFreeFunction) gst_xvcontext_free);

  g_mutex_init (&context->lock);
  context->im_format = 0;
  context->adaptor_nr = -1;

  if (!(context->disp = XOpenDisplay (config->display_name)))
    goto no_display;

  context->screen = DefaultScreenOfDisplay (context->disp);
  context->screen_num = DefaultScreen (context->disp);
  context->visual = DefaultVisual (context->disp, context->screen_num);
  context->root = DefaultRootWindow (context->disp);
  context->white = XWhitePixel (context->disp, context->screen_num);
  context->black = XBlackPixel (context->disp, context->screen_num);
  context->depth = DefaultDepthOfScreen (context->screen);

  context->width = DisplayWidth (context->disp, context->screen_num);
  context->height = DisplayHeight (context->disp, context->screen_num);
  context->widthmm = DisplayWidthMM (context->disp, context->screen_num);
  context->heightmm = DisplayHeightMM (context->disp, context->screen_num);

  GST_DEBUG ("X reports %dx%d pixels and %d mm x %d mm",
      context->width, context->height, context->widthmm, context->heightmm);

  gst_xvcontext_calculate_pixel_aspect_ratio (context);
  /* We get supported pixmap formats at supported depth */
  px_formats = XListPixmapFormats (context->disp, &nb_formats);

  if (!px_formats)
    goto no_pixel_formats;

  /* We get bpp value corresponding to our running depth */
  for (i = 0; i < nb_formats; i++) {
    if (px_formats[i].depth == context->depth)
      context->bpp = px_formats[i].bits_per_pixel;
  }

  XFree (px_formats);

  context->endianness =
      (ImageByteOrder (context->disp) ==
      LSBFirst) ? G_LITTLE_ENDIAN : G_BIG_ENDIAN;

  /* our caps system handles 24/32bpp RGB as big-endian. */
  if ((context->bpp == 24 || context->bpp == 32) &&
      context->endianness == G_LITTLE_ENDIAN) {
    context->endianness = G_BIG_ENDIAN;
    context->visual->red_mask = GUINT32_TO_BE (context->visual->red_mask);
    context->visual->green_mask = GUINT32_TO_BE (context->visual->green_mask);
    context->visual->blue_mask = GUINT32_TO_BE (context->visual->blue_mask);
    if (context->bpp == 24) {
      context->visual->red_mask >>= 8;
      context->visual->green_mask >>= 8;
      context->visual->blue_mask >>= 8;
    }
  }

  if (!(context->caps = gst_xvcontext_get_xv_support (context, config, error)))
    goto no_caps;

  /* Search for XShm extension support */
#ifdef HAVE_XSHM
  if (XShmQueryExtension (context->disp) &&
      gst_xvcontext_check_xshm_calls (context)) {
    context->use_xshm = TRUE;
    GST_DEBUG ("xvimagesink is using XShm extension");
  } else
#endif /* HAVE_XSHM */
  {
    context->use_xshm = FALSE;
    GST_DEBUG ("xvimagesink is not using XShm extension");
  }
  if (XkbQueryExtension (context->disp, &opcode, &event, &err, &xkb_major,
          &xkb_minor)) {
    context->use_xkb = TRUE;
    GST_DEBUG ("xvimagesink is using Xkb extension");
  } else {
    context->use_xkb = FALSE;
    GST_DEBUG ("xvimagesink is not using Xkb extension");
  }

  /* Search for XInput extension support */
#ifdef HAVE_XI2
  if (XQueryExtension (context->disp, "XInputExtension",
          &(context->xi_opcode), &event, &err)
      && (XIQueryVersion (context->disp, &xi2_major, &xi2_minor) == Success
          && (xi2_major > 2 || (xi2_major == 2 && xi2_minor >= 2)))) {
    context->use_xi2 = TRUE;
    GST_DEBUG ("xvimagesink is using XInput extension");
  } else
#endif /* HAVE_XI2 */
  {
    context->use_xi2 = FALSE;
    GST_DEBUG ("xvimagesink is not using XInput extension: "
        "XInput is not present");
  }

  xv_attr = XvQueryPortAttributes (context->disp, context->xv_port_id, &N_attr);

  /* Generate the channels list */
  for (i = 0; i < (sizeof (channels) / sizeof (char *)); i++) {
    XvAttribute *matching_attr = NULL;

    /* Retrieve the property atom if it exists. If it doesn't exist,
     * the attribute itself must not either, so we can skip */
    prop_atom = XInternAtom (context->disp, channels[i], True);
    if (prop_atom == None)
      continue;

    if (xv_attr != NULL) {
      for (j = 0; j < N_attr && matching_attr == NULL; ++j)
        if (!g_ascii_strcasecmp (channels[i], xv_attr[j].name))
          matching_attr = xv_attr + j;
    }

    if (matching_attr) {
      GstColorBalanceChannel *channel;

      channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
      channel->label = g_strdup (channels[i]);
      channel->min_value = matching_attr->min_value;
      channel->max_value = matching_attr->max_value;

      context->channels_list = g_list_append (context->channels_list, channel);

      /* If the colorbalance settings have not been touched we get Xv values
         as defaults and update our internal variables */
      if (!config->cb_changed) {
        gint val;

        XvGetPortAttribute (context->disp, context->xv_port_id,
            prop_atom, &val);
        /* Normalize val to [-1000, 1000] */
        val = floor (0.5 + -1000 + 2000 * (val - channel->min_value) /
            (double) (channel->max_value - channel->min_value));

        if (!g_ascii_strcasecmp (channels[i], "XV_HUE"))
          config->hue = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_SATURATION"))
          config->saturation = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_BRIGHTNESS"))
          config->brightness = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_CONTRAST"))
          config->contrast = val;
      }
    }
  }

  if (xv_attr)
    XFree (xv_attr);

  return context;

  /* ERRORS */
no_display:
  {
    gst_xvcontext_unref (context);
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
        "Could not open display %s", config->display_name);
    return NULL;
  }
no_pixel_formats:
  {
    gst_xvcontext_unref (context);
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_SETTINGS,
        ("Could not get pixel formats"));
    return NULL;
  }
no_caps:
  {
    gst_xvcontext_unref (context);
    return NULL;
  }
}

void
gst_xvcontext_set_synchronous (GstXvContext * context, gboolean synchronous)
{
  /* call XSynchronize with the current value of synchronous */
  GST_DEBUG ("XSynchronize called with %s", synchronous ? "TRUE" : "FALSE");
  g_mutex_lock (&context->lock);
  XSynchronize (context->disp, synchronous);
  g_mutex_unlock (&context->lock);
}

void
gst_xvcontext_update_colorbalance (GstXvContext * context,
    GstXvContextConfig * config)
{
  GList *channels = NULL;

  /* Don't set the attributes if they haven't been changed, to avoid
   * rounding errors changing the values */
  if (!config->cb_changed)
    return;

  /* For each channel of the colorbalance we calculate the correct value
     doing range conversion and then set the Xv port attribute to match our
     values. */
  channels = context->channels_list;

  while (channels) {
    if (channels->data && GST_IS_COLOR_BALANCE_CHANNEL (channels->data)) {
      GstColorBalanceChannel *channel = NULL;
      Atom prop_atom;
      gint value = 0;
      gdouble convert_coef;

      channel = GST_COLOR_BALANCE_CHANNEL (channels->data);
      g_object_ref (channel);

      /* Our range conversion coef */
      convert_coef = (channel->max_value - channel->min_value) / 2000.0;

      if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
        value = config->hue;
      } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
        value = config->saturation;
      } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
        value = config->contrast;
      } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
        value = config->brightness;
      } else {
        g_warning ("got an unknown channel %s", channel->label);
        g_object_unref (channel);
        return;
      }

      /* Committing to Xv port */
      g_mutex_lock (&context->lock);
      prop_atom = XInternAtom (context->disp, channel->label, True);
      if (prop_atom != None) {
        int xv_value;
        xv_value =
            floor (0.5 + (value + 1000) * convert_coef + channel->min_value);
        XvSetPortAttribute (context->disp,
            context->xv_port_id, prop_atom, xv_value);
      }
      g_mutex_unlock (&context->lock);

      g_object_unref (channel);
    }
    channels = g_list_next (channels);
  }
}

/* This function tries to get a format matching with a given caps in the
   supported list of formats we generated in gst_xvimagesink_get_xv_support */
gint
gst_xvcontext_get_format_from_info (GstXvContext * context,
    const GstVideoInfo * info)
{
  GList *list = NULL;

  list = context->formats_list;

  while (list) {
    GstXvImageFormat *format = list->data;

    if (format && format->vformat == GST_VIDEO_INFO_FORMAT (info))
      return format->format;

    list = g_list_next (list);
  }
  return -1;
}

void
gst_xvcontext_set_colorimetry (GstXvContext * context,
    GstVideoColorimetry * colorimetry)
{
  Atom prop_atom;
  int xv_value;

  if (!context->have_iturbt709 && !context->have_xvcolorspace)
    return;

  switch (colorimetry->matrix) {
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
    case GST_VIDEO_COLOR_MATRIX_BT709:
      xv_value = 1;
      break;
    default:
      xv_value = 0;
      break;
  }

  g_mutex_lock (&context->lock);
  if (context->have_iturbt709) {
    prop_atom = XInternAtom (context->disp, "XV_ITURBT_709", True);
    if (prop_atom != None) {
      XvSetPortAttribute (context->disp,
          context->xv_port_id, prop_atom, xv_value);
    }
  }

  if (context->have_xvcolorspace) {
    prop_atom = XInternAtom (context->disp, "XV_COLORSPACE", True);
    if (prop_atom != None) {
      XvSetPortAttribute (context->disp,
          context->xv_port_id, prop_atom, xv_value);
    }
  }
  g_mutex_unlock (&context->lock);
}

#ifdef HAVE_XI2
static void
gst_xv_touchdevice_free (GstXvTouchDevice * device)
{
  g_free (device->name);
}
#endif

GstXWindow *
gst_xvcontext_create_xwindow (GstXvContext * context, gint width, gint height)
{
  GstXWindow *window;
  Atom wm_delete;
  Atom hints_atom = None;

  g_return_val_if_fail (GST_IS_XVCONTEXT (context), NULL);

  window = g_new0 (GstXWindow, 1);

  window->context = gst_xvcontext_ref (context);
  window->render_rect.x = window->render_rect.y = 0;
  window->render_rect.w = width;
  window->render_rect.h = height;
  window->have_render_rect = FALSE;

  window->width = width;
  window->height = height;
  window->internal = TRUE;

#ifdef HAVE_XI2
  window->last_touch = 0;
  window->touch_devices = g_array_new (FALSE, FALSE, sizeof (GstXvTouchDevice));
  g_array_set_clear_func (window->touch_devices,
      (GDestroyNotify) gst_xv_touchdevice_free);
#endif

  g_mutex_lock (&context->lock);

  window->win = XCreateSimpleWindow (context->disp,
      context->root, 0, 0, width, height, 0, 0, context->black);

  /* We have to do that to prevent X from redrawing the background on
   * ConfigureNotify. This takes away flickering of video when resizing. */
  XSetWindowBackgroundPixmap (context->disp, window->win, None);

  /* Tell the window manager we'd like delete client messages instead of
   * being killed */
  wm_delete = XInternAtom (context->disp, "WM_DELETE_WINDOW", True);
  if (wm_delete != None) {
    (void) XSetWMProtocols (context->disp, window->win, &wm_delete, 1);
  }

  hints_atom = XInternAtom (context->disp, "_MOTIF_WM_HINTS", True);
  if (hints_atom != None) {
    MotifWmHints *hints;

    hints = g_malloc0 (sizeof (MotifWmHints));

    hints->flags |= MWM_HINTS_DECORATIONS;
    hints->decorations = 1 << 0;

    XChangeProperty (context->disp, window->win,
        hints_atom, hints_atom, 32, PropModeReplace,
        (guchar *) hints, sizeof (MotifWmHints) / sizeof (long));

    XSync (context->disp, FALSE);

    g_free (hints);
  }

  unsigned long pid = getpid ();
  Atom _NET_WM_PID = XInternAtom (context->disp, "_NET_WM_PID", 0);
  XChangeProperty (context->disp, window->win,
      _NET_WM_PID, _NET_WM_PID, 32, 0, (unsigned char *) &pid, 1);

  window->gc = XCreateGC (context->disp, window->win, 0, NULL);

  XMapRaised (context->disp, window->win);

  XSync (context->disp, FALSE);

  g_mutex_unlock (&context->lock);

  return window;
}

GstXWindow *
gst_xvcontext_create_xwindow_from_xid (GstXvContext * context, XID xid)
{
  GstXWindow *window;
  XWindowAttributes attr;

  window = g_new0 (GstXWindow, 1);
  window->win = xid;
  window->context = gst_xvcontext_ref (context);

  /* Set the event we want to receive and create a GC */
  g_mutex_lock (&context->lock);

  XGetWindowAttributes (context->disp, window->win, &attr);

  window->width = attr.width;
  window->height = attr.height;
  window->internal = FALSE;

  window->have_render_rect = FALSE;
  window->render_rect.x = window->render_rect.y = 0;
  window->render_rect.w = attr.width;
  window->render_rect.h = attr.height;

#ifdef HAVE_XI2
  window->last_touch = 0;
  window->touch_devices = g_array_new (FALSE, FALSE, sizeof (GstXvTouchDevice));
  g_array_set_clear_func (window->touch_devices,
      (GDestroyNotify) gst_xv_touchdevice_free);
#endif

  window->gc = XCreateGC (context->disp, window->win, 0, NULL);
  g_mutex_unlock (&context->lock);

  return window;
}

void
gst_xwindow_destroy (GstXWindow * window)
{
  GstXvContext *context;

  g_return_if_fail (window != NULL);

  context = window->context;

  g_mutex_lock (&context->lock);

#ifdef HAVE_XI2
  g_array_free (window->touch_devices, TRUE);
#endif

  /* If we did not create that window we just free the GC and let it live */
  if (window->internal)
    XDestroyWindow (context->disp, window->win);
  else
    XSelectInput (context->disp, window->win, 0);

  XFreeGC (context->disp, window->gc);

  XSync (context->disp, FALSE);

  g_mutex_unlock (&context->lock);

  gst_xvcontext_unref (context);

  g_free (window);
}

#ifdef HAVE_XI2
void
gst_xwindow_select_touch_events (GstXvContext * context, GstXWindow * window)
{
  XIDeviceInfo *devices;
  int ndevices, i, j, mask_len;
  unsigned char *mask;

  window->touch_devices = g_array_remove_range (window->touch_devices,
      0, window->touch_devices->len);

  mask_len = (XI_LASTEVENT + 7) << 3;
  mask = g_new0 (unsigned char, mask_len);
  XISetMask (mask, XI_TouchBegin);
  XISetMask (mask, XI_TouchUpdate);
  XISetMask (mask, XI_TouchEnd);

  devices = XIQueryDevice (window->context->disp, XIAllDevices, &ndevices);

  /* Find suitable touch screen devices, and select touch events for each */
  for (i = 0; i < ndevices; i++) {
    XIEventMask mask_data;
    GstXvTouchDevice temp;
    gboolean has_touch = FALSE;

    if (devices[i].use != XISlavePointer)
      continue;

    temp.pressure_valuator = -1;
    temp.id = devices[i].deviceid;
    temp.name = devices[i].name;

    for (j = 0; j < devices[i].num_classes; j++) {
      XIAnyClassInfo *class = devices[i].classes[j];

      /* only pick devices with direct touch, to avoid touchpads and similar */
      if (class->type == XITouchClass &&
          ((XITouchClassInfo *) class)->mode == XIDirectTouch) {
        has_touch = TRUE;
      }

      /* Find the valuator representing pressure, if present */
      if (class->type == XIValuatorClass) {
        XIValuatorClassInfo *val_info = (XIValuatorClassInfo *) class;

        if (val_info->label == XInternAtom (context->disp,
                "Abs Pressure", TRUE))
          temp.abs_pressure = TRUE;
        else if (val_info->label == XInternAtom (context->disp,
                "Rel Pressure", TRUE))
          temp.abs_pressure = FALSE;
        else
          continue;

        temp.pressure_valuator = i;
        temp.pressure_min = val_info->min;
        temp.pressure_max = val_info->max;
        temp.current_pressure = temp.pressure_min;
      }
    }

    if (has_touch) {
      GST_DEBUG ("found%s touch screen with id %d: %s",
          temp.pressure_valuator < 0 ? "" : (temp.abs_pressure ?
              "pressure-sensitive (abs)" : "pressure-sensitive (rel)"),
          temp.id, temp.name);

      GstXvTouchDevice device = temp;
      device.name = g_strdup (temp.name);

      window->touch_devices =
          g_array_append_val (window->touch_devices, device);

      mask_data.deviceid = temp.id;
      mask_data.mask_len = mask_len;
      mask_data.mask = mask;
      XISelectEvents (context->disp, window->win, &mask_data, 1);
    }
  }

  g_free (mask);
  XIFreeDeviceInfo (devices);
}
#endif

void
gst_xwindow_set_event_handling (GstXWindow * window, gboolean handle_events)
{
  GstXvContext *context;

  g_return_if_fail (window != NULL);

  context = window->context;

  g_mutex_lock (&context->lock);
  if (handle_events) {
    if (window->internal) {
      XSelectInput (context->disp, window->win,
          ExposureMask | StructureNotifyMask | PointerMotionMask |
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask);
    } else {
      XSelectInput (context->disp, window->win,
          ExposureMask | StructureNotifyMask | PointerMotionMask |
          KeyPressMask | KeyReleaseMask);
    }

#ifdef HAVE_XI2
    if (context->use_xi2) {
      XIEventMask mask_data;
      unsigned char mask[2] = { 0, };

      gst_xwindow_select_touch_events (context, window);

      XISetMask (mask, XI_HierarchyChanged);
      mask_data.deviceid = XIAllDevices;
      mask_data.mask_len = sizeof (mask);
      mask_data.mask = mask;

      /* register for HierarchyChanged events to see device changes */
      XISelectEvents (context->disp, window->win, &mask_data, 1);
    }
#endif
  } else {
    XSelectInput (context->disp, window->win, 0);
  }
  g_mutex_unlock (&context->lock);
}

void
gst_xwindow_set_title (GstXWindow * window, const gchar * title)
{
  GstXvContext *context;

  g_return_if_fail (window != NULL);

  context = window->context;

  /* we have a window */
  if (window->internal && title) {
    XTextProperty xproperty;
    XClassHint *hint = XAllocClassHint ();
    Atom _NET_WM_NAME = XInternAtom (context->disp, "_NET_WM_NAME", 0);
    Atom UTF8_STRING = XInternAtom (context->disp, "UTF8_STRING", 0);

    if ((XStringListToTextProperty (((char **) &title), 1, &xproperty)) != 0) {
      XSetWMName (context->disp, window->win, &xproperty);
      XFree (xproperty.value);

      XChangeProperty (context->disp, window->win, _NET_WM_NAME, UTF8_STRING, 8,
          0, (unsigned char *) title, strlen (title));
      XSync (context->disp, False);

      if (hint) {
        hint->res_name = (char *) title;
        hint->res_class = (char *) "GStreamer";
        XSetClassHint (context->disp, window->win, hint);
      }
      XFree (hint);
    }
  }
}

void
gst_xwindow_update_geometry (GstXWindow * window)
{
  XWindowAttributes attr;
  GstXvContext *context;

  g_return_if_fail (window != NULL);

  context = window->context;

  /* Update the window geometry */
  g_mutex_lock (&context->lock);
  XGetWindowAttributes (context->disp, window->win, &attr);

  window->width = attr.width;
  window->height = attr.height;

  if (!window->have_render_rect) {
    window->render_rect.x = window->render_rect.y = 0;
    window->render_rect.w = attr.width;
    window->render_rect.h = attr.height;
  }

  g_mutex_unlock (&context->lock);
}


void
gst_xwindow_clear (GstXWindow * window)
{
  GstXvContext *context;

  g_return_if_fail (window != NULL);

  context = window->context;

  g_mutex_lock (&context->lock);

  XvStopVideo (context->disp, context->xv_port_id, window->win);

  XSync (context->disp, FALSE);

  g_mutex_unlock (&context->lock);
}

void
gst_xwindow_set_render_rectangle (GstXWindow * window,
    gint x, gint y, gint width, gint height)
{
  g_return_if_fail (window != NULL);

  if (width >= 0 && height >= 0) {
    window->render_rect.x = x;
    window->render_rect.y = y;
    window->render_rect.w = width;
    window->render_rect.h = height;
    window->have_render_rect = TRUE;
  } else {
    window->render_rect.x = 0;
    window->render_rect.y = 0;
    window->render_rect.w = window->width;
    window->render_rect.h = window->height;
    window->have_render_rect = FALSE;
  }
}
