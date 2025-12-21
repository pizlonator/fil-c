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

/**
 * SECTION:element-xvimagesink
 * @title: xvimagesink
 *
 * XvImageSink renders video frames to a drawable (XWindow) on a local display
 * using the XVideo extension. Rendering to a remote display is theoretically
 * possible but i doubt that the XVideo extension is actually available when
 * connecting to a remote display. This element can receive a Window ID from the
 * application through the #GstVideoOverlay interface and will then render
 * video frames in this drawable. If no Window ID was provided by the
 * application, the element will create its own internal window and render
 * into it.
 *
 * ## Scaling
 *
 * The XVideo extension, when it's available, handles hardware accelerated
 * scaling of video frames. This means that the element will just accept
 * incoming video frames no matter their geometry and will then put them to the
 * drawable scaling them on the fly. Using the #GstXvImageSink:force-aspect-ratio
 * property it is possible to enforce scaling with a constant aspect ratio,
 * which means drawing black borders around the video frame.
 *
 * ## Events
 *
 * XvImageSink creates a thread to handle events coming from the drawable. There
 * are several kind of events that can be grouped in 2 big categories: input
 * events and window state related events. Input events will be translated to
 * navigation events and pushed upstream for other elements to react on them.
 * This includes events such as pointer moves, key press/release, clicks etc...
 * Other events are used to handle the drawable appearance even when the data
 * is not flowing (GST_STATE_PAUSED). That means that even when the element is
 * paused, it will receive expose events from the drawable and draw the latest
 * frame with correct borders/aspect-ratio.
 *
 * ## Pixel aspect ratio
 *
 * When changing state to GST_STATE_READY, XvImageSink will open a connection to
 * the display specified in the #GstXvImageSink:display property or the
 * default display if nothing specified. Once this connection is open it will
 * inspect the display configuration including the physical display geometry and
 * then calculate the pixel aspect ratio. When receiving video frames with a
 * different pixel aspect ratio, XvImageSink will use hardware scaling to
 * display the video frames correctly on display's pixel aspect ratio.
 * Sometimes the calculated pixel aspect ratio can be wrong, it is
 * then possible to enforce a specific pixel aspect ratio using the
 * #GstXvImageSink:pixel-aspect-ratio property.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v videotestsrc ! xvimagesink
 * ]|
 *  A pipeline to test hardware scaling.
 * When the test video signal appears you can resize the window and see that
 * video frames are scaled through hardware (no extra CPU cost). By default
 * the image will never be distorted when scaled, instead black borders will
 * be added if needed.
 * |[
 * gst-launch-1.0 -v videotestsrc ! xvimagesink force-aspect-ratio=false
 * ]|
 *  Same pipeline with #GstXvImageSink:force-aspect-ratio property set to
 * false. You can observe that no borders are drawn around the scaled image
 * now and it will be distorted to fill the entire frame instead of respecting
 * the aspect ratio.
 * |[
 * gst-launch-1.0 -v videotestsrc ! navigationtest ! xvimagesink
 * ]|
 *  A pipeline to test navigation events.
 * While moving the mouse pointer over the test signal you will see a black box
 * following the mouse pointer. If you press the mouse button somewhere on the
 * video and release it somewhere else a green box will appear where you pressed
 * the button and a red one where you released it. (The navigationtest element
 * is part of gst-plugins-good.) You can observe here that even if the images
 * are scaled through hardware the pointer coordinates are converted back to the
 * original video frame geometry so that the box can be drawn to the correct
 * position. This also handles borders correctly, limiting coordinates to the
 * image area
 * |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw, pixel-aspect-ratio=4/3 ! xvimagesink
 * ]|
 *  This is faking a 4/3 pixel aspect ratio caps on video frames produced by
 * videotestsrc, in most cases the pixel aspect ratio of the display will be
 * 1/1. This means that XvImageSink will have to do the scaling to convert
 * incoming frames to a size that will match the display pixel aspect ratio
 * (from 320x240 to 320x180 in this case).
 * |[
 * gst-launch-1.0 -v videotestsrc ! xvimagesink hue=100 saturation=-100 brightness=100
 * ]|
 *  Demonstrates how to use the colorbalance interface.
 *
 */

/* for developers: there are two useful tools : xvinfo and xvattr */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Our interfaces */
#include <gst/video/navigation.h>
#include <gst/video/videooverlay.h>
#include <gst/video/colorbalance.h>
/* Helper functions */
#include <gst/video/gstvideometa.h>

/* Object header */
#include "xvimagesink.h"
#include "xvimageallocator.h"

/* Debugging category */
#include <gst/gstinfo.h>

/* for XkbKeycodeToKeysym */
#include <X11/XKBlib.h>

/* for touchscreen events */
#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#endif

GST_DEBUG_CATEGORY_EXTERN (gst_debug_xv_context);
GST_DEBUG_CATEGORY_EXTERN (gst_debug_xv_image_pool);
GST_DEBUG_CATEGORY (gst_debug_xv_image_sink);

#define GST_CAT_DEFAULT gst_debug_xv_image_sink
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

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

static gboolean gst_xv_image_sink_open (GstXvImageSink * xvimagesink);
static void gst_xv_image_sink_close (GstXvImageSink * xvimagesink);
static void gst_xv_image_sink_xwindow_update_geometry (GstXvImageSink *
    xvimagesink);
static void gst_xv_image_sink_expose (GstVideoOverlay * overlay);

/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_xv_image_sink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

enum
{
  PROP_0,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION,
  PROP_DISPLAY,
  PROP_SYNCHRONOUS,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FORCE_ASPECT_RATIO,
  PROP_HANDLE_EVENTS,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_HANDLE_EXPOSE,
  PROP_DOUBLE_BUFFER,
  PROP_AUTOPAINT_COLORKEY,
  PROP_COLORKEY,
  PROP_DRAW_BORDERS,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_HEIGHT,
  PROP_LAST
};

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*          Object typing & Creation           */
/*                                             */
/* =========================================== */
static void gst_xv_image_sink_navigation_init (GstNavigationInterface * iface);
static void gst_xv_image_sink_video_overlay_init (GstVideoOverlayInterface *
    iface);
static void gst_xv_image_sink_colorbalance_init (GstColorBalanceInterface *
    iface);
#define gst_xv_image_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstXvImageSink, gst_xv_image_sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_xv_image_sink_navigation_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_xv_image_sink_video_overlay_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_xv_image_sink_colorbalance_init));
#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_debug_xv_context, "xcontext", 0, "xcontext miniobject");\
  GST_DEBUG_CATEGORY_INIT (gst_debug_xv_image_sink, "xvimagesink", 0, "ximagesink element");\
  GST_DEBUG_CATEGORY_INIT (gst_debug_xv_image_pool, "xvimagepool", 0, "ximagepool object");\

GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (xvimagesink, "xvimagesink",
    GST_RANK_PRIMARY, GST_TYPE_XV_IMAGE_SINK, _do_init);

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */


/* This function puts a GstXvImage on a GstXvImageSink's window. Returns FALSE
 * if no window was available  */
