/* GStreamer mplex (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * gstmplexjob.hh: gstreamer/mplex multiplex-job wrapper
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

#ifndef __GST_MPLEXJOB_H__
#define __GST_MPLEXJOB_H__

#include <glib-object.h>
#include <interact.hpp>

class GstMplexJob : public MultiplexJob {
public:
  GstMplexJob (void);

  /* gobject properties */
  static void initProperties (GObjectClass *klass);

  /* set/get gobject properties */
  void getProperty (guint         prop_id,
		    GValue       *value);
  void setProperty (guint         prop_id,
		    const GValue *value);

  int bufsize;
};

#endif /* __GST_MPLEXJOB_H__ */
