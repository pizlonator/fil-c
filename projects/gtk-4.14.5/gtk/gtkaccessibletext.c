/* gtkaccessibletext.c: Interface for accessible text objects
 *
 * SPDX-FileCopyrightText: 2023  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkaccessibletextprivate.h"

#include "gtkatcontextprivate.h"

G_DEFINE_INTERFACE (GtkAccessibleText, gtk_accessible_text, GTK_TYPE_ACCESSIBLE)

static GBytes *
gtk_accessible_text_default_get_contents (GtkAccessibleText *self,
                                          unsigned int start,
                                          unsigned int end)
{
  return NULL;
}

static GBytes *
gtk_accessible_text_default_get_contents_at (GtkAccessibleText            *self,
                                             unsigned int                  offset,
                                             GtkAccessibleTextGranularity  granularity,
                                             unsigned int                 *start,
                                             unsigned int                 *end)
{
  if (start != NULL)
    *start = 0;
  if (end != NULL)
    *end = 0;

  return NULL;
}

static unsigned int
gtk_accessible_text_default_get_caret_position (GtkAccessibleText *self)
{
  return 0;
}

static gboolean
gtk_accessible_text_default_get_selection (GtkAccessibleText       *self,
                                           gsize                   *n_ranges,
                                           GtkAccessibleTextRange **ranges)
{
  return FALSE;
}

static gboolean
gtk_accessible_text_default_get_attributes (GtkAccessibleText        *self,
                                            unsigned int              offset,
                                            gsize                    *n_ranges,
                                            GtkAccessibleTextRange  **ranges,
                                            char                   ***attribute_names,
                                            char                   ***attribute_values)
{
  *attribute_names = NULL;
  *attribute_values = NULL;
  *n_ranges = 0;
  return FALSE;
}

static void
gtk_accessible_text_default_get_default_attributes (GtkAccessibleText   *self,
                                                    char              ***attribute_names,
                                                    char              ***attribute_values)
{
  *attribute_names = NULL;
  *attribute_values = NULL;
}

static void
gtk_accessible_text_default_init (GtkAccessibleTextInterface *iface)
{
  iface->get_contents = gtk_accessible_text_default_get_contents;
  iface->get_contents_at = gtk_accessible_text_default_get_contents_at;
  iface->get_caret_position = gtk_accessible_text_default_get_caret_position;
  iface->get_selection = gtk_accessible_text_default_get_selection;
  iface->get_attributes = gtk_accessible_text_default_get_attributes;
  iface->get_default_attributes = gtk_accessible_text_default_get_default_attributes;
}

static GBytes *
nul_terminate_contents (GBytes *bytes)
{
  const char *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);
  if (size == 0 || (size > 0 && data[size - 1] != '\0'))
    {
      guchar *copy;

      copy = g_new (guchar, size + 1);
      if (size > 0)
        memcpy (copy, data, size);
      copy[size] = '\0';

      g_bytes_unref (bytes);
      bytes = g_bytes_new_take (copy, size + 1);
    }

  return bytes;
}

/*< private >
 * gtk_accessible_text_get_contents:
 * @self: the accessible object
 * @start: the beginning of the range, in characters
 * @end: the end of the range, in characters
 *
 * Retrieve the current contents of the accessible object within
 * the given range.
 *
 * If @end is `G_MAXUINT`, the end of the range is the full content of the
 * accessible object.
 *
 * Returns: (transfer full): the requested slice of the contents of the
 *   accessible object, as NUL-terminated UTF-8
 *
 * Since: 4.14
 */
GBytes *
gtk_accessible_text_get_contents (GtkAccessibleText *self,
                                  unsigned int       start,
                                  unsigned int       end)
{
  GBytes *bytes;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), NULL);
  g_return_val_if_fail (end >= start, NULL);

  bytes = GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_contents (self, start, end);

  return nul_terminate_contents (bytes);
}

/*< private >
 * gtk_accessible_text_get_contents_at:
 * @self: the accessible object
 * @offset: the offset of the text to retrieve
 * @granularity: specify the boundaries of the text
 * @start: (out): the starting offset of the contents, in characters
 * @end: (out): the ending offset of the contents, in characters
 *
 * Retrieve the current contents of the accessible object at the given
 * offset.
 *
 * Using the @granularity enumeration allows to adjust the offset so that
 * this function can return the beginning of the word, line, or sentence;
 * the initial and final boundaries are stored in @start and @end.
 *
 * Returns: (transfer full): the requested slice of the contents of the
 *   accessible object, as NUL-terminated UTF-8 buffer
 *
 * Since: 4.14
 */
GBytes *
gtk_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                     unsigned int                  offset,
                                     GtkAccessibleTextGranularity  granularity,
                                     unsigned int                 *start,
                                     unsigned int                 *end)
{
  static const char empty[] = {0};
  GBytes *bytes;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), NULL);

  bytes = GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_contents_at (self, offset, granularity, start, end);

  if (bytes == NULL)
    return g_bytes_new_static (empty, sizeof empty);

  return nul_terminate_contents (bytes);
}

