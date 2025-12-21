/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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

#ifndef __GST_XVCONTEXT_H__
#define __GST_XVCONTEXT_H__

#ifdef HAVE_XSHM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* HAVE_XSHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM */

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct _GstXvContextConfig GstXvContextConfig;
typedef struct _GstXvImageFormat GstXvImageFormat;
typedef struct _GstXvContext GstXvContext;
typedef struct _GstXvTouchDevice GstXvTouchDevice;

/**
 * GstXvContextConfig:
 *
 * current configuration of the context
 */
struct _GstXvContextConfig
{
  gchar *display_name;
  guint  adaptor_nr;

  /* port attributes */
  gboolean autopaint_colorkey;
  gint colorkey;

  gboolean double_buffer;

  gint brightness;
  gint contrast;
  gint hue;
  gint saturation;
  gboolean cb_changed;
};

/**
 * GstXvImageFormat:
 * @format: the image format
 * @caps: generated #GstCaps for this image format
 *
 * Structure storing image format to #GstCaps association.
 */
struct _GstXvImageFormat
{
  gint format;
  GstVideoFormat vformat;
  GstCaps *caps;
};

#define GST_TYPE_XVCONTEXT      (gst_xvcontext_get_type())
#define GST_IS_XVCONTEXT(obj)   (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_XVCONTEXT))
#define GST_XVCONTEXT_CAST(obj) ((GstXvContext *)obj)
#define GST_XVCONTEXT(obj)      (GST_XVCONTEXT_CAST(obj))

/*
 * GstXvContext:
 * @disp: the X11 Display of this context
 * @screen: the default Screen of Display @disp
 * @screen_num: the Screen number of @screen
 * @visual: the default Visual of Screen @screen
 * @root: the root Window of Display @disp
 * @white: the value of a white pixel on Screen @screen
 * @black: the value of a black pixel on Screen @screen
 * @depth: the color depth of Display @disp
 * @bpp: the number of bits per pixel on Display @disp
 * @endianness: the endianness of image bytes on Display @disp
 * @width: the width in pixels of Display @disp
 * @height: the height in pixels of Display @disp
 * @widthmm: the width in millimeters of Display @disp
 * @heightmm: the height in millimeters of Display @disp
 * @par: the pixel aspect ratio calculated from @width, @widthmm and @height,
 * @heightmm ratio
 * @use_xshm: used to known whether of not XShm extension is usable or not even
 * if the Extension is present
 * @use_xkb: used to known wether of not Xkb extension is usable or not even
 * if the Extension is present
 * @use_xi2: used to known wether of not XInput extension is usable or not even
 * if the Extension is present
 * @xi_opcode: XInput opcode
 * @xv_port_id: the XVideo port ID
 * @im_format: used to store at least a valid format for XShm calls checks
 * @formats_list: list of supported image formats on @xv_port_id
 * @channels_list: list of #GstColorBalanceChannels
 * @caps: the #GstCaps that Display @disp can accept
 *
 * Structure used to store various information collected/calculated for a
 * Display.
 */
struct _GstXvContext
{
  GstMiniObject parent;

  GMutex lock;

  Display *disp;

  Screen *screen;
  gint screen_num;

  Visual *visual;

  Window root;

  gulong white, black;

  gint depth;
  gint bpp;
  gint endianness;

  gint width, height;
  gint widthmm, heightmm;
  GValue *par;                  /* calculated pixel aspect ratio */

  gboolean use_xshm;
  gboolean use_xkb;
  gboolean use_xi2;
  int xi_opcode;

  XvPortID xv_port_id;
  guint nb_adaptors;
  gchar **adaptors;
  guint adaptor_nr;
  gint im_format;

  /* port features */
  gboolean have_autopaint_colorkey;
  gboolean have_colorkey;
  gboolean have_double_buffer;
  gboolean have_iturbt709;
  gboolean have_xvcolorspace;

  GList *formats_list;

  GList *channels_list;

  GstCaps *caps;

  /* Optimisation storage for buffer_alloc return */
  GstCaps *last_caps;
  gint last_format;
  gint last_width;
  gint last_height;
};

GType gst_xvcontext_get_type (void);

void            gst_xvcontext_config_clear (GstXvContextConfig *config);

GstXvContext *  gst_xvcontext_new          (GstXvContextConfig *config, GError **error);

/* refcounting */
static inline GstXvContext *
gst_xvcontext_ref (GstXvContext * xvcontext)
{
  return GST_XVCONTEXT_CAST (gst_mini_object_ref (GST_MINI_OBJECT_CAST (
      xvcontext)));
}

static inline void
gst_xvcontext_unref (GstXvContext * xvcontext)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (xvcontext));
}

gint            gst_xvcontext_get_format_from_info      (GstXvContext * xvcontext,
                                                         const GstVideoInfo * info);


void            gst_xvcontext_set_synchronous           (GstXvContext * xvcontext,
                                                         gboolean synchronous);
void            gst_xvcontext_update_colorbalance       (GstXvContext * xvcontext,
                                                         GstXvContextConfig * config);
void            gst_xvcontext_set_colorimetry           (GstXvContext * xvcontext,
                                                         GstVideoColorimetry *colorimetry);


typedef struct _GstXWindow GstXWindow;

/*
 * GstXWindow:
 * @win: the Window ID of this X11 window
 * @width: the width in pixels of Window @win
 * @height: the height in pixels of Window @win
 * @internal: used to remember if Window @win was created internally or passed
 * through the #GstVideoOverlay interface
 * @gc: the Graphical Context of Window @win
 * @last_touch: timestamp of the last received touch event
 * @touch_devices: list of known touchscreen devices
 *
 * Structure used to store information about a Window.
 */
struct _GstXWindow
{
  GstXvContext *context;

  Window win;
  gint width, height;
  gboolean have_render_rect;
  GstVideoRectangle render_rect;
  gboolean internal;
  GC gc;

#ifdef HAVE_XI2
  Time last_touch;
  GArray *touch_devices;
#endif
};

/*
 * GstXvTouchDevice:
 * @name: the name of this decive
 * @id: the device ID of this device
 * @pressure_valuator: index of the valuator that encodes pressure data, if present
 * @abs_pressure: if pressure is reported in absolute or relative values
 * @current_pressure: stores the most recent pressure value for this device
 * @pressure_min: lowest possible pressure value
 * @pressure_max: highest possible pressure value
 *
 * Structure used to store information about a touchscreen device.
 */
struct _GstXvTouchDevice {
  gchar *name;
  gint id, pressure_valuator;
  gboolean abs_pressure;
  gdouble current_pressure, pressure_min, pressure_max;
};

G_END_DECLS

GstXWindow *   gst_xvcontext_create_xwindow     (GstXvContext * context,
                                                 gint width, gint height);
GstXWindow *   gst_xvcontext_create_xwindow_from_xid (GstXvContext * context, XID xid);

void           gst_xwindow_destroy              (GstXWindow * window);

#ifdef HAVE_XI2
void           gst_xwindow_select_touch_events  (GstXvContext * context, GstXWindow * window);
#endif

void           gst_xwindow_set_event_handling   (GstXWindow * window, gboolean handle_events);
void           gst_xwindow_set_title            (GstXWindow * window, const gchar * title);

void           gst_xwindow_update_geometry      (GstXWindow * window);
void           gst_xwindow_clear                (GstXWindow * window);

void           gst_xwindow_set_render_rectangle (GstXWindow * window,
                                                 gint x, gint y, gint width, gint height);



#endif /* __GST_XVCONTEXT_H__ */
