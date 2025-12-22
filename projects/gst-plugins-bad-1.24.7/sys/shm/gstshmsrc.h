/* GStreamer
 * Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 * Copyright (C) <2009> Nokia Inc
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

#ifndef __GST_SHM_SRC_H__
#define __GST_SHM_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/base/gstbasesrc.h>

#include "shmpipe.h"

G_BEGIN_DECLS
#define GST_TYPE_SHM_SRC \
  (gst_shm_src_get_type())
#define GST_SHM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SHM_SRC,GstShmSrc))
#define GST_SHM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SHM_SRC,GstShmSrcClass))
#define GST_IS_SHM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SHM_SRC))
#define GST_IS_SHM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SHM_SRC))
typedef struct _GstShmSrc GstShmSrc;
typedef struct _GstShmSrcClass GstShmSrcClass;
typedef struct _GstShmPipe GstShmPipe;

struct _GstShmSrc
{
  GstPushSrc element;

  gchar *socket_path;

  GstShmPipe *pipe;
  GstPoll *poll;
  GstPollFD pollfd;


  GstFlowReturn flow_return;
  gboolean unlocked;
};

struct _GstShmSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_shm_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (shmsrc);

struct _GstShmPipe {
  int use_count;

  GstShmSrc *src;
  ShmPipe *pipe;
};

G_END_DECLS
#endif /* __GST_SHM_SRC_H__ */
