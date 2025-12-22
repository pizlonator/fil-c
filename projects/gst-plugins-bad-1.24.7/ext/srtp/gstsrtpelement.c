/*
 * GStreamer - GStreamer SRTP encoder and decoder
 *
 * Copyright 2009-2013 Collabora Ltd.
 *  @author: Gabriel Millaire <gabriel.millaire@collabora.co.uk>
 *  @author: Olivier Crete <olivier.crete@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gstsrtpelements.h"


void
srtp_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    srtp_init ();
    gst_type_mark_as_plugin_api (GST_TYPE_SRTP_AUTH_TYPE, 0);
    gst_type_mark_as_plugin_api (GST_TYPE_SRTP_CIPHER_TYPE, 0);
    g_once_init_leave (&res, TRUE);
  }
}