static gboolean
gst_xv_image_sink_xvimage_put (GstXvImageSink * xvimagesink,
    GstBuffer * xvimage)
{
  GstXvImageMemory *mem;
  GstVideoCropMeta *crop;
  GstVideoRectangle result;
  gboolean draw_border = FALSE;
  GstVideoRectangle src = { 0, };
  GstVideoRectangle dst = { 0, };
  GstVideoRectangle mem_crop;
  GstXWindow *xwindow;

  /* We take the flow_lock. If expose is in there we don't want to run
     concurrently from the data flow thread */
  g_mutex_lock (&xvimagesink->flow_lock);

  if (G_UNLIKELY ((xwindow = xvimagesink->xwindow) == NULL)) {
    g_mutex_unlock (&xvimagesink->flow_lock);
    return FALSE;
  }

  /* Draw borders when displaying the first frame. After this
     draw borders only on expose event or after a size change. */
  if (!xvimagesink->cur_image || xvimagesink->redraw_border) {
    draw_border = xvimagesink->draw_borders;
    xvimagesink->redraw_border = FALSE;
  }

  /* Store a reference to the last image we put, lose the previous one */
  if (xvimage && xvimagesink->cur_image != xvimage) {
    if (xvimagesink->cur_image) {
      GST_LOG_OBJECT (xvimagesink, "unreffing %p", xvimagesink->cur_image);
      gst_buffer_unref (xvimagesink->cur_image);
    }
    GST_LOG_OBJECT (xvimagesink, "reffing %p as our current image", xvimage);
    xvimagesink->cur_image = gst_buffer_ref (xvimage);
  }

  /* Expose sends a NULL image, we take the latest frame */
  if (!xvimage) {
    if (xvimagesink->cur_image) {
      draw_border = TRUE;
      xvimage = xvimagesink->cur_image;
    } else {
      g_mutex_unlock (&xvimagesink->flow_lock);
      return TRUE;
    }
  }

  mem = (GstXvImageMemory *) gst_buffer_peek_memory (xvimage, 0);
  gst_xvimage_memory_get_crop (mem, &mem_crop);

  crop = gst_buffer_get_video_crop_meta (xvimage);

  if (crop) {
    src.x = crop->x + mem_crop.x;
    src.y = crop->y + mem_crop.y;
    src.w = crop->width;
    src.h = crop->height;
    GST_LOG_OBJECT (xvimagesink,
        "crop %dx%d-%dx%d", crop->x, crop->y, crop->width, crop->height);
  } else {
    src = mem_crop;
  }

  if (xvimagesink->keep_aspect) {
    GstVideoRectangle s;

    /* We take the size of the source material as it was negotiated and
     * corrected for DAR. This size can be different from the cropped size in
     * which case the image will be scaled to fit the negotiated size. */
    s.w = GST_VIDEO_SINK_WIDTH (xvimagesink);
    s.h = GST_VIDEO_SINK_HEIGHT (xvimagesink);
    dst.w = xwindow->render_rect.w;
    dst.h = xwindow->render_rect.h;

    gst_video_sink_center_rect (s, dst, &result, TRUE);
    result.x += xwindow->render_rect.x;
    result.y += xwindow->render_rect.y;
  } else {
    memcpy (&result, &xwindow->render_rect, sizeof (GstVideoRectangle));
  }

  gst_xvimage_memory_render (mem, &src, xwindow, &result, draw_border);

  g_mutex_unlock (&xvimagesink->flow_lock);

  return TRUE;
}

static void
gst_xv_image_sink_xwindow_set_title (GstXvImageSink * xvimagesink,
    GstXWindow * xwindow, const gchar * media_title)
{
  if (media_title) {
    g_free (xvimagesink->media_title);
    xvimagesink->media_title = g_strdup (media_title);
  }
  if (xwindow) {
    /* we have a window */
    const gchar *app_name;
    const gchar *title = NULL;
    gchar *title_mem = NULL;

    /* set application name as a title */
    app_name = g_get_application_name ();

    if (app_name && xvimagesink->media_title) {
      title = title_mem = g_strconcat (xvimagesink->media_title, " : ",
          app_name, NULL);
    } else if (app_name) {
      title = app_name;
    } else if (xvimagesink->media_title) {
      title = xvimagesink->media_title;
    }

    gst_xwindow_set_title (xwindow, title);
    g_free (title_mem);
  }
}

/* This function handles a GstXWindow creation
 * The width and height are the actual pixel size on the display */
static GstXWindow *
gst_xv_image_sink_xwindow_new (GstXvImageSink * xvimagesink,
    gint width, gint height)
{
  GstXWindow *xwindow = NULL;
  GstXvContext *context;

  g_return_val_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink), NULL);

  context = xvimagesink->context;

  xwindow = gst_xvcontext_create_xwindow (context, width, height);

  /* set application name as a title */
  gst_xv_image_sink_xwindow_set_title (xvimagesink, xwindow, NULL);

  gst_xwindow_set_event_handling (xwindow, xvimagesink->handle_events);

  gst_video_overlay_got_window_handle (GST_VIDEO_OVERLAY (xvimagesink),
      xwindow->win);

  return xwindow;
}

static void
gst_xv_image_sink_xwindow_update_geometry (GstXvImageSink * xvimagesink)
{
  g_return_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink));

  /* Update the window geometry */
  g_mutex_lock (&xvimagesink->flow_lock);
  if (G_LIKELY (xvimagesink->xwindow))
    gst_xwindow_update_geometry (xvimagesink->xwindow);
  g_mutex_unlock (&xvimagesink->flow_lock);
}

/* This function commits our internal colorbalance settings to our grabbed Xv
   port. If the context is not initialized yet it simply returns */
static void
gst_xv_image_sink_update_colorbalance (GstXvImageSink * xvimagesink)
{
  GstXvContext *context;

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink));

  /* If we haven't initialized the X context we can't update anything */
  if ((context = xvimagesink->context) == NULL)
    return;

  gst_xvcontext_update_colorbalance (context, &xvimagesink->config);
}

/* This function handles XEvents that might be in the queue. It generates
   GstEvent that will be sent upstream in the pipeline to handle interactivity
   and navigation. It will also listen for configure events on the window to
   trigger caps renegotiation so on the fly software scaling can work. */
