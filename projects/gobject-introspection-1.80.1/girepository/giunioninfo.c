/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Union implementation
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

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/**
 * SECTION:giunioninfo
 * @title: GIUnionInfo
 * @short_description: Struct representing a union.
 *
 * GIUnionInfo represents a union type.
 *
 * A union has methods and fields.  Unions can optionally have a
 * discriminator, which is a field deciding what type of real union
 * fields is valid for specified instance.
 */

/**
 * g_union_info_get_n_fields:
 * @info: a #GIUnionInfo
 *
 * Obtain the number of fields this union has.
 *
 * Returns: number of fields
 */
gint
g_union_info_get_n_fields  (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

/**
 * g_union_info_get_field:
 * @info: a #GIUnionInfo
 * @n: a field index
 *
 * Obtain the type information for field with specified index.
 *
 * Returns: (transfer full): the #GIFieldInfo, free it with g_base_info_unref()
 * when done.
 */
GIFieldInfo *
g_union_info_get_field (GIUnionInfo *info,
			gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
				     rinfo->offset + header->union_blob_size +
				     n * header->field_blob_size);
}

/**
 * g_union_info_get_n_methods:
 * @info: a #GIUnionInfo
 *
 * Obtain the number of methods this union has.
 *
 * Returns: number of methods
 */
gint
g_union_info_get_n_methods (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_functions;
}

/**
 * g_union_info_get_method:
 * @info: a #GIUnionInfo
 * @n: a method index
 *
 * Obtain the type information for method with specified index.
 *
 * Returns: (transfer full): the #GIFunctionInfo, free it with g_base_info_unref()
 * when done.
 */
GIFunctionInfo *
g_union_info_get_method (GIUnionInfo *info,
			 gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size
    + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_union_info_is_discriminated:
 * @info: a #GIUnionInfo
 *
 * Return true if this union contains discriminator field.
 *
 * Returns: %TRUE if this is a discriminated union, %FALSE otherwise
 */
gboolean
g_union_info_is_discriminated (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminated;
}

/**
 * g_union_info_get_discriminator_offset:
 * @info: a #GIUnionInfo
 *
 * Returns offset of the discriminator field in the structure.
 *
 * Returns: offset in bytes of the discriminator
 */
gint
g_union_info_get_discriminator_offset (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminator_offset;
}

/**
 * g_union_info_get_discriminator_type:
 * @info: a #GIUnionInfo
 *
 * Obtain the type information of the union discriminator.
 *
 * Returns: (transfer full): the #GITypeInfo, free it with g_base_info_unref()
 * when done.
 */
GITypeInfo *
g_union_info_get_discriminator_type (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 24);
}

/**
 * g_union_info_get_discriminator:
 * @info: a #GIUnionInfo
 * @n: a union field index
 *
 * Obtain discriminator value assigned for n-th union field, i.e. n-th
 * union field is the active one if discriminator contains this
 * constant.
 *
 * Returns: (transfer full): the #GIConstantInfo, free it with g_base_info_unref()
 * when done.
 */
GIConstantInfo *
g_union_info_get_discriminator (GIUnionInfo *info,
				gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->discriminated)
    {
      Header *header = (Header *)rinfo->typelib->data;
      gint offset;

      offset = rinfo->offset + header->union_blob_size
	+ blob->n_fields * header->field_blob_size
	+ blob->n_functions * header->function_blob_size
	+ n * header->constant_blob_size;

      return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					    rinfo->typelib, offset);
    }

  return NULL;
}

/**
 * g_union_info_find_method:
 * @info: a #GIUnionInfo
 * @name: a method name
 *
 * Obtain the type information for method named @name.
 *
 * Returns: (transfer full): the #GIFunctionInfo, free it with g_base_info_unref()
 * when done.
 */
GIFunctionInfo *
g_union_info_find_method (GIUnionInfo *info,
                          const gchar *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size;

  return _g_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_functions, name);
}

/**
 * g_union_info_get_size:
 * @info: a #GIUnionInfo
 *
 * Obtain the total size of the union.
 *
 * Returns: size of the union in bytes
 */
gsize
g_union_info_get_size (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

/**
 * g_union_info_get_alignment:
 * @info: a #GIUnionInfo
 *
 * Obtain the required alignment of the union.
 *
 * Returns: required alignment in bytes
 */
gsize
g_union_info_get_alignment (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}

/**
 * g_union_info_get_copy_function:
 * @info: a union information blob
 *
 * Retrieves the name of the copy function for @info, if any is set.
 *
 * Returns: (transfer none) (nullable): the name of the copy function
 *
 * Since: 1.76
 */
const char *
g_union_info_get_copy_function (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_UNION_INFO (info), NULL);

  blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->copy_func)
    return g_typelib_get_string (rinfo->typelib, blob->copy_func);

  return NULL;
}

/**
 * g_union_info_get_free_function:
 * @info: a union information blob
 *
 * Retrieves the name of the free function for @info, if any is set.
 *
 * Returns: (transfer none) (nullable): the name of the free function
 *
 * Since: 1.76
 */
const char *
g_union_info_get_free_function (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_UNION_INFO (info), NULL);

  blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->free_func)
    return g_typelib_get_string (rinfo->typelib, blob->free_func);

  return NULL;
}