/*< private >
 * gtk_accessible_text_get_caret_position:
 * @self: the accessible object
 *
 * Retrieves the position of the caret inside the accessible object.
 *
 * If the accessible has no caret, 0 is returned.
 *
 * Returns: the position of the caret, in characters
 *
 * Since: 4.14
 */
unsigned int
gtk_accessible_text_get_caret_position (GtkAccessibleText *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), 0);

  return GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_caret_position (self);
}

/*< private >
 * gtk_accessible_text_get_selection:
 * @self: the accessible object
 * @n_ranges: (out): the number of selection ranges
 * @ranges: (optional) (out) (array length=n_ranges): the selection ranges
 *
 * Retrieves the selection ranges in the accessible object.
 *
 * If this function returns true, `n_ranges` will be set to a value
 * greater than or equal to one, and @ranges will be set to a newly
 * allocated array of [struct#Gtk.AccessibleTextRange].
 *
 * Returns: true if there's at least one selected range inside the
 *   accessible object, and false otherwise
 *
 * Since: 4.14
 */
gboolean
gtk_accessible_text_get_selection (GtkAccessibleText       *self,
                                   gsize                   *n_ranges,
                                   GtkAccessibleTextRange **ranges)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), FALSE);

  return GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_selection (self, n_ranges, ranges);
}

/*< private >
 * gtk_accessible_text_get_attributes:
 * @self: the accessible object
 * @offset: the offset, in characters
 * @n_ranges: (out): the number of attributes
 * @ranges: (out) (array length=n_attributes) (optional): the ranges of the attributes
 *   inside the accessible object
 * @attribute_names: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the names of the attributes inside the accessible object
 * @attribute_values: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the values of the attributes inside the accessible object
 *
 * Retrieves the text attributes inside the accessible object.
 *
 * Each attribute is composed by:
 *
 * - a range
 * - a name, typically in the form of a reverse DNS identifier
 * - a value
 *
 * If this function returns true, `n_attributes` will be set to a value
 * greater than or equal to one, @ranges will be set to a newly
 * allocated array of [struct#Gtk.AccessibleTextRange] which should
 * be freed with g_free(), @attribute_names and @attribute_values
 * will be set to string arrays that should be freed with g_strfreev().
 *
 * Returns: true if the accessible object has at least an attribute,
 *   and false otherwise
 *
 * Since: 4.14
 */
gboolean
gtk_accessible_text_get_attributes (GtkAccessibleText        *self,
                                    unsigned int              offset,
                                    gsize                    *n_ranges,
                                    GtkAccessibleTextRange  **ranges,
                                    char                   ***attribute_names,
                                    char                   ***attribute_values)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), FALSE);

  return GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_attributes (self,
                                                               offset,
                                                               n_ranges,
                                                               ranges,
                                                               attribute_names,
                                                               attribute_values);
}

/*< private >
 * gtk_accessible_text_get_default_attributes:
 * @self: the accessible object
 * @attribute_names: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the names of the attributes inside the accessible object
 * @attribute_values: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the values of the attributes inside the accessible object
 *
 * Retrieves the default text attributes inside the accessible object.
 *
 * Each attribute is composed by:
 *
 * - a name, typically in the form of a reverse DNS identifier
 * - a value
 *
 * If this function returns true, `n_attributes` will be set to a value
 * greater than or equal to one, @ranges will be set to a newly
 * allocated array of [struct#Gtk.AccessibleTextRange] which should
 * be freed with g_free(), @attribute_names and @attribute_values
 * will be set to string arrays that should be freed with g_strfreev().
 *
 * Since: 4.14
 */
void
gtk_accessible_text_get_default_attributes (GtkAccessibleText   *self,
                                            char              ***attribute_names,
                                            char              ***attribute_values)
{
  g_return_if_fail (GTK_IS_ACCESSIBLE_TEXT (self));

  GTK_ACCESSIBLE_TEXT_GET_IFACE (self)->get_default_attributes (self,
                                                                attribute_names,
                                                                attribute_values);
}

/*< private >
 * gtk_accessible_text_get_attributes_run:
 * @self: the accessible object
 * @offset: the offset, in characters
 * @include_defaults: whether to include the default attributes in the
 *   returned array
 * @n_ranges: (out): the number of attributes
 * @ranges: (out) (array length=n_attributes) (optional): the ranges of the attributes
 *   inside the accessible object
 * @attribute_names: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the names of the attributes inside the accessible object
 * @attribute_values: (out) (array zero-terminated=1) (element-type utf8) (optional) (transfer full):
 *   the values of the attributes inside the accessible object
 *
 * Retrieves the text attributes inside the accessible object.
 *
 * Each attribute is composed by:
 *
 * - a range
 * - a name, typically in the form of a reverse DNS identifier
 * - a value
 *
 * If this function returns true, `n_ranges` will be set to a value
 * greater than or equal to one, @ranges will be set to a newly
 * allocated array of [struct#Gtk.AccessibleTextRange] which should
 * be freed with g_free(), @attribute_names and @attribute_values
 * will be set to string arrays that should be freed with g_strfreev().
 *
 * Returns: true if the accessible object has at least an attribute,
 *   and false otherwise
 *
 * Since: 4.14
 */
