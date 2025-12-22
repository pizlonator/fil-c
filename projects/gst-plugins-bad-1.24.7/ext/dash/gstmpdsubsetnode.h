/* GStreamer
 *
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GSTMPDSUBSETNODE_H__
#define __GSTMPDSUBSETNODE_H__

#include <gst/gst.h>
#include "gstmpdnode.h"

G_BEGIN_DECLS

#define GST_TYPE_MPD_SUBSET_NODE gst_mpd_subset_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDSubsetNode, gst_mpd_subset_node, GST, MPD_SUBSET_NODE, GstMPDNode)

struct _GstMPDSubsetNode
{
  GstObject parent_instance;
  guint *contains;                   /* UIntVectorType */
  guint contains_size;               /* size of the "contains" array */
};

struct _GstMPDSubsetNodeClass {
  GstMPDNodeClass parent_class;
};


GstMPDSubsetNode * gst_mpd_subset_node_new (void);
void gst_mpd_subset_node_free (GstMPDSubsetNode* self);

G_END_DECLS

#endif /* __GSTMPDSUBSETNODE_H__ */
