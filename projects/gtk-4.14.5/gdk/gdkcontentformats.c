/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GdkContentFormats:
 *
 * The `GdkContentFormats` structure is used to advertise and negotiate the
 * format of content.
 *
 * You will encounter `GdkContentFormats` when interacting with objects
 * controlling operations that pass data between different widgets, window
 * or application, like [class@Gdk.Drag], [class@Gdk.Drop],
 * [class@Gdk.Clipboard] or [class@Gdk.ContentProvider].
 *
 * GDK supports content in 2 forms: `GType` and mime type.
 * Using `GTypes` is meant only for in-process content transfers. Mime types
 * are meant to be used for data passing both in-process and out-of-process.
 * The details of how data is passed is described in the documentation of
 * the actual implementations. To transform between the two forms,
 * [class@Gdk.ContentSerializer] and [class@Gdk.ContentDeserializer] are used.
 *
 * A `GdkContentFormats` describes a set of possible formats content can be
 * exchanged in. It is assumed that this set is ordered. `GTypes` are more
 * important than mime types. Order between different `GTypes` or mime types
 * is the order they were added in, most important first. Functions that
 * care about order, such as [method@Gdk.ContentFormats.union], will describe
 * in their documentation how they interpret that order, though in general the
 * order of the first argument is considered the primary order of the result,
 * followed by the order of further arguments.
 *
 * For debugging purposes, the function [method@Gdk.ContentFormats.to_string]
 * exists. It will print a comma-separated list of formats from most important
 * to least important.
 *
 * `GdkContentFormats` is an immutable struct. After creation, you cannot change
 * the types it represents. Instead, new `GdkContentFormats` have to be created.
 * The [struct@Gdk.ContentFormatsBuilder] structure is meant to help in this
 * endeavor.
 */

#include "config.h"

#include "gdkcontentformats.h"
#include "gdkcontentformatsprivate.h"

#include <string.h>

struct _GdkContentFormats
{
  /*< private >*/
  guint ref_count;

  const char **mime_types; /* interned */
  gsize n_mime_types;
  GType *gtypes;
  gsize n_gtypes;
};

G_DEFINE_BOXED_TYPE (GdkContentFormats, gdk_content_formats,
                     gdk_content_formats_ref,
                     gdk_content_formats_unref)


/**
 * gdk_intern_mime_type:
 * @string: (transfer none): string of a potential mime type
 *
 * Canonicalizes the given mime type and interns the result.
 *
 * If @string is not a valid mime type, %NULL is returned instead.
 * See RFC 2048 for the syntax if mime types.
 *
 * Returns: (nullable): An interned string for the canonicalized
 *   mime type or %NULL if the string wasn't a valid mime type
 */
const char *
gdk_intern_mime_type (const char *string)
{
  char *tmp;

  g_return_val_if_fail (string != NULL, NULL);

  if (!strchr (string, '/'))
    return NULL;

  tmp = g_ascii_strdown (string, -1);

  string = g_intern_string (tmp);

  g_free (tmp);

  return string;
}

static GdkContentFormats *
gdk_content_formats_new_take (GType *      gtypes,
                              gsize        n_gtypes,
                              const char **mime_types,
                              gsize        n_mime_types)
{
  GdkContentFormats *result = g_new0 (GdkContentFormats, 1);
  result->ref_count = 1;

  result->gtypes = gtypes;
  result->n_gtypes = n_gtypes;
  result->mime_types = mime_types;
  result->n_mime_types = n_mime_types;

  return result;
}

/**
 * gdk_content_formats_new:
 * @mime_types: (array length=n_mime_types) (nullable): Pointer to an
 *   array of mime types
 * @n_mime_types: number of entries in @mime_types.
 *
 * Creates a new `GdkContentFormats` from an array of mime types.
 *
 * The mime types must be valid and different from each other or the
 * behavior of the return value is undefined. If you cannot guarantee
 * this, use [struct@Gdk.ContentFormatsBuilder] instead.
 *
 * Returns: (transfer full): the new `GdkContentFormats`.
 */