gboolean
gtk_accessible_text_get_attributes_run (GtkAccessibleText        *self,
                                        unsigned int              offset,
                                        gboolean                  include_defaults,
                                        gsize                    *n_ranges,
                                        GtkAccessibleTextRange  **ranges,
                                        char                   ***attribute_names,
                                        char                   ***attribute_values)
{
  GHashTable *attrs;
  GHashTableIter attr_iter;
  gpointer key, value;
  char **attr_names, **attr_values;
  gboolean res;
  GStrvBuilder *names_builder;
  GStrvBuilder *values_builder;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE_TEXT (self), FALSE);

  attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (include_defaults)
    {
      gtk_accessible_text_get_default_attributes (self,
                                                  &attr_names,
                                                  &attr_values);

      for (unsigned i = 0; attr_names[i] != NULL; i++)
        {
          g_hash_table_insert (attrs,
                               g_steal_pointer (&attr_names[i]),
                               g_steal_pointer (&attr_values[i]));
        }

      g_free (attr_names);
      g_free (attr_values);
    }

  res = gtk_accessible_text_get_attributes (self,
                                            offset,
                                            n_ranges,
                                            ranges,
                                            &attr_names,
                                            &attr_values);

  /* If there are no attributes, we can bail out early */
  if (!res && !include_defaults)
    {
      g_hash_table_unref (attrs);
      *attribute_names = NULL;
      *attribute_values = NULL;
      return FALSE;
    }

  /* The text attributes override the default ones */
  for (unsigned i = 0; i < *n_ranges; i++)
    {
      g_hash_table_insert (attrs,
                           g_steal_pointer (&attr_names[i]),
                           g_steal_pointer (&attr_values[i]));
    }

  g_free (attr_names);
  g_free (attr_values);

  names_builder = g_strv_builder_new ();
  values_builder = g_strv_builder_new ();
  g_hash_table_iter_init (&attr_iter, attrs);
  while (g_hash_table_iter_next (&attr_iter, &key, &value))
    {
      g_strv_builder_add (names_builder, key);
      g_strv_builder_add (values_builder, value);
    }

  *attribute_names = g_strv_builder_end (names_builder);
  *attribute_values = g_strv_builder_end (values_builder);

  g_strv_builder_unref (names_builder);
  g_strv_builder_unref (values_builder);
  g_hash_table_unref (attrs);

  return TRUE;
}

/**
 * gtk_accessible_text_update_caret_position:
 * @self: the accessible object
 *
 * Updates the position of the caret.
 *
 * Implementations of the `GtkAccessibleText` interface should call this
 * function every time the caret has moved, in order to notify assistive
 * technologies.
 *
 * Since: 4.14
 */
void
gtk_accessible_text_update_caret_position (GtkAccessibleText *self)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE_TEXT (self));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (self));
  if (context == NULL)
    return;

  gtk_at_context_update_caret_position (context);

  g_object_unref (context);
}

/**
 * gtk_accessible_text_update_selection_bound:
 * @self: the accessible object
 *
 * Updates the boundary of the selection.
 *
 * Implementations of the `GtkAccessibleText` interface should call this
 * function every time the selection has moved, in order to notify assistive
 * technologies.
 *
 * Since: 4.14
 */
void
gtk_accessible_text_update_selection_bound (GtkAccessibleText *self)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE_TEXT (self));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (self));
  if (context == NULL)
    return;

  gtk_at_context_update_selection_bound (context);

  g_object_unref (context);
}

/**
 * gtk_accessible_text_update_contents:
 * @self: the accessible object
 * @change: the type of change in the contents
 * @start: the starting offset of the change, in characters
 * @end: the end offset of the change, in characters
 *
 * Notifies assistive technologies of a change in contents.
 *
 * Implementations of the `GtkAccessibleText` interface should call this
 * function every time their contents change as the result of an operation,
 * like an insertion or a removal.
 *
 * Note: If the change is a deletion, this function must be called *before*
 * removing the contents, if it is an insertion, it must be called *after*
 * inserting the new contents.
 *
 * Since: 4.14
 */
void
gtk_accessible_text_update_contents (GtkAccessibleText              *self,
                                     GtkAccessibleTextContentChange  change,
                                     unsigned int                    start,
                                     unsigned int                    end)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE_TEXT (self));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (self));
  if (context == NULL)
    return;

  gtk_at_context_update_text_contents (context, change, start, end);

  g_object_unref (context);
}
