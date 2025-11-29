/* gtktestatcontext.c: Test AT context
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktestatcontextprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkaccessibletextprivate.h"
#include "gtkdebug.h"
#include "gtkenums.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

struct _GtkTestATContext
{
  GtkATContext parent_instance;
};

struct _GtkTestATContextClass
{
  GtkATContextClass parent_class;
};

enum {
  UPDATE_CARET_POSITION,
  UPDATE_SELECTION_BOUND,
  UPDATE_TEXT_CONTENTS,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GtkTestATContext, gtk_test_at_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_test_at_context_state_change (GtkATContext                *self,
                                  GtkAccessibleStateChange     changed_states,
                                  GtkAccessiblePropertyChange  changed_properties,
                                  GtkAccessibleRelationChange  changed_relations,
                                  GtkAccessibleAttributeSet   *states,
                                  GtkAccessibleAttributeSet   *properties,
                                  GtkAccessibleAttributeSet   *relations)
{
  if (GTK_DEBUG_CHECK (A11Y))
    {
      char *states_str = gtk_accessible_attribute_set_to_string (states);
      char *properties_str = gtk_accessible_attribute_set_to_string (properties);
      char *relations_str = gtk_accessible_attribute_set_to_string (relations);
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (self);
      GtkAccessible *accessible = gtk_at_context_get_accessible (self);
      GEnumClass *class = g_type_class_ref (GTK_TYPE_ACCESSIBLE_ROLE);
      GEnumValue *value = g_enum_get_value (class, role);

      g_print ("*** Accessible state changed for accessible “%s”, with role “%s” (%d):\n"
           "***     states = %s\n"
           "*** properties = %s\n"
           "***  relations = %s\n",
            G_OBJECT_TYPE_NAME (accessible),
           value->value_nick,
           role,
           states_str,
           properties_str,
           relations_str);
      g_type_class_unref (class);

      g_free (states_str);
      g_free (properties_str);
      g_free (relations_str);
    }
}

static void
gtk_test_at_context_platform_change (GtkATContext                *self,
                                     GtkAccessiblePlatformChange  changed_platform)
{
  if (GTK_DEBUG_CHECK (A11Y))
    {
      GtkAccessible *accessible;

      accessible = gtk_at_context_get_accessible (self);

      g_print ("*** Accessible platform state changed for accessible “%s”:\n",
               G_OBJECT_TYPE_NAME (accessible));
      if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE)
        g_print ("***  focusable = %d\n",
                 gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE));
      if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED)
        g_print ("***    focused = %d\n",
                 gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED));
      if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE)
        g_print ("***    active = %d\n",
                 gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE));
    }
}

static void
gtk_test_at_context_update_caret_position (GtkATContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleText *accessible_text = GTK_ACCESSIBLE_TEXT (accessible);
  unsigned int position;

  position = gtk_accessible_text_get_caret_position (accessible_text);
  g_signal_emit (self, signals[UPDATE_CARET_POSITION], 0, position);
}

static void
gtk_test_at_context_update_selection_bound (GtkATContext *self)
{
  g_signal_emit (self, signals[UPDATE_SELECTION_BOUND], 0);
}

static void
gtk_test_at_context_update_text_contents (GtkATContext                   *self,
                                          GtkAccessibleTextContentChange  change,
                                          unsigned int                    start,
                                          unsigned int                    end)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleText *accessible_text = GTK_ACCESSIBLE_TEXT (accessible);
  GBytes *contents;

  contents = gtk_accessible_text_get_contents (accessible_text, start, end);
  g_signal_emit (self, signals[UPDATE_TEXT_CONTENTS], 0, change, start, end, contents);
  g_bytes_unref (contents);
}

static void
gtk_test_at_context_class_init (GtkTestATContextClass *klass)
{
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  context_class->state_change = gtk_test_at_context_state_change;
  context_class->platform_change = gtk_test_at_context_platform_change;

  context_class->update_caret_position = gtk_test_at_context_update_caret_position;
  context_class->update_selection_bound = gtk_test_at_context_update_selection_bound;
  context_class->update_text_contents = gtk_test_at_context_update_text_contents;

  signals[UPDATE_CARET_POSITION] =
    g_signal_new ("update-caret-position",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[UPDATE_SELECTION_BOUND] =
    g_signal_new ("update-selection-bound",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  signals[UPDATE_TEXT_CONTENTS] =
    g_signal_new ("update-text-contents",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 4,
                  GTK_TYPE_ACCESSIBLE_TEXT_CONTENT_CHANGE,
                  G_TYPE_UINT,
                  G_TYPE_UINT,
                  G_TYPE_BYTES);
}

static void
gtk_test_at_context_init (GtkTestATContext *self)
{
}

/*< private >
 * gtk_test_at_context_new:
 * @accessible_role: the `GtkAccessibleRole` for the AT context
 * @accessible: the `GtkAccessible` instance which owns the AT context
 * @display: a `GdkDisplay`
 *
 * Creates a new `GtkTestATContext` instance for @accessible, using the
 * given @accessible_role.
 *
 * Returns: (transfer full): the newly created `GtkTestATContext` instance
 */