GdkContentFormats *
gdk_content_formats_new (const char **mime_types,
                         guint        n_mime_types)
{
  guint i;
  const char **mime_types_copy;

  if (n_mime_types == 0)
      return gdk_content_formats_new_take (NULL, 0, NULL, 0);

  mime_types_copy = g_new (const char *, n_mime_types + 1);

  for (i = 0; i < n_mime_types; i++)
    mime_types_copy[i] = g_intern_string (mime_types[i]);

  mime_types_copy[n_mime_types] = NULL;

  return gdk_content_formats_new_take (NULL, 0, mime_types_copy, n_mime_types);
}

/**
 * gdk_content_formats_new_for_gtype:
 * @type: a `GType`
 *
 * Creates a new `GdkContentFormats` for a given `GType`.
 *
 * Returns: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_new_for_gtype (GType type)
{
  GType *data;

  g_return_val_if_fail (type != G_TYPE_INVALID, NULL);

  data = g_new (GType, 2);
  data[0] = type;
  data[1] = G_TYPE_INVALID;

  return gdk_content_formats_new_take (data, 1, NULL, 0);
}

/**
 * gdk_content_formats_parse:
 * @string: the string to parse
 *
 * Parses the given @string into `GdkContentFormats` and
 * returns the formats.
 *
 * Strings printed via [method@Gdk.ContentFormats.to_string]
 * can be read in again successfully using this function.
 *
 * If @string does not describe valid content formats, %NULL
 * is returned.
 *
 * Returns: (nullable): the content formats if @string is valid
 *
 * Since: 4.4
 */
GdkContentFormats *
gdk_content_formats_parse (const char *string)
{
  GdkContentFormatsBuilder *builder;
  char **split;
  gsize i;

  g_return_val_if_fail (string != NULL, NULL);

  split = g_strsplit_set (string, "\t\n\f\r ", -1); /* same as g_ascii_isspace() */
  builder = gdk_content_formats_builder_new ();

  /* first the GTypes */
  for (i = 0; split[i] != NULL; i++)
    {
      GType type;

      if (split[i][0] == 0)
        continue;

      type = g_type_from_name (split[i]);
      if (type != 0)
        gdk_content_formats_builder_add_gtype (builder, type);
      else
        break;
    }

  /* then the mime types */
  for (; split[i] != NULL; i++)
    {
      const char *mime_type;

      if (split[i][0] == 0)
        continue;

      mime_type = gdk_intern_mime_type (split[i]);
      if (mime_type)
        gdk_content_formats_builder_add_mime_type (builder, mime_type);
      else
        break;
    }

  if (split[i] != NULL)
    {
      g_strfreev (split);
      gdk_content_formats_builder_unref (builder);
      return NULL;
    }

  g_strfreev (split);
  return gdk_content_formats_builder_free_to_formats (builder);
}

/**
 * gdk_content_formats_ref:
 * @formats:  a `GdkContentFormats`
 *
 * Increases the reference count of a `GdkContentFormats` by one.
 *
 * Returns: the passed in `GdkContentFormats`.
 */
GdkContentFormats *
gdk_content_formats_ref (GdkContentFormats *formats)
{
  g_return_val_if_fail (formats != NULL, NULL);

  formats->ref_count++;

  return formats;
}

/**
 * gdk_content_formats_unref:
 * @formats: a `GdkContentFormats`
 *
 * Decreases the reference count of a `GdkContentFormats` by one.
 *
 * If the resulting reference count is zero, frees the formats.
 */
void
gdk_content_formats_unref (GdkContentFormats *formats)
{
  g_return_if_fail (formats != NULL);
  g_return_if_fail (formats->ref_count > 0);

  formats->ref_count--;
  if (formats->ref_count > 0)
    return;

  g_free (formats->gtypes);
  g_free (formats->mime_types);
  g_free (formats);
}

/**
 * gdk_content_formats_print:
 * @formats: a `GdkContentFormats`
 * @string: a `GString` to print into
 *
 * Prints the given @formats into a string for human consumption.
 *
 * The result of this function can later be parsed with
 * [func@Gdk.ContentFormats.parse].
 */