static void
gst_xv_image_sink_handle_xevents (GstXvImageSink * xvimagesink)
{
  XEvent e;
  gint pointer_x = 0, pointer_y = 0;
  gboolean pointer_moved = FALSE;
  gboolean exposed = FALSE, configured = FALSE;
#ifdef HAVE_XI2
  gboolean touch_frame_open = FALSE;
  GstNavigationModifierType state = GST_NAVIGATION_MODIFIER_NONE;
#endif

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink));

  /* Handle Interaction, produces navigation events */

  /* We get all pointer motion events, only the last position is
     interesting. */
  g_mutex_lock (&xvimagesink->flow_lock);
  g_mutex_lock (&xvimagesink->context->lock);
  while (XCheckWindowEvent (xvimagesink->context->disp,
          xvimagesink->xwindow->win, PointerMotionMask, &e)) {
    g_mutex_unlock (&xvimagesink->context->lock);
    g_mutex_unlock (&xvimagesink->flow_lock);

    switch (e.type) {
      case MotionNotify:
        pointer_x = e.xmotion.x;
        pointer_y = e.xmotion.y;
        pointer_moved = TRUE;
        break;
      default:
        break;
    }
    g_mutex_lock (&xvimagesink->flow_lock);
    g_mutex_lock (&xvimagesink->context->lock);
  }

  if (pointer_moved) {
    g_mutex_unlock (&xvimagesink->context->lock);
    g_mutex_unlock (&xvimagesink->flow_lock);

    GST_DEBUG ("xvimagesink pointer moved over window at %d,%d",
        pointer_x, pointer_y);
    gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
        gst_navigation_event_new_mouse_move (e.xbutton.x, e.xbutton.y,
            e.xbutton.state & GST_NAVIGATION_MODIFIER_MASK));

    g_mutex_lock (&xvimagesink->flow_lock);
    g_mutex_lock (&xvimagesink->context->lock);
  }

  /* We get all events on our window to throw them upstream */
  while (XCheckWindowEvent (xvimagesink->context->disp,
          xvimagesink->xwindow->win,
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask,
          &e)) {
    KeySym keysym;
    const char *key_str = NULL;

    /* We lock only for the X function call */
    g_mutex_unlock (&xvimagesink->context->lock);
    g_mutex_unlock (&xvimagesink->flow_lock);

    switch (e.type) {
      case ButtonPress:
        /* Mouse button pressed over our window. We send upstream
           events for interactivity/navigation */
        GST_DEBUG ("xvimagesink button %d pressed over window at %d,%d",
            e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
            gst_navigation_event_new_mouse_button_press (e.xbutton.button,
                e.xbutton.x, e.xbutton.y,
                e.xbutton.state & GST_NAVIGATION_MODIFIER_MASK));
        break;
      case ButtonRelease:
        /* Mouse button released over our window. We send upstream
           events for interactivity/navigation */
        GST_DEBUG ("xvimagesink button %d released over window at %d,%d",
            e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
            gst_navigation_event_new_mouse_button_release (e.xbutton.button,
                e.xbutton.x, e.xbutton.y,
                e.xbutton.state & GST_NAVIGATION_MODIFIER_MASK));
        break;
      case KeyPress:
      case KeyRelease:
        /* Key pressed/released over our window. We send upstream
           events for interactivity/navigation */
        g_mutex_lock (&xvimagesink->context->lock);
        if (xvimagesink->context->use_xkb) {
          keysym = XkbKeycodeToKeysym (xvimagesink->context->disp,
              e.xkey.keycode, 0, 0);
        } else {
          keysym = XKeycodeToKeysym (xvimagesink->context->disp,
              e.xkey.keycode, 0);
        }
        if (keysym != NoSymbol) {
          key_str = XKeysymToString (keysym);
        } else {
          key_str = "unknown";
        }
        g_mutex_unlock (&xvimagesink->context->lock);
        GST_DEBUG_OBJECT (xvimagesink,
            "key %d pressed over window at %d,%d (%s)",
            e.xkey.keycode, e.xkey.x, e.xkey.y, key_str);
        gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
            (e.type == KeyPress) ?
            gst_navigation_event_new_key_press (key_str,
                e.xkey.state & GST_NAVIGATION_MODIFIER_MASK) :
            gst_navigation_event_new_key_release (key_str,
                e.xkey.state & GST_NAVIGATION_MODIFIER_MASK));
        break;
      default:
        GST_DEBUG_OBJECT (xvimagesink, "xvimagesink unhandled X event (%d)",
            e.type);
    }
    g_mutex_lock (&xvimagesink->flow_lock);
    g_mutex_lock (&xvimagesink->context->lock);
  }

  /* Handle Expose */
  while (XCheckWindowEvent (xvimagesink->context->disp,
          xvimagesink->xwindow->win, ExposureMask | StructureNotifyMask, &e)) {
    switch (e.type) {
      case Expose:
        exposed = TRUE;
        break;
      case ConfigureNotify:
        g_mutex_unlock (&xvimagesink->context->lock);
        g_mutex_unlock (&xvimagesink->flow_lock);

        gst_xv_image_sink_xwindow_update_geometry (xvimagesink);

        g_mutex_lock (&xvimagesink->flow_lock);
        g_mutex_lock (&xvimagesink->context->lock);
        configured = TRUE;
        break;
      default:
        break;
    }
  }

  if (xvimagesink->handle_expose && (exposed || configured)) {
    g_mutex_unlock (&xvimagesink->context->lock);
    g_mutex_unlock (&xvimagesink->flow_lock);

    gst_xv_image_sink_expose (GST_VIDEO_OVERLAY (xvimagesink));

    g_mutex_lock (&xvimagesink->flow_lock);
    g_mutex_lock (&xvimagesink->context->lock);
  }

  while (XPending (xvimagesink->context->disp)) {
    XNextEvent (xvimagesink->context->disp, &e);

    switch (e.type) {
        /* Handle Display events */
      case ClientMessage:{
        Atom wm_delete;

        wm_delete = XInternAtom (xvimagesink->context->disp,
            "WM_DELETE_WINDOW", True);
        if (wm_delete != None && wm_delete == (Atom) e.xclient.data.l[0]) {
          /* Handle window deletion by posting an error on the bus */
          GST_ELEMENT_ERROR (xvimagesink, RESOURCE, NOT_FOUND,
              ("Output window was closed"), (NULL));

          g_mutex_unlock (&xvimagesink->context->lock);
          gst_xwindow_destroy (xvimagesink->xwindow);
          xvimagesink->xwindow = NULL;
          g_mutex_lock (&xvimagesink->context->lock);
        }
        break;
      }
      default:{
#ifdef HAVE_XI2
        /* Handle XInput2 touch screen events */
        if (xvimagesink->context->use_xi2
            && XGetEventData (xvimagesink->context->disp,
                (XGenericEventCookie *) & e)
            && e.xcookie.extension == xvimagesink->context->xi_opcode) {
          XGenericEventCookie *cookie;
          XIDeviceEvent *touch;
          GstXvTouchDevice device;
          GstXWindow *xwindow;
          GstEvent *nav;
          gdouble pressure;
          gboolean device_found;
          unsigned int ev_id, i;

          cookie = &e.xcookie;
          xwindow = xvimagesink->xwindow;
          nav = NULL;

          if (cookie->evtype == XI_HierarchyChanged) {
            GST_DEBUG ("ximagesink devices changed, searching for "
                "touch devices again");
            gst_xwindow_select_touch_events (xvimagesink->context, xwindow);
            break;
          }

          if (cookie->evtype != XI_TouchBegin
              && cookie->evtype != XI_TouchUpdate
              && cookie->evtype != XI_TouchEnd)
            break;

          touch = cookie->data;
          ev_id = ((unsigned int) touch->deviceid) << 16 |
              (((unsigned int) touch->detail) & 0x00ff);

          /* find device that the event belongs to */
          device_found = FALSE;
          for (i = 0; i < xwindow->touch_devices->len; i++) {
            device = g_array_index (xwindow->touch_devices,
                GstXvTouchDevice, i);
            if (device.id == touch->deviceid) {
              device_found = TRUE;
              break;
            }
          }
          if (!device_found)
            break;

          if (device.pressure_valuator >= 0) {
            pressure = touch->valuators.values[device.pressure_valuator];
            pressure -= device.pressure_min;
            pressure /= device.pressure_max - device.pressure_min;
            if (!device.abs_pressure)
              pressure += device.current_pressure;
          } else
            pressure = NAN;

          device.current_pressure = pressure;

          g_mutex_unlock (&xvimagesink->context->lock);
          g_mutex_unlock (&xvimagesink->flow_lock);

          state = touch->mods.effective;
          /* assume the event queue is ordered chronologically, and end */
          /* the previous touch event frame when the timestamp increases */
          if (touch->time != xwindow->last_touch && touch_frame_open) {
            if (touch->time < xwindow->last_touch) {
              GST_WARNING ("ximagesink out of order touch event "
                  "with timestamp %lu, not ending touch frame", touch->time);
            } else {
              GST_DEBUG ("ximagesink ending touch frame for %lu",
                  xwindow->last_touch);
              gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
                  gst_navigation_event_new_touch_frame (state));
            }
          }

          switch (cookie->evtype) {
            case XI_TouchBegin:{
              GST_DEBUG ("xvimagesink new touch point %d on device %d "
                  "at %.0f,%.0f", touch->detail, touch->deviceid,
                  touch->event_x, touch->event_y);
              nav = gst_navigation_event_new_touch_down (ev_id,
                  touch->event_x, touch->event_y, pressure, state);
              break;
            }
            case XI_TouchEnd:{
              GST_DEBUG ("xvimagesink removed touch point %d from device %d "
                  "at %.0f,%.0f", touch->detail, touch->deviceid,
                  touch->event_x, touch->event_y);
              nav = gst_navigation_event_new_touch_up (ev_id,
                  touch->event_x, touch->event_y, state);
              break;
            }
            case XI_TouchUpdate:{
              GST_DEBUG ("xvimagesink touch point %d on device %d moved "
                  "to %.0f,%.0f", touch->detail, touch->deviceid,
                  touch->event_x, touch->event_y);
              nav = gst_navigation_event_new_touch_motion (ev_id,
                  touch->event_x, touch->event_y, pressure, state);
              break;
            }
            default:
              break;
          }

          gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink), nav);
          xvimagesink->xwindow->last_touch = touch->time;
          touch_frame_open = TRUE;

          g_mutex_lock (&xvimagesink->flow_lock);
          g_mutex_lock (&xvimagesink->context->lock);
          XFreeEventData (xvimagesink->context->disp, cookie);
        }
#endif
        break;
      }
    }
  }

  g_mutex_unlock (&xvimagesink->context->lock);
  g_mutex_unlock (&xvimagesink->flow_lock);

