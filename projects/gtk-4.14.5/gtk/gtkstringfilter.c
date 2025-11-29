/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkstringfilter.h"

#include "gtktypebuiltins.h"

/**
 * GtkStringFilter:
 *
 * `GtkStringFilter` determines whether to include items by comparing
 * strings to a fixed search term.
 *
 * The strings are obtained from the items by evaluating a `GtkExpression`
 * set with [method@Gtk.StringFilter.set_expression], and they are
 * compared against a search term set with [method@Gtk.StringFilter.set_search].
 *
 * `GtkStringFilter` has several different modes of comparison - it
 * can match the whole string, just a prefix, or any substring. Use
 * [method@Gtk.StringFilter.set_match_mode] choose a mode.
 *
 * It is also possible to make case-insensitive comparisons, with
 * [method@Gtk.StringFilter.set_ignore_case].
 */

struct _GtkStringFilter
{
  GtkFilter parent_instance;

  char *search;
  char *search_prepared;

  gboolean ignore_case;
  GtkStringFilterMatchMode match_mode;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_IGNORE_CASE,
  PROP_MATCH_MODE,
  PROP_SEARCH,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringFilter, gtk_string_filter, GTK_TYPE_FILTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static char *
gtk_string_filter_prepare (GtkStringFilter *self,
                           const char      *s)
{
  char *tmp;
  char *result;

  if (s == NULL || s[0] == '\0')
    return NULL;

  tmp = g_utf8_normalize (s, -1, G_NORMALIZE_ALL);

  if (!self->ignore_case)
    return tmp;

  result = g_utf8_casefold (tmp, -1);
  g_free (tmp);

  return result;
}

/* This is necessary because code just looks at self->search otherwise
 * and that can be the empty string...
 */
static gboolean
gtk_string_filter_has_search (GtkStringFilter *self)
{
  return self->search_prepared != NULL;
}

static gboolean
gtk_string_filter_match (GtkFilter *filter,
                         gpointer   item)
{
  GtkStringFilter *self = GTK_STRING_FILTER (filter);
  GValue value = G_VALUE_INIT;
  char *prepared;
  const char *s;
  gboolean result;

  if (!gtk_string_filter_has_search (self))
    return TRUE;

  if (self->expression == NULL ||
      !gtk_expression_evaluate (self->expression, item, &value))
    return FALSE;
  s = g_value_get_string (&value);
  prepared = gtk_string_filter_prepare (self, s);
  if (prepared == NULL)
    return FALSE;

  switch (self->match_mode)
    {
    case GTK_STRING_FILTER_MATCH_MODE_EXACT:
      result = strcmp (prepared, self->search_prepared) == 0;
      break;
    case GTK_STRING_FILTER_MATCH_MODE_SUBSTRING:
      result = strstr (prepared, self->search_prepared) != NULL;
      break;
    case GTK_STRING_FILTER_MATCH_MODE_PREFIX:
      result = g_str_has_prefix (prepared, self->search_prepared);
      break;
    default:
      g_assert_not_reached ();
    }

#if 0
  g_print ("%s (%s) %s %s (%s)\n", s, prepared, result ? "==" : "!=", self->search, self->search_prepared);
#endif

  g_free (prepared);
  g_value_unset (&value);

  return result;
}

static GtkFilterMatch
gtk_string_filter_get_strictness (GtkFilter *filter)
{
  GtkStringFilter *self = GTK_STRING_FILTER (filter);

  if (!gtk_string_filter_has_search (self))
    return GTK_FILTER_MATCH_ALL;

  if (self->expression == NULL)
    return GTK_FILTER_MATCH_NONE;

  return GTK_FILTER_MATCH_SOME;
}

static void
gtk_string_filter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_string_filter_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_IGNORE_CASE:
      gtk_string_filter_set_ignore_case (self, g_value_get_boolean (value));
      break;

    case PROP_MATCH_MODE:
      gtk_string_filter_set_match_mode (self, g_value_get_enum (value));
      break;

