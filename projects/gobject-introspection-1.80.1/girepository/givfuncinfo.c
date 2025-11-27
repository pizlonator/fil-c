/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Virtual Function implementation
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

#include <string.h>

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/**
 * SECTION:givfuncinfo
 * @title: GIVFuncInfo
 * @short_description: Struct representing a virtual function
 *
 * GIVfuncInfo represents a virtual function.
 *
 * A virtual function is a callable object that belongs to either a
 * #GIObjectInfo or a #GIInterfaceInfo.
 */

GIVFuncInfo *
_g_base_info_find_vfunc (GIRealInfo   *rinfo,
			 guint32       offset,
			 gint          n_vfuncs,
			 const gchar  *name)
{
  /* FIXME hash */
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_vfuncs; i++)
    {
      VFuncBlob *fblob = (VFuncBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*) rinfo,
                                           rinfo->typelib, offset);

      offset += header->vfunc_blob_size;
    }

  return NULL;
}

/**
 * g_vfunc_info_get_flags:
 * @info: a #GIVFuncInfo
 *
 * Obtain the flags for this virtual function info. See #GIVFuncInfoFlags for
 * more information about possible flag values.
 *
 * Returns: the flags
 */
GIVFuncInfoFlags
g_vfunc_info_get_flags (GIVFuncInfo *info)
{
  GIVFuncInfoFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_VFUNC_INFO (info), 0);

  blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->must_chain_up)
    flags = flags | GI_VFUNC_MUST_CHAIN_UP;

  if (blob->must_be_implemented)
    flags = flags | GI_VFUNC_MUST_OVERRIDE;

  if (blob->must_not_be_implemented)
    flags = flags | GI_VFUNC_MUST_NOT_OVERRIDE;

  if (blob->throws)
    flags = flags | GI_VFUNC_THROWS;

  return flags;
}

/**
 * g_vfunc_info_get_offset:
 * @info: a #GIVFuncInfo
 *
 * Obtain the offset of the function pointer in the class struct. The value
 * 0xFFFF indicates that the struct offset is unknown.
 *
 * Returns: the struct offset or 0xFFFF if it's unknown
 */
gint
g_vfunc_info_get_offset (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_VFUNC_INFO (info), 0);

  blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

/**
 * g_vfunc_info_get_signal:
 * @info: a #GIVFuncInfo
 *
 * Obtain the signal for the virtual function if one is set.
 * The signal comes from the object or interface to which
 * this virtual function belongs.
 *
 * Returns: (transfer full): the signal or %NULL if none set
 */
GISignalInfo *
g_vfunc_info_get_signal (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_VFUNC_INFO (info), 0);

  blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->class_closure)
    return g_interface_info_get_signal ((GIInterfaceInfo *)rinfo->container, blob->signal);

  return NULL;
}

/**
 * g_vfunc_info_get_invoker:
 * @info: a #GIVFuncInfo
 *
 * If this virtual function has an associated invoker method, this
 * method will return it.  An invoker method is a C entry point.
 *
 * Not all virtuals will have invokers.
 *
 * Returns: (transfer full): the #GIVFuncInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_vfunc_info_get_invoker (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob;
  GIBaseInfo *container;
  GIInfoType parent_type;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_VFUNC_INFO (info), 0);

  blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  /* 1023 = 0x3ff is the maximum of the 10 bits for invoker index */
  if (blob->invoker == 1023)
    return NULL;

  container = rinfo->container;
  parent_type = g_base_info_get_type (container);
  if (parent_type == GI_INFO_TYPE_OBJECT)
    return g_object_info_get_method ((GIObjectInfo*)container, blob->invoker);
  else if (parent_type == GI_INFO_TYPE_INTERFACE)
    return g_interface_info_get_method ((GIInterfaceInfo*)container, blob->invoker);
  else
    g_assert_not_reached ();
}

/**
 * g_vfunc_info_get_address:
 * @info: a #GIVFuncInfo
 * @implementor_gtype: #GType implementing this virtual function
 * @error: return location for a #GError
 *
 * This method will look up where inside the type struct of @implementor_gtype
 * is the implementation for @info.
 *
 * Returns: address to a function or %NULL if an error happened
 */