GtkATContext *
gtk_test_at_context_new (GtkAccessibleRole  accessible_role,
                         GtkAccessible     *accessible,
                         GdkDisplay        *display)
{
  return g_object_new (GTK_TYPE_TEST_AT_CONTEXT,
                       "accessible-role", accessible_role,
                       "accessible", accessible,
                       "display", display,
                       NULL);
}

/**
 * gtk_test_accessible_has_role:
 * @accessible: a `GtkAccessible`
 * @role: a `GtkAccessibleRole`
 *
 * Checks whether the `GtkAccessible:accessible-role` of the accessible
 * is @role.
 *
 * Returns: %TRUE if the role matches
 */
gboolean
gtk_test_accessible_has_role (GtkAccessible     *accessible,
                              GtkAccessibleRole  role)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), FALSE);

  return gtk_accessible_get_accessible_role (accessible) == role;
}

/**
 * gtk_test_accessible_has_property:
 * @accessible: a `GtkAccessible`
 * @property: a `GtkAccessibleProperty`
 *
 * Checks whether the `GtkAccessible` has @property set.
 *
 * Returns: %TRUE if the @property is set in the @accessible
 */
gboolean
gtk_test_accessible_has_property (GtkAccessible         *accessible,
                                  GtkAccessibleProperty  property)
{
  GtkATContext *context;
  gboolean res;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), FALSE);

  context = gtk_accessible_get_at_context (accessible);
  if (context == NULL)
    return FALSE;

  res = gtk_at_context_has_accessible_property (context, property);

  g_object_unref (context);

  return res;
}

/**
 * gtk_test_accessible_check_property:
 * @accessible: a `GtkAccessible`
 * @property: a `GtkAccessibleProperty`
 * @...: the expected value of @property
 *
 * Checks whether the accessible @property of @accessible is set to
 * a specific value.
 *
 * Returns: (transfer full): the value of the accessible property
 */
char *
gtk_test_accessible_check_property (GtkAccessible         *accessible,
                                    GtkAccessibleProperty  property,
                                    ...)
{
  char *res = NULL;
  va_list args;

  va_start (args, property);

  GError *error = NULL;
  GtkAccessibleValue *check_value =
    gtk_accessible_value_collect_for_property (property, &error, &args);

  va_end (args);

  if (error != NULL)
    {
      res = g_strdup (error->message);
      g_error_free (error);
      return res;
    }

  if (check_value == NULL)
    check_value = gtk_accessible_value_get_default_for_property (property);

  GtkATContext *context = gtk_accessible_get_at_context (accessible);
  GtkAccessibleValue *real_value =
    gtk_at_context_get_accessible_property (context, property);

  if (gtk_accessible_value_equal (check_value, real_value))
    goto out;

  res = gtk_accessible_value_to_string (real_value);

out:
  gtk_accessible_value_unref (check_value);
  g_object_unref (context);

  return res;
}

/**
 * gtk_test_accessible_has_state:
 * @accessible: a `GtkAccessible`
 * @state: a `GtkAccessibleState`
 *
 * Checks whether the `GtkAccessible` has @state set.
 *
 * Returns: %TRUE if the @state is set in the @accessible
 */
gboolean
gtk_test_accessible_has_state (GtkAccessible      *accessible,
                               GtkAccessibleState  state)
{
  GtkATContext *context;
  gboolean res;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), FALSE);

  context = gtk_accessible_get_at_context (accessible);
  if (context == NULL)
    return FALSE;

  res = gtk_at_context_has_accessible_state (context, state);

  g_object_unref (context);

  return res;
}