void
gdk_content_formats_print (GdkContentFormats *formats,
                           GString           *string)
{
  gsize i;

  g_return_if_fail (formats != NULL);
  g_return_if_fail (string != NULL);

  for (i = 0; i < formats->n_gtypes; i++)
    {
      if (i > 0)
        g_string_append (string, " ");
      g_string_append (string, g_type_name (formats->gtypes[i]));
    }
  for (i = 0; i < formats->n_mime_types; i++)
    {
      if (i > 0 || formats->n_gtypes > 0)
        g_string_append (string, " ");
      g_string_append (string, formats->mime_types[i]);
    }
}

/**
 * gdk_content_formats_to_string:
 * @formats: a `GdkContentFormats`
 *
 * Prints the given @formats into a human-readable string.
 *
 * The resulting string can be parsed with [func@Gdk.ContentFormats.parse].
 *
 * This is a small wrapper around [method@Gdk.ContentFormats.print]
 * to help when debugging.
 *
 * Returns: (transfer full): a new string
 */
char *
gdk_content_formats_to_string (GdkContentFormats *formats)
{
  GString *string;

  g_return_val_if_fail (formats != NULL, NULL);

  string = g_string_new (NULL);
  gdk_content_formats_print (formats, string);

  return g_string_free (string, FALSE);
}

/**
 * gdk_content_formats_union:
 * @first: (transfer full): the `GdkContentFormats` to merge into
 * @second: (transfer none): the `GdkContentFormats` to merge from
 *
 * Append all missing types from @second to @first, in the order
 * they had in @second.
 *
 * Returns: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_union (GdkContentFormats       *first,
                           const GdkContentFormats *second)
{
  GdkContentFormatsBuilder *builder;

  g_return_val_if_fail (first != NULL, NULL);
  g_return_val_if_fail (second != NULL, NULL);

  builder = gdk_content_formats_builder_new ();

  gdk_content_formats_builder_add_formats (builder, first);
  gdk_content_formats_unref (first);
  gdk_content_formats_builder_add_formats (builder, second);

  return gdk_content_formats_builder_free_to_formats (builder);
}

static gboolean
gdk_content_formats_contain_interned_mime_type (const GdkContentFormats *formats,
                                                const char              *mime_type)
{
  gsize i;

  for (i = 0; i < formats->n_mime_types; i++)
    {
      if (mime_type == formats->mime_types[i])
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_content_formats_match:
 * @first: the primary `GdkContentFormats` to intersect
 * @second: the `GdkContentFormats` to intersect with
 *
 * Checks if @first and @second have any matching formats.
 *
 * Returns: %TRUE if a matching format was found.
 */
gboolean
gdk_content_formats_match (const GdkContentFormats *first,
                           const GdkContentFormats *second)
{
  g_return_val_if_fail (first != NULL, FALSE);
  g_return_val_if_fail (second != NULL, FALSE);

  return gdk_content_formats_match_gtype (first, second) != G_TYPE_INVALID
      || gdk_content_formats_match_mime_type (first, second) != NULL;
}

/**
 * gdk_content_formats_match_gtype:
 * @first: the primary `GdkContentFormats` to intersect
 * @second: the `GdkContentFormats` to intersect with
 *
 * Finds the first `GType` from @first that is also contained
 * in @second.
 *
 * If no matching `GType` is found, %G_TYPE_INVALID is returned.
 *
 * Returns: The first common `GType` or %G_TYPE_INVALID if none.
 */
GType
gdk_content_formats_match_gtype (const GdkContentFormats *first,
                                 const GdkContentFormats *second)
{
  gsize i;

  g_return_val_if_fail (first != NULL, FALSE);
  g_return_val_if_fail (second != NULL, FALSE);

  for (i = 0; i < first->n_gtypes; i++)
    {
      if (gdk_content_formats_contain_gtype (second, first->gtypes[i]))
        return first->gtypes[i];
    }

  return G_TYPE_INVALID;
}

/**
 * gdk_content_formats_match_mime_type:
 * @first: the primary `GdkContentFormats` to intersect
 * @second: the `GdkContentFormats` to intersect with
 *
 * Finds the first mime type from @first that is also contained
 * in @second.
 *
 * If no matching mime type is found, %NULL is returned.
 *
 * Returns: (nullable): The first common mime type or %NULL if none
 */