#ifdef HAVE_XI2
  /* send a touch-frame event now to avoid delay from having to wait for the */
  /* next invocation */
  if (touch_frame_open) {
    GST_DEBUG ("xvimagesink ending touch frame for %lu",
        xvimagesink->xwindow->last_touch);
    gst_navigation_send_event_simple (GST_NAVIGATION (xvimagesink),
        gst_navigation_event_new_touch_frame (state));
  }
#endif
}

static gpointer
gst_xv_image_sink_event_thread (GstXvImageSink * xvimagesink)
{
  g_return_val_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink), NULL);

  GST_OBJECT_LOCK (xvimagesink);
  while (xvimagesink->running) {
    GST_OBJECT_UNLOCK (xvimagesink);

    if (xvimagesink->xwindow) {
      gst_xv_image_sink_handle_xevents (xvimagesink);
    }
    /* FIXME: do we want to align this with the framerate or anything else? */
    g_usleep (G_USEC_PER_SEC / 20);

    GST_OBJECT_LOCK (xvimagesink);
  }
  GST_OBJECT_UNLOCK (xvimagesink);

  return NULL;
}

static void
gst_xv_image_sink_manage_event_thread (GstXvImageSink * xvimagesink)
{
  GThread *thread = NULL;

  /* don't start the thread too early */
  if (xvimagesink->context == NULL) {
    return;
  }

  GST_OBJECT_LOCK (xvimagesink);
  if (xvimagesink->handle_expose || xvimagesink->handle_events) {
    if (!xvimagesink->event_thread) {
      /* Setup our event listening thread */
      GST_DEBUG_OBJECT (xvimagesink, "run xevent thread, expose %d, events %d",
          xvimagesink->handle_expose, xvimagesink->handle_events);
      xvimagesink->running = TRUE;
      xvimagesink->event_thread = g_thread_try_new ("xvimagesink-events",
          (GThreadFunc) gst_xv_image_sink_event_thread, xvimagesink, NULL);
    }
  } else {
    if (xvimagesink->event_thread) {
      GST_DEBUG_OBJECT (xvimagesink, "stop xevent thread, expose %d, events %d",
          xvimagesink->handle_expose, xvimagesink->handle_events);
      xvimagesink->running = FALSE;
      /* grab thread and mark it as NULL */
      thread = xvimagesink->event_thread;
      xvimagesink->event_thread = NULL;
    }
  }
  GST_OBJECT_UNLOCK (xvimagesink);

  /* Wait for our event thread to finish */
  if (thread)
    g_thread_join (thread);

}

/* Element stuff */

static GstCaps *
gst_xv_image_sink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstXvImageSink *xvimagesink;
  GstCaps *caps;

  xvimagesink = GST_XV_IMAGE_SINK (bsink);

  if (xvimagesink->context) {
    if (filter)
      return gst_caps_intersect_full (filter, xvimagesink->context->caps,
          GST_CAPS_INTERSECT_FIRST);
    else
      return gst_caps_ref (xvimagesink->context->caps);
  }

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (xvimagesink));
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  return caps;
}

static GstBufferPool *
gst_xv_image_sink_create_pool (GstXvImageSink * xvimagesink, GstCaps * caps,
    gsize size, gint min)
{
  GstBufferPool *pool;
  GstStructure *config;

  pool = gst_xvimage_buffer_pool_new (xvimagesink->allocator);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, 0);

  if (!gst_buffer_pool_set_config (pool, config))
    goto config_failed;

  return pool;

config_failed:
  {
    GST_ERROR_OBJECT (xvimagesink, "failed to set config.");
    gst_object_unref (pool);
    return NULL;
  }
}

static gboolean
gst_xv_image_sink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstXvImageSink *xvimagesink;
  GstXvContext *context;
  GstBufferPool *oldpool;
  GstVideoInfo info;
  guint32 im_format = 0;
  gint video_par_n, video_par_d;        /* video's PAR */
  gint display_par_n, display_par_d;    /* display's PAR */
  guint num, den;

  xvimagesink = GST_XV_IMAGE_SINK (bsink);
  context = xvimagesink->context;

  GST_DEBUG_OBJECT (xvimagesink,
      "In setcaps. Possible caps %" GST_PTR_FORMAT ", setting caps %"
      GST_PTR_FORMAT, context->caps, caps);

  if (!gst_caps_can_intersect (context->caps, caps))
    goto incompatible_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;

  xvimagesink->fps_n = info.fps_n;
  xvimagesink->fps_d = info.fps_d;

  xvimagesink->video_width = info.width;
  xvimagesink->video_height = info.height;

  im_format = gst_xvcontext_get_format_from_info (context, &info);
  if (im_format == -1)
    goto invalid_format;

  gst_xvcontext_set_colorimetry (context, &info.colorimetry);

  /* get aspect ratio from caps if it's present, and
   * convert video width and height to a display width and height
   * using wd / hd = wv / hv * PARv / PARd */

  /* get video's PAR */
  video_par_n = info.par_n;
  video_par_d = info.par_d;

  /* get display's PAR */
  if (xvimagesink->par) {
    display_par_n = gst_value_get_fraction_numerator (xvimagesink->par);
    display_par_d = gst_value_get_fraction_denominator (xvimagesink->par);
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  if (!gst_video_calculate_display_ratio (&num, &den, info.width,
          info.height, video_par_n, video_par_d, display_par_n, display_par_d))
    goto no_disp_ratio;

  GST_DEBUG_OBJECT (xvimagesink,
      "video width/height: %dx%d, calculated display ratio: %d/%d",
      info.width, info.height, num, den);

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = num / den */

  /* start with same height, because of interlaced video */
  /* check hd / den is an integer scale factor, and scale wd with the PAR */
  if (info.height % den == 0) {
    GST_DEBUG_OBJECT (xvimagesink, "keeping video height");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = (guint)
        gst_util_uint64_scale_int (info.height, num, den);
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = info.height;
  } else if (info.width % num == 0) {
    GST_DEBUG_OBJECT (xvimagesink, "keeping video width");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = info.width;
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = (guint)
        gst_util_uint64_scale_int (info.width, den, num);
  } else {
    GST_DEBUG_OBJECT (xvimagesink, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = (guint)
        gst_util_uint64_scale_int (info.height, num, den);
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = info.height;
  }
  GST_DEBUG_OBJECT (xvimagesink, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (xvimagesink), GST_VIDEO_SINK_HEIGHT (xvimagesink));

  /* Notify application to set xwindow id now */
  g_mutex_lock (&xvimagesink->flow_lock);
  if (!xvimagesink->xwindow) {
    g_mutex_unlock (&xvimagesink->flow_lock);
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (xvimagesink));
  } else {
    g_mutex_unlock (&xvimagesink->flow_lock);
  }

  /* Creating our window and our image with the display size in pixels */
  if (GST_VIDEO_SINK_WIDTH (xvimagesink) <= 0 ||
      GST_VIDEO_SINK_HEIGHT (xvimagesink) <= 0)
    goto no_display_size;

  g_mutex_lock (&xvimagesink->flow_lock);
  if (!xvimagesink->xwindow) {
    xvimagesink->xwindow = gst_xv_image_sink_xwindow_new (xvimagesink,
        GST_VIDEO_SINK_WIDTH (xvimagesink),
        GST_VIDEO_SINK_HEIGHT (xvimagesink));
  }

  if (xvimagesink->pending_render_rect) {
    xvimagesink->pending_render_rect = FALSE;
    gst_xwindow_set_render_rectangle (xvimagesink->xwindow,
        xvimagesink->render_rect.x, xvimagesink->render_rect.y,
        xvimagesink->render_rect.w, xvimagesink->render_rect.h);
  }

  xvimagesink->info = info;

  /* After a resize, we want to redraw the borders in case the new frame size
   * doesn't cover the same area */
  xvimagesink->redraw_border = TRUE;

  /* destroy current pool */
  oldpool = xvimagesink->pool;
  xvimagesink->pool = NULL;
  g_mutex_unlock (&xvimagesink->flow_lock);

  /* deactivate and unref the old internal pool */
  if (oldpool) {
    gst_buffer_pool_set_active (oldpool, FALSE);
    gst_object_unref (oldpool);
  }

  return TRUE;

  /* ERRORS */
incompatible_caps:
  {
    GST_ERROR_OBJECT (xvimagesink, "caps incompatible");
    return FALSE;
  }
invalid_format:
  {
    GST_DEBUG_OBJECT (xvimagesink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
no_disp_ratio:
  {
    GST_ELEMENT_ERROR (xvimagesink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
no_display_size:
  {
    GST_ELEMENT_ERROR (xvimagesink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display size of the video."));
    return FALSE;
  }
}

static GstStateChangeReturn
gst_xv_image_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XV_IMAGE_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_xv_image_sink_open (xvimagesink))
        goto error;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      xvimagesink->fps_n = 0;
      xvimagesink->fps_d = 1;
      GST_VIDEO_SINK_WIDTH (xvimagesink) = 0;
      GST_VIDEO_SINK_HEIGHT (xvimagesink) = 0;
      g_mutex_lock (&xvimagesink->flow_lock);
      if (xvimagesink->pool)
        gst_buffer_pool_set_active (xvimagesink->pool, FALSE);
      g_mutex_unlock (&xvimagesink->flow_lock);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_xv_image_sink_close (xvimagesink);
      break;
    default:
      break;
  }
  return ret;

error:
  {
    return GST_STATE_CHANGE_FAILURE;
  }
}

static void
gst_xv_image_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XV_IMAGE_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (xvimagesink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, xvimagesink->fps_d,
            xvimagesink->fps_n);
      }
    }
  }
}

