/*
 * gstmpegtssection.c -
 * Copyright (C) 2013 Edward Hervey
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2007 Alessandro Decina
 *               2010 Edward Hervey
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *  Author: Edward Hervey <bilboed@bilboed.com>, Collabora Ltd.
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
 *   Edward Hervey <edward@collabora.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mpegts.h"
#include "gstmpegts-private.h"

/**
 * SECTION:gstmpegts
 * @title: Initialization
 * @short_description: Initialization of the MPEG-TS helper library
 * @include: gst/mpegts/mpegts.h
 *
 * Before any usage of this library, the initialization function should be called.
 */

GST_DEBUG_CATEGORY (mpegts_debug);

/**
 * gst_mpegts_initialize:
 *
 * Initializes the MPEG-TS helper library. Must be called before any
 * usage.
 */
void
gst_mpegts_initialize (void)
{
  GST_DEBUG_CATEGORY_INIT (mpegts_debug, "mpegts", 0, "MPEG-TS helper library");

  __initialize_sections ();
  __initialize_descriptors ();
}
