/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Field implementation
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
#include "config.h"

/**
 * SECTION:gifieldinfo
 * @title: GIFieldInfo
 * @short_description: Struct representing a struct or union field
 *
 * A GIFieldInfo struct represents a field of a struct, union, or object.
 *
 * The GIFieldInfo is fetched by calling g_struct_info_get_field(),
 * g_union_info_get_field() or g_object_info_get_field().
 *
 * A field has a size, type and a struct offset asssociated and a set of flags,
 * which are currently #GI_FIELD_IS_READABLE or #GI_FIELD_IS_WRITABLE.
 *
 * See also: #GIStructInfo, #GIUnionInfo, #GIObjectInfo
 */

/**
 * g_field_info_get_flags:
 * @info: a #GIFieldInfo
 *
 * Obtain the flags for this #GIFieldInfo. See #GIFieldInfoFlags for possible
 * flag values.
 *
 * Returns: the flags
 */
GIFieldInfoFlags
g_field_info_get_flags (GIFieldInfo *info)
{
  GIFieldInfoFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_FIELD_INFO (info), 0);

  blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->readable)
    flags = flags | GI_FIELD_IS_READABLE;

  if (blob->writable)
    flags = flags | GI_FIELD_IS_WRITABLE;

  return flags;
}

/**
 * g_field_info_get_size:
 * @info: a #GIFieldInfo
 *
 * Obtain the size in bits of the field member, this is how
 * much space you need to allocate to store the field.
 *
 * Returns: the field size
 */
gint
g_field_info_get_size (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_FIELD_INFO (info), 0);

  blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->bits;
}

/**
 * g_field_info_get_offset:
 * @info: a #GIFieldInfo
 *
 * Obtain the offset in bytes of the field member, this is relative
 * to the beginning of the struct or union.
 *
 * Returns: the field offset
 */
gint
g_field_info_get_offset (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_FIELD_INFO (info), 0);

  blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

/**
 * g_field_info_get_type:
 * @info: a #GIFieldInfo
 *
 * Obtain the type of a field as a #GITypeInfo.
 *
 * Returns: (transfer full): the #GITypeInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GITypeInfo *
g_field_info_get_type (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  FieldBlob *blob;
  GIRealInfo *type_info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FIELD_INFO (info), NULL);

  blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->has_embedded_type)
    {
      type_info = (GIRealInfo *) g_info_new (GI_INFO_TYPE_TYPE,
                                                (GIBaseInfo*)info, rinfo->typelib,
                                                rinfo->offset + header->field_blob_size);
      type_info->type_is_embedded = TRUE;
    }
  else
    return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (FieldBlob, type));

  return (GIBaseInfo*)type_info;
}

/**
 * g_field_info_get_field: (skip)
 * @field_info: a #GIFieldInfo
 * @mem: pointer to a block of memory representing a C structure or union
 * @value: a #GIArgument into which to store the value retrieved
 *
 * Reads a field identified by a #GIFieldInfo from a C structure or
 * union.  This only handles fields of simple C types. It will fail
 * for a field of a composite type like a nested structure or union
 * even if that is actually readable.
 *
 * Returns: %TRUE if reading the field succeeded, otherwise %FALSE
 */