static GstFlowReturn
gst_xv_image_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstFlowReturn res;
  GstXvImageSink *xvimagesink;
  GstBuffer *to_put = NULL;
  GstMemory *mem;

  xvimagesink = GST_XV_IMAGE_SINK (vsink);

  if (gst_buffer_n_memory (buf) == 1 && (mem = gst_buffer_peek_memory (buf, 0))
      && gst_xvimage_memory_is_from_context (mem, xvimagesink->context)) {
    /* If this buffer has been allocated using our buffer management we simply
       put the ximage which is in the PRIVATE pointer */
    GST_LOG_OBJECT (xvimagesink, "buffer %p from our pool, writing directly",
        buf);
    to_put = buf;
    res = GST_FLOW_OK;
  } else {
    GstVideoFrame src, dest;
    GstBufferPoolAcquireParams params = { 0, };
    GstVideoCropMeta *crop_meta;

    /* Else we have to copy the data into our private image, */
    /* if we have one... */
    GST_LOG_OBJECT (xvimagesink, "buffer %p not from our pool, copying", buf);

    if (xvimagesink->pool == NULL) {
      GstCaps *caps = gst_video_info_to_caps (&xvimagesink->info);

      GST_DEBUG_OBJECT (xvimagesink, "create new pool");
      xvimagesink->pool = gst_xv_image_sink_create_pool (xvimagesink, caps,
          xvimagesink->info.size, 2);
      if (xvimagesink->pool == NULL)
        goto no_pool;

      gst_caps_unref (caps);
    }

    if (!gst_buffer_pool_set_active (xvimagesink->pool, TRUE))
      goto activate_failed;

    /* take a buffer from our pool, if there is no buffer in the pool something
     * is seriously wrong, waiting for the pool here might deadlock when we try
     * to go to PAUSED because we never flush the pool then. */
    params.flags = GST_BUFFER_POOL_ACQUIRE_FLAG_DONTWAIT;
    res = gst_buffer_pool_acquire_buffer (xvimagesink->pool, &to_put, &params);
    if (res != GST_FLOW_OK)
      goto no_buffer;

    GST_CAT_LOG_OBJECT (GST_CAT_PERFORMANCE, xvimagesink,
        "slow copy buffer %p into bufferpool buffer %p", buf, to_put);

    if (!gst_video_frame_map (&src, &xvimagesink->info, buf, GST_MAP_READ))
      goto invalid_buffer;

    if (!gst_video_frame_map (&dest, &xvimagesink->info, to_put, GST_MAP_WRITE)) {
      gst_video_frame_unmap (&src);
      goto invalid_buffer;
    }

    gst_video_frame_copy (&dest, &src);

    gst_video_frame_unmap (&dest);
    gst_video_frame_unmap (&src);

    if ((crop_meta = gst_buffer_get_video_crop_meta (buf))) {
      GstVideoCropMeta *dmeta = gst_buffer_add_video_crop_meta (to_put);

      dmeta->x = crop_meta->x;
      dmeta->y = crop_meta->y;
      dmeta->width = crop_meta->width;
      dmeta->height = crop_meta->height;
    }
  }

  if (!gst_xv_image_sink_xvimage_put (xvimagesink, to_put))
    goto no_window;

done:
  if (to_put != buf)
    gst_buffer_unref (to_put);

  return res;

  /* ERRORS */
no_pool:
  {
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
        ("Internal error: can't allocate images"),
        ("We don't have a bufferpool negotiated"));
    return GST_FLOW_ERROR;
  }
no_buffer:
  {
    /* No image available. That's very bad ! */
    GST_WARNING_OBJECT (xvimagesink, "could not create image");
    return GST_FLOW_OK;
  }
invalid_buffer:
  {
    /* No Window available to put our image into */
    GST_WARNING_OBJECT (xvimagesink, "could not map image");
    res = GST_FLOW_OK;
    goto done;
  }
no_window:
  {
    /* No Window available to put our image into */
    GST_WARNING_OBJECT (xvimagesink, "could not output image - no window");
    res = GST_FLOW_ERROR;
    goto done;
  }
activate_failed:
  {
    GST_ERROR_OBJECT (xvimagesink, "failed to activate bufferpool.");
    res = GST_FLOW_ERROR;
    goto done;
  }
}

static gboolean
gst_xv_image_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:{
      GstTagList *l;
      gchar *title = NULL;

      gst_event_parse_tag (event, &l);
      gst_tag_list_get_string (l, GST_TAG_TITLE, &title);

      if (title) {
        GST_DEBUG_OBJECT (xvimagesink, "got tags, title='%s'", title);
        gst_xv_image_sink_xwindow_set_title (xvimagesink, xvimagesink->xwindow,
            title);

        g_free (title);
      }
      break;
    }
    default:
      break;
  }
  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

static gboolean
gst_xv_image_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (xvimagesink, "create new pool");
    pool = gst_xv_image_sink_create_pool (xvimagesink, caps, info.size, 0);

    if (pool == NULL)
      goto no_pool;
  }

  /* update info since allocation frame's wxh might differ from the
   * negotiation ones */
  xvimagesink->info = info;

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
no_pool:
  {
    /* Already warned in create_pool */
    return FALSE;
  }
}

/* Interfaces stuff */
static void
gst_xv_image_sink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (navigation);
  gboolean handled = FALSE;

  GstVideoRectangle src = { 0, };
  GstVideoRectangle dst = { 0, };
  GstVideoRectangle result;
  gdouble x, y, xscale = 1.0, yscale = 1.0;
  GstXWindow *xwindow;

  /* We take the flow_lock while we look at the window */
  g_mutex_lock (&xvimagesink->flow_lock);

  if (!(xwindow = xvimagesink->xwindow)) {
    g_mutex_unlock (&xvimagesink->flow_lock);
    gst_event_unref (event);
    return;
  }

  if (xvimagesink->keep_aspect) {
    /* We get the frame position using the calculated geometry from _setcaps
       that respect pixel aspect ratios */
    src.w = GST_VIDEO_SINK_WIDTH (xvimagesink);
    src.h = GST_VIDEO_SINK_HEIGHT (xvimagesink);
    dst.w = xwindow->render_rect.w;
    dst.h = xwindow->render_rect.h;

    gst_video_sink_center_rect (src, dst, &result, TRUE);
    result.x += xwindow->render_rect.x;
    result.y += xwindow->render_rect.y;
  } else {
    memcpy (&result, &xwindow->render_rect, sizeof (GstVideoRectangle));
  }

  g_mutex_unlock (&xvimagesink->flow_lock);

  /* We calculate scaling using the original video frames geometry to include
     pixel aspect ratio scaling. */
  xscale = (gdouble) xvimagesink->video_width / result.w;
  yscale = (gdouble) xvimagesink->video_height / result.h;

  event = gst_event_make_writable (event);

  /* Converting pointer coordinates to the non scaled geometry */
  if (gst_navigation_event_get_coordinates (event, &x, &y)) {
    x = MIN (x, result.x + result.w);
    x = MAX (x - result.x, 0);
    y = MIN (y, result.y + result.h);
    y = MAX (y - result.y, 0);
    gst_navigation_event_set_coordinates (event, x * xscale, y * yscale);
  }

  gst_event_ref (event);
  handled = gst_pad_push_event (GST_VIDEO_SINK_PAD (xvimagesink), event);

  if (!handled)
    gst_element_post_message ((GstElement *) xvimagesink,
        gst_navigation_message_new_event ((GstObject *) xvimagesink, event));

  gst_event_unref (event);
}

