/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Object implementation
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
 * SECTION:giobjectinfo
 * @title: GIObjectInfo
 * @short_description: Struct representing a classed type
 *
 * GIObjectInfo represents a classed type.
 *
 * Classed types in GType inherit from #GTypeInstance; the most common
 * type is #GObject.
 *
 * A GIObjectInfo doesn't represent a specific instance of a classed type,
 * instead this represent the object type (eg class).
 *
 * A GIObjectInfo has methods, fields, properties, signals, interfaces,
 * constants and virtual functions.
 */

/**
 * g_object_info_get_field_offset:
 * @info: a #GIObjectInfo
 * @n: index of queried field
 *
 * Obtain the offset of the specified field.
 *
 * Returns: field offset in bytes
 */
static gint32
g_object_info_get_field_offset (GIObjectInfo *info,
                                gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];
  guint32 offset;
  gint i;
  FieldBlob *field_blob;

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2;

  for (i = 0; i < n; i++)
    {
      field_blob = (FieldBlob *)&rinfo->typelib->data[offset];
      offset += header->field_blob_size;
      if (field_blob->has_embedded_type)
        offset += header->callback_blob_size;
    }

  return offset;
}

/**
 * g_object_info_get_parent:
 * @info: a #GIObjectInfo
 *
 * Obtain the parent of the object type.
 *
 * Returns: (transfer full) (nullable): the #GIObjectInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIObjectInfo *
g_object_info_get_parent (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->parent)
    return (GIObjectInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->parent);
  else
    return NULL;
}

/**
 * g_object_info_get_abstract:
 * @info: a #GIObjectInfo
 *
 * Obtain if the object type is an abstract type, eg if it cannot be
 * instantiated
 *
 * Returns: %TRUE if the object type is abstract
 */
gboolean
g_object_info_get_abstract (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), FALSE);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->abstract != 0;
}

/**
 * g_object_info_get_final:
 * @info: a #GIObjectInfo
 *
 * Checks whether the object type is a final type, i.e. if it cannot
 * be derived
 *
 * Returns: %TRUE if the object type is final
 *
 * Since: 1.70
 */
gboolean
g_object_info_get_final (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *) info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), FALSE);

  blob = (ObjectBlob *) &rinfo->typelib->data[rinfo->offset];

  return blob->final_ != 0;
}

/**
 * g_object_info_get_fundamental:
 * @info: a #GIObjectInfo
 *
 * Obtain if the object type is of a fundamental type which is not
 * G_TYPE_OBJECT. This is mostly for supporting GstMiniObject.
 *
 * Returns: %TRUE if the object type is a fundamental type
 */
gboolean
g_object_info_get_fundamental (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), FALSE);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->fundamental != 0;
}

/**
 * g_object_info_get_type_name:
 * @info: a #GIObjectInfo
 *
 * Obtain the name of the objects class/type.
 *
 * Returns: name of the objects type
 */
const gchar *
g_object_info_get_type_name (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_name);
}

/**
 * g_object_info_get_type_init:
 * @info: a #GIObjectInfo
 *
 * Obtain the function which when called will return the GType
 * function for which this object type is registered.
 *
 * Returns: the type init function
 */
const gchar *
g_object_info_get_type_init (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_init);
}

/**
 * g_object_info_get_n_interfaces:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of interfaces that this object type has.
 *
 * Returns: number of interfaces
 */
gint
g_object_info_get_n_interfaces (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_interfaces;
}

/**
 * g_object_info_get_interface:
 * @info: a #GIObjectInfo
 * @n: index of interface to get
 *
 * Obtain an object type interface at index @n.
 *
 * Returns: (transfer full): the #GIInterfaceInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIInterfaceInfo *
g_object_info_get_interface (GIObjectInfo *info,
			     gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
						 rinfo->typelib, blob->interfaces[n]);
}

/**
 * g_object_info_get_n_fields:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of fields that this object type has.
 *
 * Returns: number of fields
 */
gint
g_object_info_get_n_fields (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

/**
 * g_object_info_get_field:
 * @info: a #GIObjectInfo
 * @n: index of field to get
 *
 * Obtain an object type field at index @n.
 *
 * Returns: (transfer full): the #GIFieldInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFieldInfo *
g_object_info_get_field (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  offset = g_object_info_get_field_offset(info, n);

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * g_object_info_get_n_properties:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of properties that this object type has.
 *
 * Returns: number of properties
 */
gint
g_object_info_get_n_properties (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];
  return blob->n_properties;
}

