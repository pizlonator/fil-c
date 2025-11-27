/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Interface implementation
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
 * SECTION:giinterfaceinfo
 * @title: GIInterfaceInfo
 * @short_description: Struct representing a GInterface
 *
 * GIInterfaceInfo represents a #GInterface type.
 *
 * A GInterface has methods, fields, properties, signals, interfaces, constants,
 * virtual functions and prerequisites.
 */

/**
 * g_interface_info_get_n_prerequisites:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of prerequisites for this interface type.
 * A prerequisites is another interface that needs to be implemented for
 * interface, similar to an base class for GObjects.
 *
 * Returns: number of prerequisites
 */
gint
g_interface_info_get_n_prerequisites (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_prerequisites;
}

/**
 * g_interface_info_get_prerequisite:
 * @info: a #GIInterfaceInfo
 * @n: index of prerequisites to get
 *
 * Obtain an interface type prerequisites index @n.
 *
 * Returns: (transfer full): the prerequisites as a #GIBaseInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIBaseInfo *
g_interface_info_get_prerequisite (GIInterfaceInfo *info,
				   gint            n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return _g_info_from_entry (rinfo->repository,
			     rinfo->typelib, blob->prerequisites[n]);
}


/**
 * g_interface_info_get_n_properties:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of properties that this interface type has.
 *
 * Returns: number of properties
 */
gint
g_interface_info_get_n_properties (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_properties;
}

/**
 * g_interface_info_get_property:
 * @info: a #GIInterfaceInfo
 * @n: index of property to get
 *
 * Obtain an interface type property at index @n.
 *
 * Returns: (transfer full): the #GIPropertyInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIPropertyInfo *
g_interface_info_get_property (GIInterfaceInfo *info,
			       gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_interface_info_get_n_methods:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of methods that this interface type has.
 *
 * Returns: number of methods
 */
gint
g_interface_info_get_n_methods (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

/**
 * g_interface_info_get_method:
 * @info: a #GIInterfaceInfo
 * @n: index of method to get
 *
 * Obtain an interface type method at index @n.
 *
 * Returns: (transfer full): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_interface_info_get_method (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_interface_info_find_method:
 * @info: a #GIInterfaceInfo
 * @name: name of method to obtain
 *
 * Obtain a method of the interface type given a @name. %NULL will be
 * returned if there's no method available with that name.
 *
 * Returns: (transfer full): the #GIFunctionInfo or %NULL if none found.
 * Free the struct by calling g_base_info_unref() when done.
 */
GIFunctionInfo *
g_interface_info_find_method (GIInterfaceInfo *info,
			      const gchar     *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size;

  return _g_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

/**
 * g_interface_info_get_n_signals:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of signals that this interface type has.
 *
 * Returns: number of signals
 */
gint
g_interface_info_get_n_signals (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

/**
 * g_interface_info_get_signal:
 * @info: a #GIInterfaceInfo
 * @n: index of signal to get
 *
 * Obtain an interface type signal at index @n.
 *
 * Returns: (transfer full): the #GISignalInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GISignalInfo *
g_interface_info_get_signal (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

/**
 * g_interface_info_find_signal:
 * @info: a #GIInterfaceInfo
 * @name: Name of signal
 *
 * TODO
 *
 * Returns: (transfer full): Info for the signal with name @name in @info, or
 * %NULL on failure.
 * Since: 1.34
 */
GISignalInfo *
g_interface_info_find_signal (GIInterfaceInfo *info,
                              const gchar  *name)
{
  gint n_signals;
  gint i;

  n_signals = g_interface_info_get_n_signals (info);
  for (i = 0; i < n_signals; i++)
    {
      GISignalInfo *siginfo = g_interface_info_get_signal (info, i);

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
 * g_interface_info_get_n_vfuncs:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of virtual functions that this interface type has.
 *
 * Returns: number of virtual functions
 */
gint
g_interface_info_get_n_vfuncs (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

/**
 * g_interface_info_get_vfunc:
 * @info: a #GIInterfaceInfo
 * @n: index of virtual function to get
 *
 * Obtain an interface type virtual function at index @n.
 *
 * Returns: (transfer full): the #GIVFuncInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_interface_info_get_vfunc (GIInterfaceInfo *info,
			    gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

/**
 * g_interface_info_find_vfunc:
 * @info: a #GIInterfaceInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. See the documentation
 * for g_object_info_find_vfunc() for more information on virtuals.
 *
 * Returns: (transfer full): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_interface_info_find_vfunc (GIInterfaceInfo *info,
                             const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + blob->n_prerequisites % 2) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return _g_base_info_find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

/**
 * g_interface_info_get_n_constants:
 * @info: a #GIInterfaceInfo
 *
 * Obtain the number of constants that this interface type has.
 *
 * Returns: number of constants
 */
gint
g_interface_info_get_n_constants (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), 0);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

/**
 * g_interface_info_get_constant:
 * @info: a #GIInterfaceInfo
 * @n: index of constant to get
 *
 * Obtain an interface type constant at index @n.
 *
 * Returns: (transfer full): the #GIConstantInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIConstantInfo *
g_interface_info_get_constant (GIInterfaceInfo *info,
			       gint             n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_interface_info_get_iface_struct:
 * @info: a #GIInterfaceInfo
 *
 * Returns the layout C structure associated with this #GInterface.
 *
 * Returns: (transfer full): the #GIStructInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_interface_info_get_iface_struct (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_INTERFACE_INFO (info), NULL);

  blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