static void
gst_xv_image_sink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = gst_xv_image_sink_navigation_send_event;
}

static void
gst_xv_image_sink_set_window_handle (GstVideoOverlay * overlay, guintptr id)
{
  XID xwindow_id = id;
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (overlay);
  GstXWindow *xwindow = NULL;
  GstXvContext *context;

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink));

  g_mutex_lock (&xvimagesink->flow_lock);

  /* If we already use that window return */
  if (xvimagesink->xwindow && (xwindow_id == xvimagesink->xwindow->win)) {
    g_mutex_unlock (&xvimagesink->flow_lock);
    return;
  }

  /* If the element has not initialized the X11 context try to do so */
  if (!xvimagesink->context &&
      !(xvimagesink->context =
          gst_xvcontext_new (&xvimagesink->config, NULL))) {
    g_mutex_unlock (&xvimagesink->flow_lock);
    /* we have thrown a GST_ELEMENT_ERROR now */
    return;
  }

  context = xvimagesink->context;

  gst_xv_image_sink_update_colorbalance (xvimagesink);

  /* If a window is there already we destroy it */
  if (xvimagesink->xwindow) {
    gst_xwindow_destroy (xvimagesink->xwindow);
    xvimagesink->xwindow = NULL;
  }

  /* If the xid is 0 we go back to an internal window */
  if (xwindow_id == 0) {
    /* If no width/height caps nego did not happen window will be created
       during caps nego then */
    if (GST_VIDEO_SINK_WIDTH (xvimagesink)
        && GST_VIDEO_SINK_HEIGHT (xvimagesink)) {
      xwindow =
          gst_xv_image_sink_xwindow_new (xvimagesink,
          GST_VIDEO_SINK_WIDTH (xvimagesink),
          GST_VIDEO_SINK_HEIGHT (xvimagesink));
    }
  } else {
    xwindow = gst_xvcontext_create_xwindow_from_xid (context, xwindow_id);
    gst_xwindow_set_event_handling (xwindow, xvimagesink->handle_events);
  }

  if (xwindow)
    xvimagesink->xwindow = xwindow;

  g_mutex_unlock (&xvimagesink->flow_lock);
}

static void
gst_xv_image_sink_expose (GstVideoOverlay * overlay)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (overlay);

  GST_DEBUG ("doing expose");
  gst_xv_image_sink_xwindow_update_geometry (xvimagesink);
  gst_xv_image_sink_xvimage_put (xvimagesink, NULL);
}

static void
gst_xv_image_sink_set_event_handling (GstVideoOverlay * overlay,
    gboolean handle_events)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (overlay);

  g_mutex_lock (&xvimagesink->flow_lock);
  xvimagesink->handle_events = handle_events;
  if (G_LIKELY (xvimagesink->xwindow))
    gst_xwindow_set_event_handling (xvimagesink->xwindow, handle_events);
  g_mutex_unlock (&xvimagesink->flow_lock);
}

static void
gst_xv_image_sink_set_render_rectangle (GstVideoOverlay * overlay, gint x,
    gint y, gint width, gint height)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (overlay);

  g_mutex_lock (&xvimagesink->flow_lock);
  if (G_LIKELY (xvimagesink->xwindow)) {
    gst_xwindow_set_render_rectangle (xvimagesink->xwindow, x, y, width,
        height);
  } else {
    xvimagesink->render_rect.x = x;
    xvimagesink->render_rect.y = y;
    xvimagesink->render_rect.w = width;
    xvimagesink->render_rect.h = height;
    xvimagesink->pending_render_rect = TRUE;
  }
  g_mutex_unlock (&xvimagesink->flow_lock);
}

static void
gst_xv_image_sink_video_overlay_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_xv_image_sink_set_window_handle;
  iface->expose = gst_xv_image_sink_expose;
  iface->handle_events = gst_xv_image_sink_set_event_handling;
  iface->set_render_rectangle = gst_xv_image_sink_set_render_rectangle;
}

static const GList *
gst_xv_image_sink_colorbalance_list_channels (GstColorBalance * balance)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (balance);

  g_return_val_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink), NULL);

  if (xvimagesink->context)
    return xvimagesink->context->channels_list;
  else
    return NULL;
}

static void
gst_xv_image_sink_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (balance);

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink));
  g_return_if_fail (channel->label != NULL);

  xvimagesink->config.cb_changed = TRUE;

  /* Normalize val to [-1000, 1000] */
  value = floor (0.5 + -1000 + 2000 * (value - channel->min_value) /
      (double) (channel->max_value - channel->min_value));

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    xvimagesink->config.hue = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    xvimagesink->config.saturation = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    xvimagesink->config.contrast = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    xvimagesink->config.brightness = value;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
    return;
  }

  gst_xv_image_sink_update_colorbalance (xvimagesink);
}

static gint
gst_xv_image_sink_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (balance);
  gint value = 0;

  g_return_val_if_fail (GST_IS_XV_IMAGE_SINK (xvimagesink), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    value = xvimagesink->config.hue;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    value = xvimagesink->config.saturation;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    value = xvimagesink->config.contrast;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    value = xvimagesink->config.brightness;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
  }

  /* Normalize val to [channel->min_value, channel->max_value] */
  value = channel->min_value + (channel->max_value - channel->min_value) *
      (value + 1000) / 2000;

  return value;
}

static GstColorBalanceType
gst_xv_image_sink_colorbalance_get_balance_type (GstColorBalance * balance)
{
  return GST_COLOR_BALANCE_HARDWARE;
}

static void
gst_xv_image_sink_colorbalance_init (GstColorBalanceInterface * iface)
{
  iface->list_channels = gst_xv_image_sink_colorbalance_list_channels;
  iface->set_value = gst_xv_image_sink_colorbalance_set_value;
  iface->get_value = gst_xv_image_sink_colorbalance_get_value;
  iface->get_balance_type = gst_xv_image_sink_colorbalance_get_balance_type;
}

#if 0
static const GList *
gst_xv_image_sink_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *list = NULL;

  if (!list) {
    list = g_list_append (NULL, g_object_class_find_property (klass, "device"));
    list =
        g_list_append (list, g_object_class_find_property (klass,
            "autopaint-colorkey"));
    list =
        g_list_append (list, g_object_class_find_property (klass,
            "double-buffer"));
    list =
        g_list_append (list, g_object_class_find_property (klass, "colorkey"));
  }

  return list;
}

