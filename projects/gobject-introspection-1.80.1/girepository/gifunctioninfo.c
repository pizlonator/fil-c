/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Function implementation
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
 * SECTION:gifunctioninfo
 * @title: GIFunctionInfo
 * @short_description: Struct representing a function
 *
 * GIFunctionInfo represents a function, method or constructor.
 *
 * To find out what kind of entity a #GIFunctionInfo represents, call
 * g_function_info_get_flags().
 *
 * See also #GICallableInfo for information on how to retreive arguments and
 * other metadata.
 */

GIFunctionInfo *
_g_base_info_find_method (GIBaseInfo   *base,
			  guint32       offset,
			  gint          n_methods,
			  const gchar  *name)
{
  /* FIXME hash */
  GIRealInfo *rinfo = (GIRealInfo*)base;
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_methods; i++)
    {
      FunctionBlob *fblob = (FunctionBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base,
			                      rinfo->typelib, offset);

      offset += header->function_blob_size;
    }

  return NULL;
}

/**
 * g_function_info_get_symbol:
 * @info: a #GIFunctionInfo
 *
 * Obtain the symbol of the function. The symbol is the name of the
 * exported function, suitable to be used as an argument to
 * g_module_symbol().
 *
 * Returns: the symbol
 */
const gchar *
g_function_info_get_symbol (GIFunctionInfo *info)
{
  GIRealInfo *rinfo;
  FunctionBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->symbol);
}

/**
 * g_function_info_get_flags:
 * @info: a #GIFunctionInfo
 *
 * Obtain the #GIFunctionInfoFlags for the @info.
 *
 * Returns: the flags
 */
GIFunctionInfoFlags
g_function_info_get_flags (GIFunctionInfo *info)
{
  GIFunctionInfoFlags flags;
  GIRealInfo *rinfo;
  FunctionBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), -1);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  /* Make sure we don't flag Constructors as methods */
  if (!blob->constructor && !blob->is_static)
    flags = flags | GI_FUNCTION_IS_METHOD;

  if (blob->constructor)
    flags = flags | GI_FUNCTION_IS_CONSTRUCTOR;

  if (blob->getter)
    flags = flags | GI_FUNCTION_IS_GETTER;

  if (blob->setter)
    flags = flags | GI_FUNCTION_IS_SETTER;

  if (blob->wraps_vfunc)
    flags = flags | GI_FUNCTION_WRAPS_VFUNC;

  if (blob->throws)
    flags = flags | GI_FUNCTION_THROWS;

  return flags;
}

/**
 * g_function_info_get_property:
 * @info: a #GIFunctionInfo
 *
 * Obtain the property associated with this #GIFunctionInfo.
 * Only #GIFunctionInfo with the flag %GI_FUNCTION_IS_GETTER or
 * %GI_FUNCTION_IS_SETTER have a property set. For other cases,
 * %NULL will be returned.
 *
 * Returns: (transfer full): the property or %NULL if not set. Free it with
 * g_base_info_unref() when done.
 */
GIPropertyInfo *
g_function_info_get_property (GIFunctionInfo *info)
{
  GIRealInfo *rinfo, *container_rinfo;
  FunctionBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
  container_rinfo = (GIRealInfo *)rinfo->container;

  if (container_rinfo->type == GI_INFO_TYPE_INTERFACE)
    {
      GIInterfaceInfo *container = (GIInterfaceInfo *)rinfo->container;

      return g_interface_info_get_property (container, blob->index);
    }
  else if (container_rinfo->type == GI_INFO_TYPE_OBJECT)
    {
      GIObjectInfo *container = (GIObjectInfo *)rinfo->container;

      return g_object_info_get_property (container, blob->index);
    }
  else
    return NULL;
}

/**
 * g_function_info_get_vfunc:
 * @info: a #GIFunctionInfo
 *
 * Obtain the virtual function associated with this #GIFunctionInfo.
 * Only #GIFunctionInfo with the flag %GI_FUNCTION_WRAPS_VFUNC has
 * a virtual function set. For other cases, %NULL will be returned.
 *
 * Returns: (transfer full): the virtual function or %NULL if not set.
 * Free it by calling g_base_info_unref() when done.
 */
GIVFuncInfo *
g_function_info_get_vfunc (GIFunctionInfo *info)
{
  GIRealInfo *rinfo;
  FunctionBlob *blob;
  GIInterfaceInfo *container;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
  container = (GIInterfaceInfo *)rinfo->container;

  return g_interface_info_get_vfunc (container, blob->index);
}

/**
 * g_invoke_error_quark:
 *
 * TODO
 *
 * Returns: TODO
 */
GQuark
g_invoke_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-invoke-error-quark");
  return quark;
}

/**
 * g_function_info_invoke: (skip)
 * @info: a #GIFunctionInfo describing the function to invoke
 * @in_args: (array length=n_in_args): an array of #GIArgument<!-- -->s, one for each in
 *    parameter of @info. If there are no in parameter, @in_args
 *    can be %NULL
 * @n_in_args: the length of the @in_args array
 * @out_args: (array length=n_out_args): an array of #GIArgument<!-- -->s, one for each out
 *    parameter of @info. If there are no out parameters, @out_args
 *    may be %NULL
 * @n_out_args: the length of the @out_args array
 * @return_value: return location for the return value of the
 *    function.
 * @error: return location for detailed error information, or %NULL
 *
 * Invokes the function described in @info with the given
 * arguments. Note that inout parameters must appear in both
 * argument lists. This function uses dlsym() to obtain a pointer
 * to the function, so the library or shared object containing the
 * described function must either be linked to the caller, or must
 * have been g_module_symbol()<!-- -->ed before calling this function.
 *
 * Returns: %TRUE if the function has been invoked, %FALSE if an
 *   error occurred.
 */
gboolean
g_function_info_invoke (GIFunctionInfo *info,
			const GIArgument  *in_args,
			int               n_in_args,
			const GIArgument  *out_args,
			int               n_out_args,
			GIArgument        *return_value,
			GError          **error)
{
  const gchar *symbol;
  gpointer func;
  gboolean is_method;
  gboolean throws;

  symbol = g_function_info_get_symbol (info);

  if (!g_typelib_symbol (g_base_info_get_typelib((GIBaseInfo *) info),
                         symbol, &func))
    {
      g_set_error (error,
                   G_INVOKE_ERROR,
                   G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
                   "Could not locate %s: %s", symbol, g_module_error ());

      return FALSE;
    }

  is_method = (g_function_info_get_flags (info) & GI_FUNCTION_IS_METHOD) != 0
    && (g_function_info_get_flags (info) & GI_FUNCTION_IS_CONSTRUCTOR) == 0;
  throws = g_function_info_get_flags (info) & GI_FUNCTION_THROWS;

  return g_callable_info_invoke ((GICallableInfo*) info,
                                 func,
                                 in_args,
                                 n_in_args,
                                 out_args,
                                 n_out_args,
                                 return_value,
                                 is_method,
                                 throws,
                                 error);
}