/**
 * g_object_info_get_property:
 * @info: a #GIObjectInfo
 * @n: index of property to get
 *
 * Obtain an object type property at index @n.
 *
 * Returns: (transfer full): the #GIPropertyInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIPropertyInfo *
g_object_info_get_property (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_object_info_get_n_methods:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of methods that this object type has.
 *
 * Returns: number of methods
 */
gint
g_object_info_get_n_methods (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

/**
 * g_object_info_get_method:
 * @info: a #GIObjectInfo
 * @n: index of method to get
 *
 * Obtain an object type method at index @n.
 *
 * Returns: (transfer full): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_object_info_get_method (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];


  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

    return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					  rinfo->typelib, offset);
}

/**
 * g_object_info_find_method:
 * @info: a #GIObjectInfo
 * @name: name of method to obtain
 *
 * Obtain a method of the object type given a @name. %NULL will be
 * returned if there's no method available with that name.
 *
 * Returns: (transfer full) (nullable): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_object_info_find_method (GIObjectInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size +
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size;

  return _g_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

/**
 * g_object_info_find_method_using_interfaces:
 * @info: a #GIObjectInfo
 * @name: name of method to obtain
 * @implementor: (out) (transfer full): The implementor of the interface
 *
 * Obtain a method of the object given a @name, searching both the
 * object @info and any interfaces it implements.  %NULL will be
 * returned if there's no method available with that name.
 *
 * Note that this function does *not* search parent classes; you will have
 * to chain up if that's desired.
 *
 * Returns: (transfer full) (nullable): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_object_info_find_method_using_interfaces (GIObjectInfo  *info,
					    const gchar   *name,
					    GIObjectInfo **implementor)
{
  GIFunctionInfo *result = NULL;
  GIObjectInfo *implementor_result = NULL;

  result = g_object_info_find_method (info, name);
  if (result)
    implementor_result = g_base_info_ref ((GIBaseInfo*) info);

  if (result == NULL)
    {
      int n_interfaces;
      int i;

      n_interfaces = g_object_info_get_n_interfaces (info);
      for (i = 0; i < n_interfaces; ++i)
	{
	  GIInterfaceInfo *iface_info;

	  iface_info = g_object_info_get_interface (info, i);

	  result = g_interface_info_find_method (iface_info, name);

	  if (result != NULL)
	    {
	      implementor_result = iface_info;
	      break;
	    }
	  g_base_info_unref ((GIBaseInfo*) iface_info);
	}
    }
  if (implementor)
    *implementor = implementor_result;
  else if (implementor_result != NULL)
    g_base_info_unref ((GIBaseInfo*) implementor_result);
  return result;
}

/**
 * g_object_info_get_n_signals:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of signals that this object type has.
 *
 * Returns: number of signals
 */
gint
g_object_info_get_n_signals (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

/**
 * g_object_info_get_signal:
 * @info: a #GIObjectInfo
 * @n: index of signal to get
 *
 * Obtain an object type signal at index @n.
 *
 * Returns: (transfer full): the #GISignalInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GISignalInfo *
g_object_info_get_signal (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

/**
 * g_object_info_find_signal:
 * @info: a #GIObjectInfo
 * @name: Name of signal
 *
 * TODO
 *
 * Returns: (transfer full) (nullable): Info for the signal with name @name in @info, or %NULL on failure.
 */
GISignalInfo *
g_object_info_find_signal (GIObjectInfo *info,
			   const gchar  *name)
{
  gint n_signals;
  gint i;

  n_signals = g_object_info_get_n_signals (info);
  for (i = 0; i < n_signals; i++)
    {
      GISignalInfo *siginfo = g_object_info_get_signal (info, i);

      if (g_strcmp0 (g_base_info_get_name (siginfo), name) != 0)
	{
	  g_base_info_unref ((GIBaseInfo*)siginfo);
	  continue;
	}

      return siginfo;
    }
  return NULL;
}


/**
 * g_object_info_get_n_vfuncs:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of virtual functions that this object type has.
 *
 * Returns: number of virtual functions
 */
gint
g_object_info_get_n_vfuncs (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

/**
 * g_object_info_get_vfunc:
 * @info: a #GIObjectInfo
 * @n: index of virtual function to get
 *
 * Obtain an object type virtual function at index @n.
 *
 * Returns: (transfer full): the #GIVFuncInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_get_vfunc (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

/**
 * g_object_info_find_vfunc:
 * @info: a #GIObjectInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. Note that the namespace
 * for virtuals is distinct from that of methods; there may or may not be
 * a concrete method associated for a virtual. If there is one, it may
 * be retrieved using g_vfunc_info_get_invoker(), otherwise %NULL will be
 * returned.
 * See the documentation for g_vfunc_info_get_invoker() for more
 * information on invoking virtuals.
 *
 * Returns: (transfer full) (nullable): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_find_vfunc (GIObjectInfo *info,
                          const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return _g_base_info_find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

/**
 * g_object_info_find_vfunc_using_interfaces:
 * @info: a #GIObjectInfo
 * @name: name of vfunc to obtain
 * @implementor: (out) (transfer full): The implementor of the interface
 *
 * Locate a virtual function slot with name @name, searching both the object
 * @info and any interfaces it implements.  Note that the namespace for
 * virtuals is distinct from that of methods; there may or may not be a
 * concrete method associated for a virtual. If there is one, it may be
 * retrieved using g_vfunc_info_get_invoker(), otherwise %NULL will be
 * returned.
 *
 * Note that this function does *not* search parent classes; you will have
 * to chain up if that's desired.
 *
 * Returns: (transfer full) (nullable): the #GIVFuncInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_find_vfunc_using_interfaces (GIObjectInfo  *info,
                                           const gchar   *name,
                                           GIObjectInfo **implementor)
{
  GIVFuncInfo *result = NULL;
  GIObjectInfo *implementor_result = NULL;

  result = g_object_info_find_vfunc (info, name);
  if (result)
    implementor_result = g_base_info_ref ((GIBaseInfo*) info);

  if (result == NULL)
    {
      int n_interfaces;
      int i;

      n_interfaces = g_object_info_get_n_interfaces (info);
      for (i = 0; i < n_interfaces; ++i)
	{
	  GIInterfaceInfo *iface_info;

	  iface_info = g_object_info_get_interface (info, i);

	  result = g_interface_info_find_vfunc (iface_info, name);

	  if (result != NULL)
	    {
	      implementor_result = iface_info;
	      break;
	    }
	  g_base_info_unref ((GIBaseInfo*) iface_info);
	}
    }
  if (implementor)
    *implementor = implementor_result;
  else if (implementor_result != NULL)
    g_base_info_unref ((GIBaseInfo*) implementor_result);
  return result;
}

/**
 * g_object_info_get_n_constants:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of constants that this object type has.
 *
 * Returns: number of constants
 */
gint
g_object_info_get_n_constants (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

/**
 * g_object_info_get_constant:
 * @info: a #GIObjectInfo
 * @n: index of constant to get
 *
 * Obtain an object type constant at index @n.
 *
 * Returns: (transfer full): the #GIConstantInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIConstantInfo *
g_object_info_get_constant (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_field_callbacks * header->callback_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_object_info_get_class_struct:
 * @info: a #GIObjectInfo
 *
 * Every #GObject has two structures; an instance structure and a class
 * structure.  This function returns the metadata for the class structure.
 *
 * Returns: (transfer full) (nullable): the #GIStructInfo or %NULL. Free with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_object_info_get_class_struct (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

typedef const char* (*SymbolGetter) (GIObjectInfo *info);

static void *
_get_func(GIObjectInfo *info,
          SymbolGetter getter)
{
  const char* symbol;
  GSList *parents = NULL, *l;
  GIObjectInfo *parent_info;
  gpointer func = NULL;

  parent_info = g_base_info_ref (info);
  while (parent_info != NULL)
    {
      parents = g_slist_prepend (parents, parent_info);
      parent_info = g_object_info_get_parent (parent_info);
    }

  for (l = parents; l; l = l->next)
    {
      parent_info = l->data;
      symbol = getter (parent_info);
      if (symbol == NULL)
        continue;

      g_typelib_symbol (((GIRealInfo *)parent_info)->typelib, symbol, (gpointer*) &func);
      if (func)
        break;
    }

  g_slist_free_full (parents, (GDestroyNotify) g_base_info_unref);
  return func;
}

/**
 * g_object_info_get_ref_function:
 * @info: a #GIObjectInfo
 *
 * Obtain the symbol name of the function that should be called to ref this
 * object type. It's mainly used fundamental types. The type signature for
 * the symbol is %GIObjectInfoRefFunction, to fetch the function pointer
 * see g_object_info_get_ref_function().
 *
 * Returns: (nullable): the symbol or %NULL
 */
const char *
g_object_info_get_ref_function (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->ref_func)
    return g_typelib_get_string (rinfo->typelib, blob->ref_func);

  return NULL;
}

/**
 * g_object_info_get_ref_function_pointer: (skip)
 * @info: a #GIObjectInfo
 *
 * Obtain a pointer to a function which can be used to
 * increase the reference count an instance of this object type.
 * This takes derivation into account and will reversely traverse
 * the base classes of this type, starting at the top type.
 *
 * Returns: (nullable): the function pointer or %NULL
 */
GIObjectInfoRefFunction
g_object_info_get_ref_function_pointer (GIObjectInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  return (GIObjectInfoRefFunction)_get_func(info, (SymbolGetter)g_object_info_get_ref_function);
}

/**
 * g_object_info_get_unref_function:
 * @info: a #GIObjectInfo
 *
 * Obtain the symbol name of the function that should be called to unref this
 * object type. It's mainly used fundamental types. The type signature for
 * the symbol is %GIObjectInfoUnrefFunction, to fetch the function pointer
 * see g_object_info_get_unref_function().
 *
 * Returns: (nullable): the symbol or %NULL
 */
const char *
g_object_info_get_unref_function (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->unref_func)
    return g_typelib_get_string (rinfo->typelib, blob->unref_func);

  return NULL;
}

/**
 * g_object_info_get_unref_function_pointer: (skip)
 * @info: a #GIObjectInfo
 *
 * Obtain a pointer to a function which can be used to
 * decrease the reference count an instance of this object type.
 * This takes derivation into account and will reversely traverse
 * the base classes of this type, starting at the top type.
 *
 * Returns: (nullable): the function pointer or %NULL
 */
GIObjectInfoUnrefFunction
g_object_info_get_unref_function_pointer (GIObjectInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  return (GIObjectInfoUnrefFunction)_get_func(info, (SymbolGetter)g_object_info_get_unref_function);
}

/**
 * g_object_info_get_set_value_function:
 * @info: a #GIObjectInfo
 *
 * Obtain the symbol name of the function that should be called to convert
 * set a GValue giving an object instance pointer of this object type.
 * I's mainly used fundamental types. The type signature for the symbol
 * is %GIObjectInfoSetValueFunction, to fetch the function pointer
 * see g_object_info_get_set_value_function().
 *
 * Returns: (nullable): the symbol or %NULL
 */
const char *
g_object_info_get_set_value_function (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->set_value_func)
    return g_typelib_get_string (rinfo->typelib, blob->set_value_func);

  return NULL;
}

/**
 * g_object_info_get_set_value_function_pointer: (skip)
 * @info: a #GIObjectInfo
 *
 * Obtain a pointer to a function which can be used to
 * set a GValue given an instance of this object type.
 * This takes derivation into account and will reversely traverse
 * the base classes of this type, starting at the top type.
 *
 * Returns: (nullable): the function pointer or %NULL
 */
GIObjectInfoSetValueFunction
g_object_info_get_set_value_function_pointer (GIObjectInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  return (GIObjectInfoSetValueFunction)_get_func(info, (SymbolGetter)g_object_info_get_set_value_function);
}

/**
 * g_object_info_get_get_value_function:
 * @info: a #GIObjectInfo
 *
 * Obtain the symbol name of the function that should be called to convert
 * an object instance pointer of this object type to a GValue.
 * I's mainly used fundamental types. The type signature for the symbol
 * is %GIObjectInfoGetValueFunction, to fetch the function pointer
 * see g_object_info_get_get_value_function().
 *
 * Returns: (nullable): the symbol or %NULL
 */
const char *
g_object_info_get_get_value_function (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->get_value_func)
    return g_typelib_get_string (rinfo->typelib, blob->get_value_func);

  return NULL;
}

/**
 * g_object_info_get_get_value_function_pointer: (skip)
 * @info: a #GIObjectInfo
 *
 * Obtain a pointer to a function which can be used to
 * extract an instance of this object type out of a GValue.
 * This takes derivation into account and will reversely traverse
 * the base classes of this type, starting at the top type.
 *
 * Returns: (nullable): the function pointer or %NULL
 */
GIObjectInfoGetValueFunction
g_object_info_get_get_value_function_pointer (GIObjectInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  return (GIObjectInfoGetValueFunction)_get_func(info, (SymbolGetter)g_object_info_get_get_value_function);
}
