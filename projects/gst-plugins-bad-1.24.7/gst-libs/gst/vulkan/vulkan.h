/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_H__
#define __GST_VULKAN_H__

#include <gst/gst.h>

#include <gst/vulkan/gstvkapi.h>
#include <gst/vulkan/gstvkdebug.h>
#include <gst/vulkan/gstvkerror.h>
#include <gst/vulkan/gstvkformat.h>

/* vulkan wrapper objects */
#include <gst/vulkan/gstvkinstance.h>
#include <gst/vulkan/gstvkphysicaldevice.h>
#include <gst/vulkan/gstvkdevice.h>
#include <gst/vulkan/gstvkqueue.h>
#include <gst/vulkan/gstvkfence.h>
#include <gst/vulkan/gstvkdisplay.h>
#include <gst/vulkan/gstvkwindow.h>
#include <gst/vulkan/gstvkmemory.h>
#include <gst/vulkan/gstvkbarrier.h>
#include <gst/vulkan/gstvkbuffermemory.h>
#include <gst/vulkan/gstvkimagememory.h>
#include <gst/vulkan/gstvkimageview.h>
#include <gst/vulkan/gstvkbufferpool.h>
#include <gst/vulkan/gstvkimagebufferpool.h>
#include <gst/vulkan/gstvkcommandbuffer.h>
#include <gst/vulkan/gstvkcommandpool.h>
#include <gst/vulkan/gstvkdescriptorset.h>
#include <gst/vulkan/gstvkdescriptorpool.h>
#include <gst/vulkan/gstvkhandle.h>

/* helper elements */
#include <gst/vulkan/gstvkvideofilter.h>

/* helper vulkan objects */
#include <gst/vulkan/gstvkdescriptorcache.h>
#include <gst/vulkan/gstvktrash.h>
#include <gst/vulkan/gstvkswapper.h>
#include <gst/vulkan/gstvkhandlepool.h>
#include <gst/vulkan/gstvkfullscreenquad.h>

#include <gst/vulkan/gstvkoperation.h>
#include <gst/vulkan/gstvkutils.h>
#include <gst/vulkan/gstvkvideoutils.h>

#endif /* __GST_VULKAN_H__ */
