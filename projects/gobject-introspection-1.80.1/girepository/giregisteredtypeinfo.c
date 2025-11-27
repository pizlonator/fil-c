/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Registered Type implementation
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
 * SECTION:giregisteredtypeinfo
 * @title: GIRegisteredTypeInfo
 * @short_description: Struct representing a struct with a GType
 *
 * GIRegisteredTypeInfo represents an entity with a GType associated.
 *
 * Could be either a #GIEnumInfo, #GIInterfaceInfo, #GIObjectInfo,
 * #GIStructInfo or a #GIUnionInfo.
 *
 * A registered type info struct has a name and a type function.
 *
 * To get the name call g_registered_type_info_get_type_name().
 * Most users want to call g_registered_type_info_get_g_type() and don't worry
 * about the rest of the details.
 */

/**
 * g_registered_type_info_get_type_name:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the type name of the struct within the GObject type system.
 * This type can be passed to g_type_name() to get a #GType.
 *
 * Returns: the type name
 */
const gchar *
g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), NULL);

  blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_name)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_name);

  return NULL;
}

/**
 * g_registered_type_info_get_type_init:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the type init function for @info. The type init function is the
 * function which will register the GType within the GObject type system.
 * Usually this is not called by langauge bindings or applications, use
 * g_registered_type_info_get_g_type() directly instead.
 *
 * Returns: the symbol name of the type init function, suitable for
 * passing into g_module_symbol().
 */
const gchar *
g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), NULL);

  blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_init)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_init);

  return NULL;
}

/**
 * g_registered_type_info_get_g_type:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the #GType for this registered type or G_TYPE_NONE which a special meaning.
 * It means that either there is no type information associated with this @info or
 * that the shared library which provides the type_init function for this
 * @info cannot be called.
 *
 * Returns: the #GType.
 */
GType
g_registered_type_info_get_g_type (GIRegisteredTypeInfo *info)
{
  const char *type_init;
  GType (* get_type_func) (void);
  GIRealInfo *rinfo = (GIRealInfo*)info;

  g_return_val_if_fail (info != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), G_TYPE_INVALID);

  type_init = g_registered_type_info_get_type_init (info);

  if (type_init == NULL)
    return G_TYPE_NONE;
  else if (!strcmp (type_init, "intern"))
    /* The special string "intern" is used for some types exposed by libgobject
       (that therefore should be always available) */
    return g_type_from_name (g_registered_type_info_get_type_name (info));

  get_type_func = NULL;
  if (!g_typelib_symbol (rinfo->typelib,
                         type_init,
                         (void**) &get_type_func))
    return G_TYPE_NONE;

  return (* get_type_func) ();
}