gpointer
g_vfunc_info_get_address (GIVFuncInfo      *vfunc_info,
                          GType             implementor_gtype,
                          GError          **error)
{
  GIBaseInfo *container_info;
  GIInterfaceInfo *interface_info;
  GIObjectInfo *object_info;
  GIStructInfo *struct_info;
  GIFieldInfo *field_info = NULL;
  int length, i, offset;
  gpointer implementor_class, implementor_vtable;
  gpointer func = NULL;

  container_info = g_base_info_get_container (vfunc_info);
  if (g_base_info_get_type (container_info) == GI_INFO_TYPE_OBJECT)
    {
      object_info = (GIObjectInfo*) container_info;
      interface_info = NULL;
      struct_info = g_object_info_get_class_struct (object_info);
    }
  else
    {
      interface_info = (GIInterfaceInfo*) container_info;
      object_info = NULL;
      struct_info = g_interface_info_get_iface_struct (interface_info);
    }

  length = g_struct_info_get_n_fields (struct_info);
  for (i = 0; i < length; i++)
    {
      field_info = g_struct_info_get_field (struct_info, i);

      if (strcmp (g_base_info_get_name ( (GIBaseInfo*) field_info),
                  g_base_info_get_name ( (GIBaseInfo*) vfunc_info)) != 0) {
          g_base_info_unref (field_info);
          field_info = NULL;
          continue;
      }

      break;
    }

  if (field_info == NULL)
    {
      g_set_error (error,
                   G_INVOKE_ERROR,
                   G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
                   "Couldn't find struct field for this vfunc");
      goto out;
    }

  implementor_class = g_type_class_ref (implementor_gtype);

  if (object_info)
    {
      implementor_vtable = implementor_class;
    }
  else
    {
      GType interface_type;

      interface_type = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo*) interface_info);
      implementor_vtable = g_type_interface_peek (implementor_class, interface_type);
    }

  offset = g_field_info_get_offset (field_info);
  func = *(gpointer*) G_STRUCT_MEMBER_P (implementor_vtable, offset);
  g_type_class_unref (implementor_class);
  g_base_info_unref (field_info);

  if (func == NULL)
    {
      g_set_error (error,
                   G_INVOKE_ERROR,
                   G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
                   "Class %s doesn't implement %s",
                   g_type_name (implementor_gtype),
                   g_base_info_get_name ( (GIBaseInfo*) vfunc_info));
      goto out;
    }

 out:
  g_base_info_unref ((GIBaseInfo*) struct_info);

  return func;
}

/**
 * g_vfunc_info_invoke: (skip)
 * @info: a #GIVFuncInfo describing the virtual function to invoke
 * @implementor: #GType of the type that implements this virtual function
 * @in_args: (array length=n_in_args): an array of #GIArgument<!-- -->s, one for each in
 *    parameter of @info. If there are no in parameter, @in_args
 *    can be %NULL
 * @n_in_args: the length of the @in_args array
 * @out_args: (array length=n_out_args): an array of #GIArgument<!-- -->s, one for each out
 *    parameter of @info. If there are no out parameters, @out_args
 *    may be %NULL
 * @n_out_args: the length of the @out_args array
 * @return_value: return location for the return value of the
 *    function. If the function returns void, @return_value may be
 *    %NULL
 * @error: return location for detailed error information, or %NULL
 *
 * Invokes the function described in @info with the given
 * arguments. Note that inout parameters must appear in both
 * argument lists.
 *
 * Returns: %TRUE if the function has been invoked, %FALSE if an
 *   error occurred.
 */
gboolean
g_vfunc_info_invoke (GIVFuncInfo      *info,
                     GType             implementor,
                     const GIArgument *in_args,
                     int               n_in_args,
                     const GIArgument *out_args,
                     int               n_out_args,
                     GIArgument       *return_value,
                     GError          **error)
{
  gpointer func;

  func = g_vfunc_info_get_address (info, implementor, error);
  if (*error != NULL)
    return FALSE;

  return g_callable_info_invoke ((GICallableInfo*) info,
                                 func,
                                 in_args,
                                 n_in_args,
                                 out_args,
                                 n_out_args,
                                 return_value,
                                 TRUE,
                                 FALSE,
                                 error);
}
