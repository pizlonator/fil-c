/*
 * Farsight Voice+Video library
 *
 *  Copyright 2006 Collabora Ltd,
 *  Copyright 2006 Nokia Corporation
 *   @author: Philippe Kalaf <philippe.kalaf@collabora.co.uk>.
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
 *
 */

#ifndef __GST_NET_SIM_H__
#define __GST_NET_SIM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #define's don't like whitespacey bits */
#define GST_TYPE_NET_SIM \
  (gst_net_sim_get_type())
#define GST_NET_SIM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_NET_SIM,GstNetSim))
#define GST_NET_SIM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_NET_SIM,GstNetSimClass))
#define GST_IS_NET_SIM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NET_SIM))
#define GST_IS_NET_SIM_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NET_SIM))

typedef struct _GstNetSim GstNetSim;
typedef struct _GstNetSimClass GstNetSimClass;

typedef enum
{
  DISTRIBUTION_UNIFORM,
  DISTRIBUTION_NORMAL,
  DISTRIBUTION_GAMMA
} GstNetSimDistribution;

typedef struct
{
  gboolean generate;
  gdouble z0;
  gdouble z1;
} NormalDistributionState;

struct _GstNetSim
{
  GstElement parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  GMutex loop_mutex;
  GCond start_cond;
  GMainLoop *main_loop;
  gboolean running;
  GRand *rand_seed;
  gsize bucket_size;
  GstClockTime prev_time;
  NormalDistributionState delay_state;
  gint64 last_ready_time;

  /* properties */
  gint min_delay;
  gint max_delay;
  GstNetSimDistribution delay_distribution;
  gfloat delay_probability;
  gfloat drop_probability;
  gfloat duplicate_probability;
  guint drop_packets;
  gint max_kbps;
  gint max_bucket_size;
  gboolean allow_reordering;
};

struct _GstNetSimClass
{
  GstElementClass parent_class;
};

GType gst_net_sim_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (netsim);

G_END_DECLS

#endif /* __GST_NET_SIM_H__ */
