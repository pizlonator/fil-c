/* GStreamer
 * Copyright (C) <2009> Kapil Agrawal <kapil@mediamagictechnologies.com>
 *
 * gstopencv.cpp: plugin registering
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

#include "gstcvdilate.h"
#include "gstcvequalizehist.h"
#include "gstcverode.h"
#include "gstcvlaplace.h"
#include "gstcvsmooth.h"
#include "gstcvsobel.h"
#include "gstedgedetect.h"
#include "gstfaceblur.h"
#include "gstfacedetect.h"
#include "gstmotioncells.h"
#include "gsttemplatematch.h"
#include "gsttextoverlay.h"
#include "gsthanddetect.h"
#include "gstskindetect.h"
#include "gstretinex.h"
#include "gstsegmentation.h"
#include "gstgrabcut.h"
#include "gstdisparity.h"
#include "gstdewarp.h"
#include "gstcameracalibrate.h"
#include "gstcameraundistort.h"
#include "gstcvtracker.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (cvdilate, plugin);
  ret |= GST_ELEMENT_REGISTER (cvequalizehist, plugin);
  ret |= GST_ELEMENT_REGISTER (cverode, plugin);
  ret |= GST_ELEMENT_REGISTER (cvlaplace, plugin);
  ret |= GST_ELEMENT_REGISTER (cvsmooth, plugin);
  ret |= GST_ELEMENT_REGISTER (cvsobel, plugin);
  ret |= GST_ELEMENT_REGISTER (edgedetect, plugin);
  ret |= GST_ELEMENT_REGISTER (faceblur, plugin);
  ret |= GST_ELEMENT_REGISTER (facedetect, plugin);
  ret |= GST_ELEMENT_REGISTER (motioncells, plugin);
  ret |= GST_ELEMENT_REGISTER (templatematch, plugin);
  ret |= GST_ELEMENT_REGISTER (opencvtextoverlay, plugin);
  ret |= GST_ELEMENT_REGISTER (handdetect, plugin);
  ret |= GST_ELEMENT_REGISTER (skindetect, plugin);
  ret |= GST_ELEMENT_REGISTER (retinex, plugin);
  ret |= GST_ELEMENT_REGISTER (segmentation, plugin);
  ret |= GST_ELEMENT_REGISTER (grabcut, plugin);
  ret |= GST_ELEMENT_REGISTER (disparity, plugin);
  ret |= GST_ELEMENT_REGISTER (dewarp, plugin);
  ret |= GST_ELEMENT_REGISTER (cameracalibrate, plugin);
  ret |= GST_ELEMENT_REGISTER (cameraundistort, plugin);
  ret |= GST_ELEMENT_REGISTER (cvtracker, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    opencv,
    "GStreamer OpenCV Plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
