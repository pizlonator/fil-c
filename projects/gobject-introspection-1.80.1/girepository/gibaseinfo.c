/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Base struct implementation
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include "gitypelib-internal.h"
#include "girepository-private.h"

#define INVALID_REFCOUNT 0x7FFFFFFF

/* GBoxed registration of BaseInfo. */
GType
g_base_info_gtype_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type =
        g_boxed_type_register_static ("GIBaseInfo",
                                      (GBoxedCopyFunc) g_base_info_ref,
                                      (GBoxedFreeFunc) g_base_info_unref);

  return our_type;
}

/* info creation */
GIBaseInfo *
_g_info_new_full (GIInfoType     type,
                  GIRepository  *repository,
                  GIBaseInfo    *container,
                  GITypelib      *typelib,
                  guint32        offset)
{
  GIRealInfo *info;

  g_return_val_if_fail (container != NULL || repository != NULL, NULL);

  info = g_slice_new (GIRealInfo);

  _g_info_init (info, type, repository, container, typelib, offset);
  info->ref_count = 1;

  if (container && ((GIRealInfo *) container)->ref_count != INVALID_REFCOUNT)
    g_base_info_ref (info->container);

  g_object_ref (info->repository);

  return (GIBaseInfo*)info;
}

/**
 * g_info_new:
 * @type: TODO
 * @container: TODO
 * @typelib: TODO
 * @offset: TODO
 *
 * TODO
 *
 * Returns: TODO
 */
GIBaseInfo *
g_info_new (GIInfoType     type,
            GIBaseInfo    *container,
            GITypelib      *typelib,
            guint32        offset)
{
  return _g_info_new_full (type, ((GIRealInfo*)container)->repository, container, typelib, offset);
}

void
_g_info_init (GIRealInfo     *info,
              GIInfoType      type,
              GIRepository   *repository,
              GIBaseInfo     *container,
              GITypelib       *typelib,
              guint32         offset)
{
  memset (info, 0, sizeof (GIRealInfo));

  /* Invalid refcount used to flag stack-allocated infos */
  info->ref_count = INVALID_REFCOUNT;
  info->type = type;

  info->typelib = typelib;
  info->offset = offset;

  if (container)
    info->container = container;

  g_assert (G_IS_IREPOSITORY (repository));
  info->repository = repository;
}

GIBaseInfo *
_g_info_from_entry (GIRepository *repository,
                    GITypelib     *typelib,
                    guint16       index)
{
  GIBaseInfo *result;
  DirEntry *entry = g_typelib_get_dir_entry (typelib, index);

  if (entry->local)
    result = _g_info_new_full (entry->blob_type, repository, NULL, typelib, entry->offset);
  else
    {
      const gchar *namespace = g_typelib_get_string (typelib, entry->offset);
      const gchar *name = g_typelib_get_string (typelib, entry->name);

      result = g_irepository_find_by_name (repository, namespace, name);
      if (result == NULL)
        {
          GIUnresolvedInfo *unresolved;

          unresolved = g_slice_new0 (GIUnresolvedInfo);

          unresolved->type = GI_INFO_TYPE_UNRESOLVED;
          unresolved->ref_count = 1;
          unresolved->repository = g_object_ref (repository);
          unresolved->container = NULL;
          unresolved->name = name;
          unresolved->namespace = namespace;

          return (GIBaseInfo *)unresolved;
	}
      return (GIBaseInfo *)result;
    }

  return (GIBaseInfo *)result;
}

GITypeInfo *
_g_type_info_new (GIBaseInfo    *container,
                 GITypelib      *typelib,
		 guint32        offset)
{
  SimpleTypeBlob *type = (SimpleTypeBlob *)&typelib->data[offset];

  return (GITypeInfo *) g_info_new (GI_INFO_TYPE_TYPE, container, typelib,
                                    (type->flags.reserved == 0 && type->flags.reserved2 == 0) ? offset : type->offset);
}