const char *
gdk_content_formats_match_mime_type (const GdkContentFormats *first,
                                     const GdkContentFormats *second)
{
  gsize i;

  g_return_val_if_fail (first != NULL, FALSE);
  g_return_val_if_fail (second != NULL, FALSE);

  for (i = 0; i < first->n_mime_types; i++)
    {
      if (gdk_content_formats_contain_interned_mime_type (second, first->mime_types[i]))
        return first->mime_types[i];
    }

  return NULL;
}

/**
 * gdk_content_formats_contain_gtype:
 * @formats: a `GdkContentFormats`
 * @type: the `GType` to search for
 *
 * Checks if a given `GType` is part of the given @formats.
 *
 * Returns: %TRUE if the `GType` was found
 */
gboolean
gdk_content_formats_contain_gtype (const GdkContentFormats *formats,
                                   GType                    type)
{
  g_return_val_if_fail (formats != NULL, FALSE);

  gsize i;

  for (i = 0; i < formats->n_gtypes; i++)
    {
      if (type == formats->gtypes[i])
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_content_formats_contain_mime_type:
 * @formats: a `GdkContentFormats`
 * @mime_type: the mime type to search for
 *
 * Checks if a given mime type is part of the given @formats.
 *
 * Returns: %TRUE if the mime_type was found
 */
gboolean
gdk_content_formats_contain_mime_type (const GdkContentFormats *formats,
                                       const char              *mime_type)
{
  g_return_val_if_fail (formats != NULL, FALSE);
  g_return_val_if_fail (mime_type != NULL, FALSE);

  return gdk_content_formats_contain_interned_mime_type (formats,
                                                         g_intern_string (mime_type));
}

/**
 * gdk_content_formats_get_gtypes:
 * @formats: a `GdkContentFormats`
 * @n_gtypes: (out) (optional): optional pointer to take the
 *   number of `GType`s contained in the return value
 *
 * Gets the `GType`s included in @formats.
 *
 * Note that @formats may not contain any `GType`s, in particular when
 * they are empty. In that case %NULL will be returned.
 *
 * Returns: (transfer none) (nullable) (array length=n_gtypes zero-terminated=1):
 *   %G_TYPE_INVALID-terminated array of types included in @formats
 */
const GType *
gdk_content_formats_get_gtypes (const GdkContentFormats *formats,
                                gsize                   *n_gtypes)
{
  g_return_val_if_fail (formats != NULL, NULL);

  if (n_gtypes)
    *n_gtypes = formats->n_gtypes;

  return formats->gtypes;
}

/**
 * gdk_content_formats_get_mime_types:
 * @formats: a `GdkContentFormats`
 * @n_mime_types: (out) (optional): optional pointer to take the
 *   number of mime types contained in the return value
 *
 * Gets the mime types included in @formats.
 *
 * Note that @formats may not contain any mime types, in particular
 * when they are empty. In that case %NULL will be returned.
 *
 * Returns: (transfer none) (nullable) (array length=n_mime_types zero-terminated=1):
 *   %NULL-terminated array of interned strings of mime types included
 *   in @formats
 */
const char * const *
gdk_content_formats_get_mime_types (const GdkContentFormats *formats,
                                    gsize                   *n_mime_types)
{
  g_return_val_if_fail (formats != NULL, NULL);

  if (n_mime_types)
    *n_mime_types = formats->n_mime_types;

  return formats->mime_types;
}

/**
 * GdkContentFormatsBuilder:
 *
 * A `GdkContentFormatsBuilder` is an auxiliary struct used to create
 * new `GdkContentFormats`, and should not be kept around.
 */

struct _GdkContentFormatsBuilder
{
  int ref_count;

  /* (element-type GType) */
  GSList *gtypes;
  gsize n_gtypes;

  /* (element-type utf8) (interned) */
  GSList *mime_types;
  gsize n_mime_types;
};

G_DEFINE_BOXED_TYPE (GdkContentFormatsBuilder,
                     gdk_content_formats_builder,
                     gdk_content_formats_builder_ref,
                     gdk_content_formats_builder_unref)

/**
 * gdk_content_formats_builder_new:
 *
 * Create a new `GdkContentFormatsBuilder` object.
 *
 * The resulting builder would create an empty `GdkContentFormats`.
 * Use addition functions to add types to it.
 *
 * Returns: a new `GdkContentFormatsBuilder`
 */
GdkContentFormatsBuilder *
gdk_content_formats_builder_new (void)
{
  GdkContentFormatsBuilder *builder;

  builder = g_new0 (GdkContentFormatsBuilder, 1);
  builder->ref_count = 1;

  return builder;
}

/**
 * gdk_content_formats_builder_ref:
 * @builder: a `GdkContentFormatsBuilder`
 *
 * Acquires a reference on the given @builder.
 *
 * This function is intended primarily for bindings.
 * `GdkContentFormatsBuilder` objects should not be kept around.
 *
 * Returns: (transfer none): the given `GdkContentFormatsBuilder`
 *   with its reference count increased
 */
GdkContentFormatsBuilder *
gdk_content_formats_builder_ref (GdkContentFormatsBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (builder->ref_count > 0, NULL);

  builder->ref_count += 1;

  return builder;
}

static void
gdk_content_formats_builder_clear (GdkContentFormatsBuilder *builder)
{
  g_clear_pointer (&builder->gtypes, g_slist_free);
  g_clear_pointer (&builder->mime_types, g_slist_free);

  builder->n_gtypes = 0;
  builder->n_mime_types = 0;
}

/**
 * gdk_content_formats_builder_unref:
 * @builder: a `GdkContentFormatsBuilder`
 *
 * Releases a reference on the given @builder.
 */
void
gdk_content_formats_builder_unref (GdkContentFormatsBuilder *builder)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->ref_count > 0);

  builder->ref_count -= 1;

  if (builder->ref_count > 0)
    return;

  gdk_content_formats_builder_clear (builder);
  g_free (builder);
}