    case PROP_SEARCH:
      gtk_string_filter_set_search (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_string_filter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
      break;

    case PROP_IGNORE_CASE:
      g_value_set_boolean (value, self->ignore_case);
      break;

    case PROP_MATCH_MODE:
      g_value_set_enum (value, self->match_mode);
      break;

    case PROP_SEARCH:
      g_value_set_string (value, self->search);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_filter_dispose (GObject *object)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  g_clear_pointer (&self->search, g_free);
  g_clear_pointer (&self->search_prepared, g_free);
  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_string_filter_parent_class)->dispose (object);
}

static void
gtk_string_filter_class_init (GtkStringFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->match = gtk_string_filter_match;
  filter_class->get_strictness = gtk_string_filter_get_strictness;

  object_class->get_property = gtk_string_filter_get_property;
  object_class->set_property = gtk_string_filter_set_property;
  object_class->dispose = gtk_string_filter_dispose;

  /**
   * GtkStringFilter:expression: (type GtkExpression) (attributes org.gtk.Property.get=gtk_string_filter_get_expression org.gtk.Property.set=gtk_string_filter_set_expression)
   *
   * The expression to evaluate on item to get a string to compare with.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression", NULL, NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringFilter:ignore-case: (attributes org.gtk.Property.get=gtk_string_filter_get_ignore_case org.gtk.Property.set=gtk_string_filter_set_ignore_case)
   *
   * If matching is case sensitive.
   */
  properties[PROP_IGNORE_CASE] =
      g_param_spec_boolean ("ignore-case", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringFilter:match-mode: (attributes org.gtk.Property.get=gtk_string_filter_get_match_mode org.gtk.Property.set=gtk_string_filter_set_match_mode)
   *
   * If exact matches are necessary or if substrings are allowed.
   */
  properties[PROP_MATCH_MODE] =
      g_param_spec_enum ("match-mode", NULL, NULL,
                         GTK_TYPE_STRING_FILTER_MATCH_MODE,
                         GTK_STRING_FILTER_MATCH_MODE_SUBSTRING,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringFilter:search: (attributes org.gtk.Property.get=gtk_string_filter_get_search org.gtk.Property.set=gtk_string_filter_set_search)
   *
   * The search term.
   */
  properties[PROP_SEARCH] =
      g_param_spec_string ("search", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_string_filter_init (GtkStringFilter *self)
{
  self->ignore_case = TRUE;
  self->match_mode = GTK_STRING_FILTER_MATCH_MODE_SUBSTRING;
}

/**
 * gtk_string_filter_new:
 * @expression: (transfer full) (nullable): The expression to evaluate
 *
 * Creates a new string filter.
 *
 * You will want to set up the filter by providing a string to search for
 * and by providing a property to look up on the item.
 *
 * Returns: a new `GtkStringFilter`
 */
GtkStringFilter *
gtk_string_filter_new (GtkExpression *expression)
{
  GtkStringFilter *result;

  result = g_object_new (GTK_TYPE_STRING_FILTER,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_string_filter_get_search: (attributes org.gtk.Method.get_property=search)
 * @self: a `GtkStringFilter`
 *
 * Gets the search term.
 *
 * Returns: (nullable) (transfer none): The search term
 **/
const char *
gtk_string_filter_get_search (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), NULL);

  return self->search;
}

/**
 * gtk_string_filter_set_search: (attributes org.gtk.Method.set_property=search)
 * @self: a `GtkStringFilter`
 * @search: (transfer none) (nullable): The string to search for
 *   or %NULL to clear the search
 *
 * Sets the string to search for.
 */
void
gtk_string_filter_set_search (GtkStringFilter *self,
                              const char      *search)
{
  GtkFilterChange change;

  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (g_strcmp0 (self->search, search) == 0)
    return;

  if (search == NULL || search[0] == 0)
    change = GTK_FILTER_CHANGE_LESS_STRICT;
  else if (!gtk_string_filter_has_search (self))
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else if (g_str_has_prefix (search, self->search))
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else if (g_str_has_prefix (self->search, search))
    change = GTK_FILTER_CHANGE_LESS_STRICT;
  else
    change = GTK_FILTER_CHANGE_DIFFERENT;

  g_free (self->search);
  g_free (self->search_prepared);

  self->search = g_strdup (search);
  self->search_prepared = gtk_string_filter_prepare (self, search);

  gtk_filter_changed (GTK_FILTER (self), change);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH]);
}

/**
 * gtk_string_filter_get_expression: (attributes org.gtk.Method.get_property=expression)
 * @self: a `GtkStringFilter`
 *
 * Gets the expression that the string filter uses to
 * obtain strings from items.
 *
 * Returns: (transfer none) (nullable): a `GtkExpression`
 */
GtkExpression *
gtk_string_filter_get_expression (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), NULL);

  return self->expression;
}