gboolean
g_field_info_get_field (GIFieldInfo *field_info,
			gpointer     mem,
			GIArgument   *value)
{
  int offset;
  GITypeInfo *type_info;
  gboolean result = FALSE;

  g_return_val_if_fail (field_info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_FIELD_INFO (field_info), FALSE);

  if ((g_field_info_get_flags (field_info) & GI_FIELD_IS_READABLE) == 0)
    return FALSE;

  offset = g_field_info_get_offset (field_info);
  type_info = g_field_info_get_type (field_info);

  if (g_type_info_is_pointer (type_info))
    {
      value->v_pointer = G_STRUCT_MEMBER (gpointer, mem, offset);
      result = TRUE;
    }
  else
    {
      switch (g_type_info_get_tag (type_info))
	{
	case GI_TYPE_TAG_VOID:
	  g_warning("Field %s: should not be have void type",
		    g_base_info_get_name ((GIBaseInfo *)field_info));
	  break;
	case GI_TYPE_TAG_BOOLEAN:
	  value->v_boolean = G_STRUCT_MEMBER (gboolean, mem, offset) != FALSE;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT8:
	case GI_TYPE_TAG_UINT8:
	  value->v_uint8 = G_STRUCT_MEMBER (guint8, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT16:
	case GI_TYPE_TAG_UINT16:
	  value->v_uint16 = G_STRUCT_MEMBER (guint16, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT32:
	case GI_TYPE_TAG_UINT32:
	case GI_TYPE_TAG_UNICHAR:
	  value->v_uint32 = G_STRUCT_MEMBER (guint32, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT64:
	case GI_TYPE_TAG_UINT64:
	  value->v_uint64 = G_STRUCT_MEMBER (guint64, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_GTYPE:
	  value->v_size = G_STRUCT_MEMBER (gsize, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_FLOAT:
	  value->v_float = G_STRUCT_MEMBER (gfloat, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_DOUBLE:
	  value->v_double = G_STRUCT_MEMBER (gdouble, mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_ARRAY:
	  /* We don't check the array type and that it is fixed-size,
	     we trust g-ir-compiler to do the right thing */
	  value->v_pointer = G_STRUCT_MEMBER_P (mem, offset);
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_UTF8:
	case GI_TYPE_TAG_FILENAME:
	case GI_TYPE_TAG_GLIST:
	case GI_TYPE_TAG_GSLIST:
	case GI_TYPE_TAG_GHASH:
	  g_warning("Field %s: type %s should have is_pointer set",
		    g_base_info_get_name ((GIBaseInfo *)field_info),
		    g_type_tag_to_string (g_type_info_get_tag (type_info)));
	  break;
	case GI_TYPE_TAG_ERROR:
	  /* Needs to be handled by the language binding directly */
	  break;
	case GI_TYPE_TAG_INTERFACE:
	  {
	    GIBaseInfo *interface = g_type_info_get_interface (type_info);
	    switch (g_base_info_get_type (interface))
	      {
	      case GI_INFO_TYPE_STRUCT:
	      case GI_INFO_TYPE_UNION:
	      case GI_INFO_TYPE_BOXED:
		/* Needs to be handled by the language binding directly */
		break;
	      case GI_INFO_TYPE_OBJECT:
		break;
	      case GI_INFO_TYPE_ENUM:
	      case GI_INFO_TYPE_FLAGS:
		{
		  /* FIXME: there's a mismatch here between the value->v_int we use
		   * here and the gint64 result returned from g_value_info_get_value().
		   * But to switch this to gint64, we'd have to make g_function_info_invoke()
		   * translate value->v_int64 to the proper ABI for an enum function
		   * call parameter, which will usually be int, and then fix up language
		   * bindings.
		   */
		  GITypeTag storage_type = g_enum_info_get_storage_type ((GIEnumInfo *)interface);
		  switch (storage_type)
		    {
		    case GI_TYPE_TAG_INT8:
		    case GI_TYPE_TAG_UINT8:
		      value->v_int = (gint)G_STRUCT_MEMBER (guint8, mem, offset);
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT16:
		    case GI_TYPE_TAG_UINT16:
		      value->v_int = (gint)G_STRUCT_MEMBER (guint16, mem, offset);
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT32:
		    case GI_TYPE_TAG_UINT32:
		      value->v_int = (gint)G_STRUCT_MEMBER (guint32, mem, offset);
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT64:
		    case GI_TYPE_TAG_UINT64:
		      value->v_int = (gint)G_STRUCT_MEMBER (guint64, mem, offset);
		      result = TRUE;
		      break;
		    default:
		      g_warning("Field %s: Unexpected enum storage type %s",
				g_base_info_get_name ((GIBaseInfo *)field_info),
				g_type_tag_to_string (storage_type));
		      break;
		    }
		  break;
		}
	      case GI_INFO_TYPE_VFUNC:
	      case GI_INFO_TYPE_CALLBACK:
		g_warning("Field %s: Interface type %d should have is_pointer set",
			  g_base_info_get_name ((GIBaseInfo *)field_info),
			  g_base_info_get_type (interface));
		break;
	      case GI_INFO_TYPE_INVALID:
	      case GI_INFO_TYPE_INTERFACE:
	      case GI_INFO_TYPE_FUNCTION:
	      case GI_INFO_TYPE_CONSTANT:
	      case GI_INFO_TYPE_INVALID_0:
	      case GI_INFO_TYPE_VALUE:
	      case GI_INFO_TYPE_SIGNAL:
	      case GI_INFO_TYPE_PROPERTY:
	      case GI_INFO_TYPE_FIELD:
	      case GI_INFO_TYPE_ARG:
	      case GI_INFO_TYPE_TYPE:
	      case GI_INFO_TYPE_UNRESOLVED:
		g_warning("Field %s: Interface type %d not expected",
			  g_base_info_get_name ((GIBaseInfo *)field_info),
			  g_base_info_get_type (interface));
		break;
	      default:
		break;
	      }

	    g_base_info_unref ((GIBaseInfo *)interface);
	    break;
	  }
	  break;
	default:
	  break;
	}
    }

  g_base_info_unref ((GIBaseInfo *)type_info);

  return result;
}

/**
 * g_field_info_set_field: (skip)
 * @field_info: a #GIFieldInfo
 * @mem: pointer to a block of memory representing a C structure or union
 * @value: a #GIArgument holding the value to store
 *
 * Writes a field identified by a #GIFieldInfo to a C structure or
 * union.  This only handles fields of simple C types. It will fail
 * for a field of a composite type like a nested structure or union
 * even if that is actually writable. Note also that that it will refuse
 * to write fields where memory management would by required. A field
 * with a type such as 'char *' must be set with a setter function.
 *
 * Returns: %TRUE if writing the field succeeded, otherwise %FALSE
 */
gboolean
g_field_info_set_field (GIFieldInfo     *field_info,
			gpointer         mem,
			const GIArgument *value)
{
  int offset;
  GITypeInfo *type_info;
  gboolean result = FALSE;

  g_return_val_if_fail (field_info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_FIELD_INFO (field_info), FALSE);

  if ((g_field_info_get_flags (field_info) & GI_FIELD_IS_WRITABLE) == 0)
    return FALSE;

  offset = g_field_info_get_offset (field_info);
  type_info = g_field_info_get_type (field_info);

  if (!g_type_info_is_pointer (type_info))
    {
      switch (g_type_info_get_tag (type_info))
	{
	case GI_TYPE_TAG_VOID:
	  g_warning("Field %s: should not be have void type",
		    g_base_info_get_name ((GIBaseInfo *)field_info));
	  break;
	case GI_TYPE_TAG_BOOLEAN:
	  G_STRUCT_MEMBER (gboolean, mem, offset) = value->v_boolean != FALSE;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT8:
	case GI_TYPE_TAG_UINT8:
	  G_STRUCT_MEMBER (guint8, mem, offset) = value->v_uint8;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT16:
	case GI_TYPE_TAG_UINT16:
	  G_STRUCT_MEMBER (guint16, mem, offset) = value->v_uint16;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT32:
	case GI_TYPE_TAG_UINT32:
	case GI_TYPE_TAG_UNICHAR:
	  G_STRUCT_MEMBER (guint32, mem, offset) = value->v_uint32;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_INT64:
	case GI_TYPE_TAG_UINT64:
	  G_STRUCT_MEMBER (guint64, mem, offset) = value->v_uint64;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_GTYPE:
	  G_STRUCT_MEMBER (gsize, mem, offset) = value->v_size;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_FLOAT:
	  G_STRUCT_MEMBER (gfloat, mem, offset) = value->v_float;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_DOUBLE:
	  G_STRUCT_MEMBER (gdouble, mem, offset)= value->v_double;
	  result = TRUE;
	  break;
	case GI_TYPE_TAG_UTF8:
	case GI_TYPE_TAG_FILENAME:
	case GI_TYPE_TAG_ARRAY:
	case GI_TYPE_TAG_GLIST:
	case GI_TYPE_TAG_GSLIST:
	case GI_TYPE_TAG_GHASH:
	  g_warning("Field %s: type %s should have is_pointer set",
		    g_base_info_get_name ((GIBaseInfo *)field_info),
		    g_type_tag_to_string (g_type_info_get_tag (type_info)));
	  break;
	case GI_TYPE_TAG_ERROR:
	  /* Needs to be handled by the language binding directly */
	  break;
	case GI_TYPE_TAG_INTERFACE:
	  {
	    GIBaseInfo *interface = g_type_info_get_interface (type_info);
	    switch (g_base_info_get_type (interface))
	      {
	      case GI_INFO_TYPE_STRUCT:
	      case GI_INFO_TYPE_UNION:
	      case GI_INFO_TYPE_BOXED:
		/* Needs to be handled by the language binding directly */
		break;
	      case GI_INFO_TYPE_OBJECT:
		break;
	      case GI_INFO_TYPE_ENUM:
	      case GI_INFO_TYPE_FLAGS:
		{
		  /* See FIXME above
		   */
		  GITypeTag storage_type = g_enum_info_get_storage_type ((GIEnumInfo *)interface);
		  switch (storage_type)
		    {
		    case GI_TYPE_TAG_INT8:
		    case GI_TYPE_TAG_UINT8:
		      G_STRUCT_MEMBER (guint8, mem, offset) = (guint8)value->v_int;
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT16:
		    case GI_TYPE_TAG_UINT16:
		      G_STRUCT_MEMBER (guint16, mem, offset) = (guint16)value->v_int;
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT32:
		    case GI_TYPE_TAG_UINT32:
		      G_STRUCT_MEMBER (guint32, mem, offset) = (guint32)value->v_int;
		      result = TRUE;
		      break;
		    case GI_TYPE_TAG_INT64:
		    case GI_TYPE_TAG_UINT64:
		      G_STRUCT_MEMBER (guint64, mem, offset) = (guint64)value->v_int;
		      result = TRUE;
		      break;
		    default:
		      g_warning("Field %s: Unexpected enum storage type %s",
				g_base_info_get_name ((GIBaseInfo *)field_info),
				g_type_tag_to_string (storage_type));
		      break;
		    }
		  break;
		}
		break;
	      case GI_INFO_TYPE_VFUNC:
	      case GI_INFO_TYPE_CALLBACK:
		g_warning("Field%s: Interface type %d should have is_pointer set",
			  g_base_info_get_name ((GIBaseInfo *)field_info),
			  g_base_info_get_type (interface));
		break;
	      case GI_INFO_TYPE_INVALID:
	      case GI_INFO_TYPE_INTERFACE:
	      case GI_INFO_TYPE_FUNCTION:
	      case GI_INFO_TYPE_CONSTANT:
	      case GI_INFO_TYPE_INVALID_0:
	      case GI_INFO_TYPE_VALUE:
	      case GI_INFO_TYPE_SIGNAL:
	      case GI_INFO_TYPE_PROPERTY:
	      case GI_INFO_TYPE_FIELD:
	      case GI_INFO_TYPE_ARG:
	      case GI_INFO_TYPE_TYPE:
	      case GI_INFO_TYPE_UNRESOLVED:
		g_warning("Field %s: Interface type %d not expected",
			  g_base_info_get_name ((GIBaseInfo *)field_info),
			  g_base_info_get_type (interface));
		break;
	      default:
		break;
	      }

	    g_base_info_unref ((GIBaseInfo *)interface);
	    break;
	  }
	  break;
	default:
	  break;
	}
    } else {
      switch (g_type_info_get_tag (type_info))
        {
        case GI_TYPE_TAG_INTERFACE:
          {
	    GIBaseInfo *interface = g_type_info_get_interface (type_info);
	    switch (g_base_info_get_type (interface))
              {
                case GI_INFO_TYPE_OBJECT:
                case GI_INFO_TYPE_INTERFACE:
                  G_STRUCT_MEMBER (gpointer, mem, offset) = (gpointer)value->v_pointer;
                  result = TRUE;
                  break;
	        default:
		  break;
              }
              g_base_info_unref ((GIBaseInfo *)interface);
          }
	default:
	  break;
        }
    }

  g_base_info_unref ((GIBaseInfo *)type_info);

  return result;
}