void
_g_type_info_init (GIBaseInfo *info,
                   GIBaseInfo *container,
                   GITypelib   *typelib,
                   guint32     offset)
{
  GIRealInfo *rinfo = (GIRealInfo*)container;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&typelib->data[offset];

  _g_info_init ((GIRealInfo*)info, GI_INFO_TYPE_TYPE, rinfo->repository, container, typelib,
                (type->flags.reserved == 0 && type->flags.reserved2 == 0) ? offset : type->offset);
}

/* GIBaseInfo functions */

/**
 * SECTION:gibaseinfo
 * @title: GIBaseInfo
 * @short_description: Base struct for all GITypelib structs
 *
 * GIBaseInfo is the common base struct of all other Info structs
 * accessible through the #GIRepository API.
 *
 * All info structures can be cast to a #GIBaseInfo, for instance:
 *
 * |[<!-- language="C" -->
 *    GIFunctionInfo *function_info = ...;
 *    GIBaseInfo *info = (GIBaseInfo *) function_info;
 * ]|
 *
 * Most #GIRepository APIs returning a #GIBaseInfo is actually
 * creating a new struct; in other words, g_base_info_unref() has to
 * be called when done accessing the data.
 *
 * #GIBaseInfo structuress are normally accessed by calling either
 * g_irepository_find_by_name(), g_irepository_find_by_gtype() or
 * g_irepository_get_info().
 *
 * |[<!-- language="C" -->
 * GIBaseInfo *button_info =
 *   g_irepository_find_by_name (NULL, "Gtk", "Button");
 *
 * // ... use button_info ...
 *
 * g_base_info_unref (button_info);
 * ]|
 *
 * ## Hierarchy
 *
 * |[<!-- language="plain" -->
 *   GIBaseInfo
 *    +---- GIArgInfo
 *    +---- GICallableInfo
 *    +---- GIConstantInfo
 *    +---- GIFieldInfo
 *    +---- GIPropertyInfo
 *    +---- GIRegisteredTypeInfo
 *    +---- GITypeInfo
 * ]|
 */

/**
 * g_base_info_ref: (skip)
 * @info: a #GIBaseInfo
 *
 * Increases the reference count of @info.
 *
 * Returns: the same @info.
 */
GIBaseInfo *
g_base_info_ref (GIBaseInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;

  g_assert (rinfo->ref_count != INVALID_REFCOUNT);
  g_atomic_int_inc (&rinfo->ref_count);

  return info;
}

/**
 * g_base_info_unref: (skip)
 * @info: a #GIBaseInfo
 *
 * Decreases the reference count of @info. When its reference count
 * drops to 0, the info is freed.
 */
void
g_base_info_unref (GIBaseInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;

  g_assert (rinfo->ref_count > 0 && rinfo->ref_count != INVALID_REFCOUNT);

  if (!g_atomic_int_dec_and_test (&rinfo->ref_count))
    return;

  if (rinfo->container && ((GIRealInfo *) rinfo->container)->ref_count != INVALID_REFCOUNT)
    g_base_info_unref (rinfo->container);

  if (rinfo->repository)
    g_object_unref (rinfo->repository);

  if (rinfo->type == GI_INFO_TYPE_UNRESOLVED)
    g_slice_free (GIUnresolvedInfo, (GIUnresolvedInfo *) rinfo);
  else
    g_slice_free (GIRealInfo, rinfo);
}

/**
 * g_base_info_get_type:
 * @info: a #GIBaseInfo
 *
 * Obtain the info type of the GIBaseInfo.
 *
 * Returns: the info type of @info
 */
GIInfoType
g_base_info_get_type (GIBaseInfo *info)
{

  return ((GIRealInfo*)info)->type;
}

/**
 * g_base_info_get_name:
 * @info: a #GIBaseInfo
 *
 * Obtain the name of the @info. What the name represents depends on
 * the #GIInfoType of the @info. For instance for #GIFunctionInfo it is
 * the name of the function.
 *
 * Returns: the name of @info or %NULL if it lacks a name.
 */