/**
 * gtk_string_filter_set_expression: (attributes org.gtk.Method.set_property=expression)
 * @self: a `GtkStringFilter`
 * @expression: (nullable): a `GtkExpression`
 *
 * Sets the expression that the string filter uses to
 * obtain strings from items.
 *
 * The expression must have a value type of %G_TYPE_STRING.
 */
void
gtk_string_filter_set_expression (GtkStringFilter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_STRING_FILTER (self));
  g_return_if_fail (expression == NULL || gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  self->expression = gtk_expression_ref (expression);

  if (gtk_string_filter_has_search (self))
    gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_string_filter_get_ignore_case: (attributes org.gtk.Method.get_property=ignore-case)
 * @self: a `GtkStringFilter`
 *
 * Returns whether the filter ignores case differences.
 *
 * Returns: %TRUE if the filter ignores case
 */
gboolean
gtk_string_filter_get_ignore_case (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), TRUE);

  return self->ignore_case;
}

/**
 * gtk_string_filter_set_ignore_case: (attributes org.gtk.Method.set_property=ignore-case)
 * @self: a `GtkStringFilter`
 * @ignore_case: %TRUE to ignore case
 *
 * Sets whether the filter ignores case differences.
 */
void
gtk_string_filter_set_ignore_case (GtkStringFilter *self,
                                   gboolean         ignore_case)
{
  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (self->ignore_case == ignore_case)
    return;

  self->ignore_case = ignore_case;

  if (self->search)
    {
      g_free (self->search_prepared);
      self->search_prepared = gtk_string_filter_prepare (self, self->search);
      gtk_filter_changed (GTK_FILTER (self), ignore_case ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_CASE]);
}

/**
 * gtk_string_filter_get_match_mode: (attributes org.gtk.Method.get_property=match-mode)
 * @self: a `GtkStringFilter`
 *
 * Returns the match mode that the filter is using.
 *
 * Returns: the match mode of the filter
 */
GtkStringFilterMatchMode
gtk_string_filter_get_match_mode (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), GTK_STRING_FILTER_MATCH_MODE_EXACT);

  return self->match_mode;
}

/**
 * gtk_string_filter_set_match_mode: (attributes org.gtk.Method.set_property=match-mode)
 * @self: a `GtkStringFilter`
 * @mode: the new match mode
 *
 * Sets the match mode for the filter.
 */
void
gtk_string_filter_set_match_mode (GtkStringFilter *self,
                                  GtkStringFilterMatchMode mode)
{
  GtkStringFilterMatchMode old_mode;

  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (self->match_mode == mode)
    return;

  old_mode = self->match_mode;
  self->match_mode = mode;

  if (self->search_prepared && self->expression)
    {
      switch (old_mode)
        {
        case GTK_STRING_FILTER_MATCH_MODE_EXACT:
          gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_LESS_STRICT);
          break;

        case GTK_STRING_FILTER_MATCH_MODE_SUBSTRING:
          gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_MORE_STRICT);
          break;

        case GTK_STRING_FILTER_MATCH_MODE_PREFIX:
          if (mode == GTK_STRING_FILTER_MATCH_MODE_SUBSTRING)
            gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_LESS_STRICT);
          else
            gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_MORE_STRICT);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATCH_MODE]);
}
