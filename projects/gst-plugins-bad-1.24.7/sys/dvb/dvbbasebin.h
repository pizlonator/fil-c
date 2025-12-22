/*
 * dvbbasebin.h - 
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
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

#ifndef GST_DVB_BASE_BIN_H
#define GST_DVB_BASE_BIN_H

#include <gst/gst.h>
#include <glib.h>
#include "camdevice.h"

G_BEGIN_DECLS

#define GST_TYPE_DVB_BASE_BIN \
  (dvb_base_bin_get_type())
#define GST_DVB_BASE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVB_BASE_BIN,DvbBaseBin))
#define GST_DVB_BASE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVB_BASE_BIN,DvbBaseBinClass))
#define GST_IS_DVB_BASE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVB_BASE_BIN))
#define GST_IS_DVB_BASE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVB_BASE_BIN))

typedef struct _DvbBaseBin DvbBaseBin;
typedef struct _DvbBaseBinClass DvbBaseBinClass;

struct _DvbBaseBin {
  GstBin bin;

  GstElement *dvbsrc;
  GstElement *buffer_queue;
  GstElement *tsparse;
  CamDevice *hwcam;
  gboolean trycam;
  GList *pmtlist;
  gboolean pmtlist_changed;
  gchar *filter;
  GHashTable *streams;
  GHashTable *programs;
  gboolean disposed;

  GstTask *task;
  GstPoll *poll;
  GRecMutex lock;

  /* Cached value */
  gchar *program_numbers;
};

struct _DvbBaseBinClass {
  GstBinClass parent_class;

  /* signals */
  void (*do_tune) (DvbBaseBin * dvbbasebin);
};

GType dvb_base_bin_get_type(void);

G_END_DECLS

#endif /* GST_DVB_BASE_BIN_H */