const gchar *
g_base_info_get_name (GIBaseInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;
  g_assert (rinfo->ref_count > 0);
  switch (rinfo->type)
    {
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_INVALID_0:
    case GI_INFO_TYPE_UNION:
      {
        CommonBlob *blob = (CommonBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_VALUE:
      {
        ValueBlob *blob = (ValueBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_SIGNAL:
      {
        SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_PROPERTY:
      {
        PropertyBlob *blob = (PropertyBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_VFUNC:
      {
        VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_FIELD:
      {
        FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_ARG:
      {
        ArgBlob *blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

        return g_typelib_get_string (rinfo->typelib, blob->name);
      }
      break;
    case GI_INFO_TYPE_UNRESOLVED:
      {
        GIUnresolvedInfo *unresolved = (GIUnresolvedInfo *)info;

        return unresolved->name;
      }
      break;
    case GI_INFO_TYPE_TYPE:
      return NULL;
    default: ;
      g_assert_not_reached ();
      /* unnamed */
    }

  return NULL;
}

/**
 * g_base_info_get_namespace:
 * @info: a #GIBaseInfo
 *
 * Obtain the namespace of @info.
 *
 * Returns: the namespace
 */
const gchar *
g_base_info_get_namespace (GIBaseInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;
  Header *header = (Header *)rinfo->typelib->data;

  g_assert (rinfo->ref_count > 0);

  if (rinfo->type == GI_INFO_TYPE_UNRESOLVED)
    {
      GIUnresolvedInfo *unresolved = (GIUnresolvedInfo *)info;

      return unresolved->namespace;
    }

  return g_typelib_get_string (rinfo->typelib, header->namespace);
}

/**
 * g_base_info_is_deprecated:
 * @info: a #GIBaseInfo
 *
 * Obtain whether the @info is represents a metadata which is
 * deprecated or not.
 *
 * Returns: %TRUE if deprecated
 */
gboolean
g_base_info_is_deprecated (GIBaseInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;
  switch (rinfo->type)
    {
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_INVALID_0:
      {
        CommonBlob *blob = (CommonBlob *)&rinfo->typelib->data[rinfo->offset];

        return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_VALUE:
      {
        ValueBlob *blob = (ValueBlob *)&rinfo->typelib->data[rinfo->offset];

        return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_SIGNAL:
      {
        SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

        return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_PROPERTY:
      {
        PropertyBlob *blob = (PropertyBlob *)&rinfo->typelib->data[rinfo->offset];

        return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_VFUNC:
    case GI_INFO_TYPE_FIELD:
    case GI_INFO_TYPE_ARG:
    case GI_INFO_TYPE_TYPE:
    default: ;
      /* no deprecation flag for these */
    }

  return FALSE;
}

/**
 * g_base_info_get_attribute:
 * @info: a #GIBaseInfo
 * @name: a freeform string naming an attribute
 *
 * Retrieve an arbitrary attribute associated with this node.
 *
 * Returns: The value of the attribute, or %NULL if no such attribute exists
 */
const gchar *
g_base_info_get_attribute (GIBaseInfo   *info,
                           const gchar  *name)
{
  GIAttributeIter iter = { 0, };
  gchar *curname, *curvalue;
  while (g_base_info_iterate_attributes (info, &iter, &curname, &curvalue))
    {
      if (strcmp (name, curname) == 0)
        return (const gchar*) curvalue;
    }

  return NULL;
}

static int
cmp_attribute (const void *av,
               const void *bv)
{
  const AttributeBlob *a = av;
  const AttributeBlob *b = bv;

  if (a->offset < b->offset)
    return -1;
  else if (a->offset == b->offset)
    return 0;
  else
    return 1;
}

/*
 * _attribute_blob_find_first:
 * @GIBaseInfo: A #GIBaseInfo.
 * @blob_offset: The offset for the blob to find the first attribute for.
 *
 * Searches for the first #AttributeBlob for @blob_offset and returns
 * it if found.
 *
 * Returns: A pointer to #AttributeBlob or %NULL if not found.
 */
AttributeBlob *
_attribute_blob_find_first (GIBaseInfo *info,
                            guint32     blob_offset)
{
  GIRealInfo *rinfo = (GIRealInfo *) info;
  Header *header = (Header *)rinfo->typelib->data;
  AttributeBlob blob, *first, *res, *previous;

  blob.offset = blob_offset;

  first = (AttributeBlob *) &rinfo->typelib->data[header->attributes];

  res = bsearch (&blob, first, header->n_attributes,
                 header->attribute_blob_size, cmp_attribute);

  if (res == NULL)
    return NULL;

  previous = res - 1;
  while (previous >= first && previous->offset == blob_offset)
    {
      res = previous;
      previous = res - 1;
    }

  return res;
}

/**
 * g_base_info_iterate_attributes:
 * @info: a #GIBaseInfo
 * @iterator: (inout): a #GIAttributeIter structure, must be initialized; see below
 * @name: (out) (transfer none): Returned name, must not be freed
 * @value: (out) (transfer none): Returned name, must not be freed
 *
 * Iterate over all attributes associated with this node.  The iterator
 * structure is typically stack allocated, and must have its first
 * member initialized to %NULL.  Attributes are arbitrary namespaced key–value
 * pairs which can be attached to almost any item.  They are intended for use
 * by software higher in the toolchain than bindings, and are distinct from
 * normal GIR annotations.
 *
 * Both the @name and @value should be treated as constants
 * and must not be freed.
 *
 * |[<!-- language="C" -->
 * void
 * print_attributes (GIBaseInfo *info)
 * {
 *   GIAttributeIter iter = { 0, };
 *   char *name;
 *   char *value;
 *   while (g_base_info_iterate_attributes (info, &iter, &name, &value))
 *     {
 *       g_print ("attribute name: %s value: %s", name, value);
 *     }
 * }
 * ]|
 *
 * Returns: %TRUE if there are more attributes
 */
gboolean
g_base_info_iterate_attributes (GIBaseInfo      *info,
                                GIAttributeIter *iterator,
                                gchar           **name,
                                gchar           **value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  AttributeBlob *next, *after;

  after = (AttributeBlob *) &rinfo->typelib->data[header->attributes +
                                                  header->n_attributes * header->attribute_blob_size];

  if (iterator->data != NULL)
    next = (AttributeBlob *) iterator->data;
  else
    next = _attribute_blob_find_first (info, rinfo->offset);

  if (next == NULL || next->offset != rinfo->offset || next >= after)
    return FALSE;

  *name = (gchar*) g_typelib_get_string (rinfo->typelib, next->name);
  *value = (gchar*) g_typelib_get_string (rinfo->typelib, next->value);
  iterator->data = next + 1;

  return TRUE;
}

/**
 * g_base_info_get_container:
 * @info: a #GIBaseInfo
 *
 * Obtain the container of the @info. The container is the parent
 * GIBaseInfo. For instance, the parent of a #GIFunctionInfo is an
 * #GIObjectInfo or #GIInterfaceInfo.
 *
 * Returns: (transfer none): the container
 */
GIBaseInfo *
g_base_info_get_container (GIBaseInfo *info)
{
  return ((GIRealInfo*)info)->container;
}

/**
 * g_base_info_get_typelib:
 * @info: a #GIBaseInfo
 *
 * Obtain the typelib this @info belongs to
 *
 * Returns: (transfer none): the typelib.
 */
GITypelib *
g_base_info_get_typelib (GIBaseInfo *info)
{
  return ((GIRealInfo*)info)->typelib;
}

/**
 * g_base_info_equal:
 * @info1: a #GIBaseInfo
 * @info2: a #GIBaseInfo
 *
 * Compare two #GIBaseInfo.
 *
 * Using pointer comparison is not practical since many functions return
 * different instances of #GIBaseInfo that refers to the same part of the
 * TypeLib; use this function instead to do #GIBaseInfo comparisons.
 *
 * Returns: %TRUE if and only if @info1 equals @info2.
 */
gboolean
g_base_info_equal (GIBaseInfo *info1, GIBaseInfo *info2)
{
  /* Compare the TypeLib pointers, which are mmapped. */
  GIRealInfo *rinfo1 = (GIRealInfo*)info1;
  GIRealInfo *rinfo2 = (GIRealInfo*)info2;
  return rinfo1->typelib->data + rinfo1->offset == rinfo2->typelib->data + rinfo2->offset;
}