/**
 * gdk_content_formats_builder_free_to_formats: (skip)
 * @builder: a `GdkContentFormatsBuilder`
 *
 * Creates a new `GdkContentFormats` from the current state of the
 * given @builder, and frees the @builder instance.
 *
 * Returns: (transfer full): the newly created `GdkContentFormats`
 *   with all the formats added to @builder
 */
GdkContentFormats *
gdk_content_formats_builder_free_to_formats (GdkContentFormatsBuilder *builder)
{
  GdkContentFormats *res;

  g_return_val_if_fail (builder != NULL, NULL);

  res = gdk_content_formats_builder_to_formats (builder);

  gdk_content_formats_builder_unref (builder);

  return res;
}

/**
 * gdk_content_formats_builder_to_formats:
 * @builder: a `GdkContentFormats`Builder
 *
 * Creates a new `GdkContentFormats` from the given @builder.
 *
 * The given `GdkContentFormatsBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same @builder instance.
 *
 * This function is intended primarily for bindings. C code should use
 * [method@Gdk.ContentFormatsBuilder.free_to_formats].
 *
 * Returns: (transfer full): the newly created `GdkContentFormats`
 *   with all the formats added to @builder
 */
GdkContentFormats *
gdk_content_formats_builder_to_formats (GdkContentFormatsBuilder *builder)
{
  GdkContentFormats *result;
  GType *gtypes;
  const char **mime_types;
  GSList *l;
  gsize i;

  g_return_val_if_fail (builder != NULL, NULL);

  if (builder->n_gtypes > 0)
    {
      gtypes = g_new (GType, builder->n_gtypes + 1);
      i = builder->n_gtypes;
      gtypes[i--] = G_TYPE_INVALID;
      /* add backwards because most important type is last in the list */
      for (l = builder->gtypes; l; l = l->next)
        gtypes[i--] = GPOINTER_TO_SIZE (l->data);
    }
  else
    {
      gtypes = NULL;
    }

  if (builder->n_mime_types > 0)
    {
      mime_types = g_new (const char *, builder->n_mime_types + 1);
      i = builder->n_mime_types;
      mime_types[i--] = NULL;
      /* add backwards because most important type is last in the list */
      for (l = builder->mime_types; l; l = l->next)
        mime_types[i--] = l->data;
    }
  else
    {
      mime_types = NULL;
    }

  result = gdk_content_formats_new_take (gtypes, builder->n_gtypes,
                                         mime_types, builder->n_mime_types);

  gdk_content_formats_builder_clear (builder);

  return result;
}