/**
 * gtk_test_accessible_check_state:
 * @accessible: a `GtkAccessible`
 * @state: a `GtkAccessibleState`
 * @...: the expected value of @state
 *
 * Checks whether the accessible @state of @accessible is set to
 * a specific value.
 *
 * Returns: (transfer full): the value of the accessible state
 */
char *
gtk_test_accessible_check_state (GtkAccessible      *accessible,
                                 GtkAccessibleState  state,
                                 ...)
{
  char *res = NULL;
  va_list args;

  va_start (args, state);

  GError *error = NULL;
  GtkAccessibleValue *check_value =
    gtk_accessible_value_collect_for_state (state, &error, &args);

  va_end (args);

  if (error != NULL)
    {
      res = g_strdup (error->message);
      g_error_free (error);
      return res;
    }

  if (check_value == NULL)
    check_value = gtk_accessible_value_get_default_for_state (state);

  GtkATContext *context = gtk_accessible_get_at_context (accessible);
  GtkAccessibleValue *real_value =
    gtk_at_context_get_accessible_state (context, state);

  if (gtk_accessible_value_equal (check_value, real_value))
    goto out;

  res = gtk_accessible_value_to_string (real_value);

out:
  gtk_accessible_value_unref (check_value);
  g_object_unref (context);

  return res;
}

/**
 * gtk_test_accessible_has_relation:
 * @accessible: a `GtkAccessible`
 * @relation: a `GtkAccessibleRelation`
 *
 * Checks whether the `GtkAccessible` has @relation set.
 *
 * Returns: %TRUE if the @relation is set in the @accessible
 */
gboolean
gtk_test_accessible_has_relation (GtkAccessible         *accessible,
                                  GtkAccessibleRelation  relation)
{
  GtkATContext *context;
  gboolean res;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), FALSE);

  context = gtk_accessible_get_at_context (accessible);
  if (context == NULL)
    return FALSE;

  res = gtk_at_context_has_accessible_relation (context, relation);

  g_object_unref (context);

  return res;
}

/**
 * gtk_test_accessible_check_relation:
 * @accessible: a `GtkAccessible`
 * @relation: a `GtkAccessibleRelation`
 * @...: the expected value of @relation
 *
 * Checks whether the accessible @relation of @accessible is set to
 * a specific value.
 *
 * Returns: (transfer full): the value of the accessible relation
 */
char *
gtk_test_accessible_check_relation (GtkAccessible         *accessible,
                                    GtkAccessibleRelation  relation,
                                    ...)
{
  char *res = NULL;
  va_list args;

  va_start (args, relation);

  GError *error = NULL;
  GtkAccessibleValue *check_value =
    gtk_accessible_value_collect_for_relation (relation, &error, &args);

  va_end (args);

  if (error != NULL)
    {
      res = g_strdup (error->message);
      g_error_free (error);
      return res;
    }

  if (check_value == NULL)
    check_value = gtk_accessible_value_get_default_for_relation (relation);

  GtkATContext *context = gtk_accessible_get_at_context (accessible);
  GtkAccessibleValue *real_value =
    gtk_at_context_get_accessible_relation (context, relation);

  if (gtk_accessible_value_equal (check_value, real_value))
    goto out;

  res = gtk_accessible_value_to_string (real_value);

out:
  gtk_accessible_value_unref (check_value);
  g_object_unref (context);

  return res;
}

/*< private >
 * gtk_test_accessible_assertion_message_role:
 * @domain: a domain
 * @file: a file name
 * @line: the line in @file
 * @func: a function name in @file
 * @expr: the expression being tested
 * @accessible: a `GtkAccessible`
 * @expected_role: the expected `GtkAccessibleRole`
 * @actual_role: the actual `GtkAccessibleRole`
 *
 * Prints an assertion message for gtk_test_accessible_assert_role().
 */
void
gtk_test_accessible_assertion_message_role (const char        *domain,
                                            const char        *file,
                                            int                line,
                                            const char        *func,
                                            const char        *expr,
                                            GtkAccessible     *accessible,
                                            GtkAccessibleRole  expected_role,
                                            GtkAccessibleRole  actual_role)
{
  char *role_name = g_enum_to_string (GTK_TYPE_ACCESSIBLE_ROLE, actual_role);
  char *s = g_strdup_printf ("assertion failed: (%s): %s.accessible-role = %s (%d)",
                             expr,
                             G_OBJECT_TYPE_NAME (accessible),
                             role_name,
                             actual_role);

  g_assertion_message (domain, file, line, func, s);

  g_free (role_name);
  g_free (s);
}
