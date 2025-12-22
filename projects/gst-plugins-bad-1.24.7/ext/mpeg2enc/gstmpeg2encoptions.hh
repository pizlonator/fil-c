/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * gstmpeg2encoptions.hh: gobject/mpeg2enc option wrapping class
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

#ifndef __GST_MPEG2ENCOPTIONS_H__
#define __GST_MPEG2ENCOPTIONS_H__

#include <glib-object.h>
#include <format_codes.h>
#include <mpeg2encoptions.hh>

class GstMpeg2EncOptions : public MPEG2EncOptions {
public:
  GstMpeg2EncOptions ();

  /* Init properties (call once) */
  static void initProperties (GObjectClass *klass);

  /* GObject property foo, C++ style */
  void getProperty (guint         prop_id,
		    GValue       *value);
  void setProperty (guint         prop_id,
		    const GValue *value);

  gboolean disable_encode_retries;
};

#endif /* __GST_MPEG2ENCOPTIONS_H__ */