/**
 * gdk_content_formats_builder_add_formats:
 * @builder: a `GdkContentFormatsBuilder`
 * @formats: the formats to add
 *
 * Appends all formats from @formats to @builder, skipping those that
 * already exist.
 */
void
gdk_content_formats_builder_add_formats (GdkContentFormatsBuilder *builder,
                                         const GdkContentFormats  *formats)
{
  gsize i;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (formats != NULL);

  for (i = 0; i < formats->n_gtypes; i++)
    gdk_content_formats_builder_add_gtype (builder, formats->gtypes[i]);

  for (i = 0; i < formats->n_mime_types; i++)
    gdk_content_formats_builder_add_mime_type (builder, formats->mime_types[i]);
}

/**
 * gdk_content_formats_builder_add_gtype:
 * @builder: a `GdkContentFormats`Builder
 * @type: a `GType`
 *
 * Appends @type to @builder if it has not already been added.
 **/
void
gdk_content_formats_builder_add_gtype (GdkContentFormatsBuilder *builder,
                                       GType                     type)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (type != G_TYPE_INVALID);

  if (g_slist_find (builder->gtypes, GSIZE_TO_POINTER (type)))
    return;

  builder->gtypes = g_slist_prepend (builder->gtypes, GSIZE_TO_POINTER (type));
  builder->n_gtypes++;
}

/**
 * gdk_content_formats_builder_add_mime_type:
 * @builder: a `GdkContentFormatsBuilder`
 * @mime_type: a mime type
 *
 * Appends @mime_type to @builder if it has not already been added.
 */
void
gdk_content_formats_builder_add_mime_type (GdkContentFormatsBuilder *builder,
                                           const char               *mime_type)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (mime_type != NULL);

  mime_type = g_intern_string (mime_type);

  if (g_slist_find (builder->mime_types, mime_type))
    return;

  builder->mime_types = g_slist_prepend (builder->mime_types, (gpointer) mime_type);
  builder->n_mime_types++;
}

/* {{{ GdkFileList */

/* We're using GdkFileList* and GSList* interchangeably, counting on the
 * fact that we're just passing around gpointers; the only reason why we
 * have a GdkFileList opaque type is for language bindings, because they
 * can have no idea what a GSList of GFiles is.
 */

static gpointer
gdk_file_list_copy (gpointer list)
{
  return g_slist_copy_deep (list, (GCopyFunc) g_object_ref, NULL);
}

static void
gdk_file_list_free (gpointer list)
{
  g_slist_free_full (list, g_object_unref);
}

G_DEFINE_BOXED_TYPE (GdkFileList, gdk_file_list, gdk_file_list_copy, gdk_file_list_free)

/**
 * gdk_file_list_get_files:
 * @file_list: the file list
 *
 * Retrieves the list of files inside a `GdkFileList`.
 *
 * This function is meant for language bindings.
 *
 * Returns: (transfer container) (element-type GFile): the files inside the list
 *
 * Since: 4.6
 */
GSList *
gdk_file_list_get_files (GdkFileList *file_list)
{
  return g_slist_copy ((GSList *) file_list);
}

/**
 * gdk_file_list_new_from_list:
 * @files: (element-type GFile): a list of files
 *
 * Creates a new files list container from a singly linked list of
 * `GFile` instances.
 *
 * This function is meant to be used by language bindings
 *
 * Returns: (transfer full): the newly created files list
 *
 * Since: 4.8
 */
GdkFileList *
gdk_file_list_new_from_list (GSList *files)
{
  return gdk_file_list_copy (files);
}

/**
 * gdk_file_list_new_from_array:
 * @files: (array length=n_files): the files to add to the list
 * @n_files: the number of files in the array
 *
 * Creates a new `GdkFileList` for the given array of files.
 *
 * This function is meant to be used by language bindings.
 *
 * Returns: (transfer full): the newly create files list
 *
 * Since: 4.8
 */
GdkFileList *
gdk_file_list_new_from_array (GFile **files,
                              gsize   n_files)
{
  if (files == NULL || n_files == 0)
    return NULL;

  GSList *res = NULL;
  for (gssize i = n_files - 1; i >= 0; i--)
    res = g_slist_prepend (res, g_object_ref (files[i]));

  return (GdkFileList *) res;
}

/* }}} */