static void
gst_xv_image_sink_probe_probe_property (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (probe);

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      GST_DEBUG_OBJECT (xvimagesink,
          "probing device list and get capabilities");
      if (!xvimagesink->context) {
        GST_DEBUG_OBJECT (xvimagesink, "generating context");
        xvimagesink->context = gst_xv_image_sink_context_get (xvimagesink);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

static gboolean
gst_xv_image_sink_probe_needs_probe (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      if (xvimagesink->context != NULL) {
        ret = FALSE;
      } else {
        ret = TRUE;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;
}

static GValueArray *
gst_xv_image_sink_probe_get_values (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XV_IMAGE_SINK (probe);
  GValueArray *array = NULL;

  if (G_UNLIKELY (!xvimagesink->context)) {
    GST_WARNING_OBJECT (xvimagesink, "we don't have any context, can't "
        "get values");
    goto beach;
  }

  switch (prop_id) {
    case PROP_DEVICE:
    {
      guint i;
      GValue value = { 0 };

      array = g_value_array_new (xvimagesink->context->nb_adaptors);
      g_value_init (&value, G_TYPE_STRING);

      for (i = 0; i < xvimagesink->context->nb_adaptors; i++) {
        gchar *adaptor_id_s = g_strdup_printf ("%u", i);

        g_value_set_string (&value, adaptor_id_s);
        g_value_array_append (array, &value);
        g_free (adaptor_id_s);
      }
      g_value_unset (&value);
      break;
    }
    case PROP_AUTOPAINT_COLORKEY:
      if (xvimagesink->have_autopaint_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_DOUBLE_BUFFER:
      if (xvimagesink->have_double_buffer) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_COLORKEY:
      if (xvimagesink->have_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (1);
        g_value_init (&value, GST_TYPE_INT_RANGE);
        gst_value_set_int_range (&value, 0, 0xffffff);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

beach:
  return array;
}

static void
gst_xv_image_sink_property_probe_interface_init (GstPropertyProbeInterface *
    iface)
{
  iface->get_properties = gst_xv_image_sink_probe_get_properties;
  iface->probe_property = gst_xv_image_sink_probe_probe_property;
  iface->needs_probe = gst_xv_image_sink_probe_needs_probe;
  iface->get_values = gst_xv_image_sink_probe_get_values;
}
#endif

/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_xv_image_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink;

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (object));

  xvimagesink = GST_XV_IMAGE_SINK (object);

  switch (prop_id) {
    case PROP_HUE:
      xvimagesink->config.hue = g_value_get_int (value);
      xvimagesink->config.cb_changed = TRUE;
      gst_xv_image_sink_update_colorbalance (xvimagesink);
      break;
    case PROP_CONTRAST:
      xvimagesink->config.contrast = g_value_get_int (value);
      xvimagesink->config.cb_changed = TRUE;
      gst_xv_image_sink_update_colorbalance (xvimagesink);
      break;
    case PROP_BRIGHTNESS:
      xvimagesink->config.brightness = g_value_get_int (value);
      xvimagesink->config.cb_changed = TRUE;
      gst_xv_image_sink_update_colorbalance (xvimagesink);
      break;
    case PROP_SATURATION:
      xvimagesink->config.saturation = g_value_get_int (value);
      xvimagesink->config.cb_changed = TRUE;
      gst_xv_image_sink_update_colorbalance (xvimagesink);
      break;
    case PROP_DISPLAY:
      g_free (xvimagesink->config.display_name);
      xvimagesink->config.display_name = g_value_dup_string (value);
      break;
    case PROP_SYNCHRONOUS:
      xvimagesink->synchronous = g_value_get_boolean (value);
      if (xvimagesink->context) {
        gst_xvcontext_set_synchronous (xvimagesink->context,
            xvimagesink->synchronous);
      }
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      g_free (xvimagesink->par);
      xvimagesink->par = g_new0 (GValue, 1);
      g_value_init (xvimagesink->par, GST_TYPE_FRACTION);
      if (!g_value_transform (value, xvimagesink->par)) {
        g_warning ("Could not transform string to aspect ratio");
        gst_value_set_fraction (xvimagesink->par, 1, 1);
      }
      GST_DEBUG_OBJECT (xvimagesink, "set PAR to %d/%d",
          gst_value_get_fraction_numerator (xvimagesink->par),
          gst_value_get_fraction_denominator (xvimagesink->par));
      break;
    case PROP_FORCE_ASPECT_RATIO:
      xvimagesink->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_HANDLE_EVENTS:
      gst_xv_image_sink_set_event_handling (GST_VIDEO_OVERLAY (xvimagesink),
          g_value_get_boolean (value));
      gst_xv_image_sink_manage_event_thread (xvimagesink);
      break;
    case PROP_DEVICE:
      xvimagesink->config.adaptor_nr = atoi (g_value_get_string (value));
      break;
    case PROP_HANDLE_EXPOSE:
      xvimagesink->handle_expose = g_value_get_boolean (value);
      gst_xv_image_sink_manage_event_thread (xvimagesink);
      break;
    case PROP_DOUBLE_BUFFER:
      xvimagesink->double_buffer = g_value_get_boolean (value);
      break;
    case PROP_AUTOPAINT_COLORKEY:
      xvimagesink->config.autopaint_colorkey = g_value_get_boolean (value);
      break;
    case PROP_COLORKEY:
      xvimagesink->config.colorkey = g_value_get_int (value);
      break;
    case PROP_DRAW_BORDERS:
      xvimagesink->draw_borders = g_value_get_boolean (value);
      break;
    default:
      if (!gst_video_overlay_set_property (object, PROP_LAST, prop_id, value))
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_xv_image_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink;

  g_return_if_fail (GST_IS_XV_IMAGE_SINK (object));

  xvimagesink = GST_XV_IMAGE_SINK (object);

  switch (prop_id) {
    case PROP_HUE:
      g_value_set_int (value, xvimagesink->config.hue);
      break;
    case PROP_CONTRAST:
      g_value_set_int (value, xvimagesink->config.contrast);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_int (value, xvimagesink->config.brightness);
      break;
    case PROP_SATURATION:
      g_value_set_int (value, xvimagesink->config.saturation);
      break;
    case PROP_DISPLAY:
      g_value_set_string (value, xvimagesink->config.display_name);
      break;
    case PROP_SYNCHRONOUS:
      g_value_set_boolean (value, xvimagesink->synchronous);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      if (xvimagesink->par)
        g_value_transform (xvimagesink->par, value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, xvimagesink->keep_aspect);
      break;
    case PROP_HANDLE_EVENTS:
      g_value_set_boolean (value, xvimagesink->handle_events);
      break;
    case PROP_DEVICE:
    {
      char *adaptor_nr_s =
          g_strdup_printf ("%u", xvimagesink->config.adaptor_nr);

      g_value_set_string (value, adaptor_nr_s);
      g_free (adaptor_nr_s);
      break;
    }
    case PROP_DEVICE_NAME:
      if (xvimagesink->context && xvimagesink->context->adaptors) {
        g_value_set_string (value,
            xvimagesink->context->adaptors[xvimagesink->config.adaptor_nr]);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    case PROP_HANDLE_EXPOSE:
      g_value_set_boolean (value, xvimagesink->handle_expose);
      break;
    case PROP_DOUBLE_BUFFER:
      g_value_set_boolean (value, xvimagesink->double_buffer);
      break;
    case PROP_AUTOPAINT_COLORKEY:
      g_value_set_boolean (value, xvimagesink->config.autopaint_colorkey);
      break;
    case PROP_COLORKEY:
      g_value_set_int (value, xvimagesink->config.colorkey);
      break;
    case PROP_DRAW_BORDERS:
      g_value_set_boolean (value, xvimagesink->draw_borders);
      break;
    case PROP_WINDOW_WIDTH:
      if (xvimagesink->xwindow)
        g_value_set_uint64 (value, xvimagesink->xwindow->width);
      else
        g_value_set_uint64 (value, 0);
      break;
    case PROP_WINDOW_HEIGHT:
      if (xvimagesink->xwindow)
        g_value_set_uint64 (value, xvimagesink->xwindow->height);
      else
        g_value_set_uint64 (value, 0);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_xv_image_sink_open (GstXvImageSink * xvimagesink)
{
  GError *error = NULL;

  /* Initializing the XvContext unless already done through GstVideoOverlay */
  if (!xvimagesink->context) {
    GstXvContext *context;
    if (!(context = gst_xvcontext_new (&xvimagesink->config, &error)))
      goto no_context;

    GST_OBJECT_LOCK (xvimagesink);
    xvimagesink->context = context;
  } else
    GST_OBJECT_LOCK (xvimagesink);
  /* make an allocator for this context */
  xvimagesink->allocator = gst_xvimage_allocator_new (xvimagesink->context);
  GST_OBJECT_UNLOCK (xvimagesink);

  /* update object's par with calculated one if not set yet */
  if (!xvimagesink->par) {
    xvimagesink->par = g_new0 (GValue, 1);
    gst_value_init_and_copy (xvimagesink->par, xvimagesink->context->par);
    GST_DEBUG_OBJECT (xvimagesink, "set calculated PAR on object's PAR");
  }
  /* call XSynchronize with the current value of synchronous */
  gst_xvcontext_set_synchronous (xvimagesink->context,
      xvimagesink->synchronous);
  gst_xv_image_sink_update_colorbalance (xvimagesink);
  gst_xv_image_sink_manage_event_thread (xvimagesink);

  return TRUE;

no_context:
  {
    gst_element_message_full (GST_ELEMENT (xvimagesink), GST_MESSAGE_ERROR,
        error->domain, error->code, g_strdup ("Could not initialise Xv output"),
        g_strdup (error->message), __FILE__, GST_FUNCTION, __LINE__);
    g_clear_error (&error);
    return FALSE;
  }
}

static void
gst_xv_image_sink_close (GstXvImageSink * xvimagesink)
{
  GThread *thread;
  GstXvContext *context;

  GST_OBJECT_LOCK (xvimagesink);
  xvimagesink->running = FALSE;
  /* grab thread and mark it as NULL */
  thread = xvimagesink->event_thread;
  xvimagesink->event_thread = NULL;
  GST_OBJECT_UNLOCK (xvimagesink);

  /* Wait for our event thread to finish before we clean up our stuff. */
  if (thread)
    g_thread_join (thread);

  if (xvimagesink->cur_image) {
    gst_buffer_unref (xvimagesink->cur_image);
    xvimagesink->cur_image = NULL;
  }

  g_mutex_lock (&xvimagesink->flow_lock);

  if (xvimagesink->pool) {
    gst_object_unref (xvimagesink->pool);
    xvimagesink->pool = NULL;
  }

  if (xvimagesink->xwindow) {
    gst_xwindow_clear (xvimagesink->xwindow);
    gst_xwindow_destroy (xvimagesink->xwindow);
    xvimagesink->xwindow = NULL;
  }
  g_mutex_unlock (&xvimagesink->flow_lock);

  if (xvimagesink->allocator) {
    gst_object_unref (xvimagesink->allocator);
    xvimagesink->allocator = NULL;
  }

  GST_OBJECT_LOCK (xvimagesink);
  /* grab context and mark it as NULL */
  context = xvimagesink->context;
  xvimagesink->context = NULL;
  GST_OBJECT_UNLOCK (xvimagesink);

  if (context)
    gst_xvcontext_unref (context);
}

/* Finalize is called only once, dispose can be called multiple times.
 * We use mutexes and don't reset stuff to NULL here so let's register
 * as a finalize. */
static void
gst_xv_image_sink_finalize (GObject * object)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XV_IMAGE_SINK (object);

  gst_xv_image_sink_close (xvimagesink);

  gst_xvcontext_config_clear (&xvimagesink->config);

  if (xvimagesink->par) {
    g_free (xvimagesink->par);
    xvimagesink->par = NULL;
  }
  g_mutex_clear (&xvimagesink->flow_lock);
  g_free (xvimagesink->media_title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_xv_image_sink_init (GstXvImageSink * xvimagesink)
{
  xvimagesink->config.display_name = NULL;
  xvimagesink->config.adaptor_nr = 0;
  xvimagesink->config.autopaint_colorkey = TRUE;
  xvimagesink->config.double_buffer = TRUE;
  /* on 16bit displays this becomes r,g,b = 1,2,3
   * on 24bit displays this becomes r,g,b = 8,8,16
   * as a port atom value */
  xvimagesink->config.colorkey = (8 << 16) | (8 << 8) | 16;
  xvimagesink->config.hue = xvimagesink->config.saturation = 0;
  xvimagesink->config.contrast = xvimagesink->config.brightness = 0;
  xvimagesink->config.cb_changed = FALSE;

  xvimagesink->context = NULL;
  xvimagesink->xwindow = NULL;
  xvimagesink->cur_image = NULL;

  xvimagesink->fps_n = 0;
  xvimagesink->fps_d = 0;
  xvimagesink->video_width = 0;
  xvimagesink->video_height = 0;

  g_mutex_init (&xvimagesink->flow_lock);

  xvimagesink->pool = NULL;

  xvimagesink->synchronous = FALSE;
  xvimagesink->running = FALSE;
  xvimagesink->keep_aspect = TRUE;
  xvimagesink->handle_events = TRUE;
  xvimagesink->par = NULL;
  xvimagesink->handle_expose = TRUE;

  xvimagesink->draw_borders = TRUE;
}

static void
gst_xv_image_sink_class_init (GstXvImageSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_xv_image_sink_set_property;
  gobject_class->get_property = gst_xv_image_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_int ("contrast", "Contrast", "The contrast of the video",
          -1000, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_int ("brightness", "Brightness",
          "The brightness of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_int ("hue", "Hue", "The hue of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_int ("saturation", "Saturation",
          "The saturation of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_string ("display", "Display", "X Display name", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SYNCHRONOUS,
      g_param_spec_boolean ("synchronous", "Synchronous",
          "When enabled, runs the X display in synchronous mode. "
          "(unrelated to A/V sync, used only for debugging)", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HANDLE_EVENTS,
      g_param_spec_boolean ("handle-events", "Handle XEvents",
          "When enabled, XEvents will be selected and handled", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Adaptor number",
          "The number of the video adaptor", "0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Adaptor name",
          "The name of the video adaptor", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_video_overlay_install_properties (gobject_class, PROP_LAST);

  /**
   * GstXvImageSink:handle-expose
   *
   * When enabled, the current frame will always be drawn in response to X
   * Expose.
   */
  g_object_class_install_property (gobject_class, PROP_HANDLE_EXPOSE,
      g_param_spec_boolean ("handle-expose", "Handle expose",
          "When enabled, "
          "the current frame will always be drawn in response to X Expose "
          "events", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:double-buffer
   *
   * Whether to double-buffer the output.
   */
  g_object_class_install_property (gobject_class, PROP_DOUBLE_BUFFER,
      g_param_spec_boolean ("double-buffer", "Double-buffer",
          "Whether to double-buffer the output", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstXvImageSink:autopaint-colorkey
   *
   * Whether to autofill overlay with colorkey
   */
  g_object_class_install_property (gobject_class, PROP_AUTOPAINT_COLORKEY,
      g_param_spec_boolean ("autopaint-colorkey", "Autofill with colorkey",
          "Whether to autofill overlay with colorkey", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstXvImageSink:colorkey
   *
   * Color to use for the overlay mask.
   */
  g_object_class_install_property (gobject_class, PROP_COLORKEY,
      g_param_spec_int ("colorkey", "Colorkey",
          "Color to use for the overlay mask", G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:draw-borders
   *
   * Draw black borders when using GstXvImageSink:force-aspect-ratio to fill
   * unused parts of the video area.
   */
  g_object_class_install_property (gobject_class, PROP_DRAW_BORDERS,
      g_param_spec_boolean ("draw-borders", "Draw Borders",
          "Draw black borders to fill unused area in force-aspect-ratio mode",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:window-width
   *
   * Actual width of the video window.
   */
  g_object_class_install_property (gobject_class, PROP_WINDOW_WIDTH,
      g_param_spec_uint64 ("window-width", "window-width",
          "Width of the window", 0, G_MAXUINT64, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:window-height
   *
   * Actual height of the video window.
   */
  g_object_class_install_property (gobject_class, PROP_WINDOW_HEIGHT,
      g_param_spec_uint64 ("window-height", "window-height",
          "Height of the window", 0, G_MAXUINT64, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_xv_image_sink_finalize;

  gst_element_class_set_static_metadata (gstelement_class,
      "Video sink", "Sink/Video",
      "A Xv based videosink", "Julien Moutte <julien@moutte.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_xv_image_sink_sink_template_factory);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_xv_image_sink_change_state);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_xv_image_sink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_xv_image_sink_setcaps);
  gstbasesink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_xv_image_sink_get_times);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_xv_image_sink_propose_allocation);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_xv_image_sink_event);

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_xv_image_sink_show_frame);

  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
}
