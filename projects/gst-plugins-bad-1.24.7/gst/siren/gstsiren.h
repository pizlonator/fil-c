/*
 * Siren encoding/Decoder library
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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
 *
 */

#ifndef __GST_SIREN_H__
#define __GST_SIREN_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_SIREN \
  (gst_siren_get_type())
#define GST_SIREN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_SIREN,GstSiren))
#define GST_SIREN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_SIREN,GstSirenClass))
#define GST_IS_SIREN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIREN))
#define GST_IS_SIREN_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIREN))

typedef struct _GstSiren GstSiren;
typedef struct _GstSirenClass GstSirenClass;

struct _GstSiren
{
  GstBaseTransform parent;
};

struct _GstSirenClass
{
  GstBaseTransformClass parent_class;
};

GType gst_siren_get_type (void);

G_END_DECLS
#endif /* __GST_SIREN_H__ */
