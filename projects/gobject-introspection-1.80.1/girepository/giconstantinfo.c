/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Constant implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <string.h> // memcpy

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/**
 * SECTION:giconstantinfo
 * @title: GIConstantInfo
 * @short_description: Struct representing a constant
 *
 * GIConstantInfo represents a constant.
 *
 * A constant has a type associated which can be obtained by calling
 * g_constant_info_get_type() and a value, which can be obtained by
 * calling g_constant_info_get_value().
 */


/**
 * g_constant_info_get_type:
 * @info: a #GIConstantInfo
 *
 * Obtain the type of the constant as a #GITypeInfo.
 *
 * Returns: (transfer full): the #GITypeInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GITypeInfo *
g_constant_info_get_type (GIConstantInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_CONSTANT_INFO (info), NULL);

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 8);
}

#define DO_ALIGNED_COPY(dest_addr, src_addr, type) \
        memcpy((dest_addr), (src_addr), sizeof(type))

/**
 * g_constant_info_free_value: (skip)
 * @info: a #GIConstantInfo
 * @value: the argument
 *
 * Free the value returned from g_constant_info_get_value().
 *
 * Since: 1.32
 */
void
g_constant_info_free_value (GIConstantInfo *info,
                            GIArgument     *value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ConstantBlob *blob;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_CONSTANT_INFO (info));

  blob = (ConstantBlob *)&rinfo->typelib->data[rinfo->offset];

  /* FIXME non-basic types ? */
  if (blob->type.flags.reserved == 0 && blob->type.flags.reserved2 == 0)
    {
      if (blob->type.flags.pointer)
        g_free (value->v_pointer);
    }
}

/**
 * g_constant_info_get_value: (skip)
 * @info: a #GIConstantInfo
 * @value: (out): an argument
 *
 * Obtain the value associated with the #GIConstantInfo and store it in the
 * @value parameter. @argument needs to be allocated before passing it in.
 * The size of the constant value stored in @argument will be returned.
 * Free the value with g_constant_info_free_value().
 *
 * Returns: size of the constant
 */
gint
g_constant_info_get_value (GIConstantInfo *info,
			   GIArgument      *value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ConstantBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_CONSTANT_INFO (info), 0);

  blob = (ConstantBlob *)&rinfo->typelib->data[rinfo->offset];

  /* FIXME non-basic types ? */
  if (blob->type.flags.reserved == 0 && blob->type.flags.reserved2 == 0)
    {
      if (blob->type.flags.pointer)
        {
#if GLIB_CHECK_VERSION (2, 67, 5)
          gsize blob_size = blob->size;

	  value->v_pointer = g_memdup2 (&rinfo->typelib->data[blob->offset], blob_size);
#else
	  value->v_pointer = g_memdup (&rinfo->typelib->data[blob->offset], blob->size);
#endif
        }
      else
	{
	  switch (blob->type.flags.tag)
	    {
	    case GI_TYPE_TAG_BOOLEAN:
	      value->v_boolean = *(gboolean*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT8:
	      value->v_int8 = *(gint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT8:
	      value->v_uint8 = *(guint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT16:
	      value->v_int16 = *(gint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT16:
	      value->v_uint16 = *(guint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT32:
	      value->v_int32 = *(gint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT32:
	      value->v_uint32 = *(guint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT64:
	      DO_ALIGNED_COPY(&value->v_int64, &rinfo->typelib->data[blob->offset], gint64);
	      break;
	    case GI_TYPE_TAG_UINT64:
	      DO_ALIGNED_COPY(&value->v_uint64, &rinfo->typelib->data[blob->offset], guint64);
	      break;
	    case GI_TYPE_TAG_FLOAT:
	      DO_ALIGNED_COPY(&value->v_float, &rinfo->typelib->data[blob->offset], gfloat);
	      break;
	    case GI_TYPE_TAG_DOUBLE:
	      DO_ALIGNED_COPY(&value->v_double, &rinfo->typelib->data[blob->offset], gdouble);
	      break;
	    default:
	      g_assert_not_reached ();
	    }
	}
    }

  return blob->size;
}

