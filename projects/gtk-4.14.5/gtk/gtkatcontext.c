/* gtkatcontext.c: Assistive technology context
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

/**
 * GtkATContext:
 *
 * `GtkATContext` is an abstract class provided by GTK to communicate to
 * platform-specific assistive technologies API.
 *
 * Each platform supported by GTK implements a `GtkATContext` subclass, and
 * is responsible for updating the accessible state in response to state
 * changes in `GtkAccessible`.
 */

#include "config.h"

#include "gtkatcontextprivate.h"

#include "gtkaccessiblevalueprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtktestatcontextprivate.h"
#include "gtktypebuiltins.h"

#include "gtkbutton.h"
#include "gtktogglebutton.h"
#include "gtkmenubutton.h"
#include "gtkdropdown.h"
#include "gtkcolordialogbutton.h"
#include "gtkfontdialogbutton.h"
#include "gtkscalebutton.h"
#include "print/gtkprinteroptionwidgetprivate.h"

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
#include "a11y/gtkatspicontextprivate.h"
#endif

G_DEFINE_ABSTRACT_TYPE (GtkATContext, gtk_at_context, G_TYPE_OBJECT)

enum
{
  PROP_ACCESSIBLE_ROLE = 1,
  PROP_ACCESSIBLE,
  PROP_DISPLAY,

  N_PROPS
};

enum
{
  STATE_CHANGE,

  LAST_SIGNAL
};

static GParamSpec *obj_props[N_PROPS];

static guint obj_signals[LAST_SIGNAL];

static char *gtk_at_context_get_description_internal (GtkATContext*self, gboolean check_duplicates);
static char *gtk_at_context_get_name_internal (GtkATContext*self, gboolean check_duplicates);

static void
gtk_at_context_finalize (GObject *gobject)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  gtk_accessible_attribute_set_unref (self->properties);
  gtk_accessible_attribute_set_unref (self->relations);
  gtk_accessible_attribute_set_unref (self->states);

  G_OBJECT_CLASS (gtk_at_context_parent_class)->finalize (gobject);
}

static void
gtk_at_context_dispose (GObject *gobject)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  gtk_at_context_unrealize (self);

  if (self->accessible_parent != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->accessible_parent),
                                    (gpointer *) &self->accessible_parent);
      self->accessible_parent = NULL;
    }

  if (self->next_accessible_sibling != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->next_accessible_sibling),
                                    (gpointer *) &self->next_accessible_sibling);
      self->next_accessible_sibling = NULL;
    }

  G_OBJECT_CLASS (gtk_at_context_parent_class)->dispose (gobject);
}

static void
gtk_at_context_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      gtk_at_context_set_accessible_role (self, g_value_get_enum (value));
      break;

    case PROP_ACCESSIBLE:
      self->accessible = g_value_get_object (value);
      break;

    case PROP_DISPLAY:
      gtk_at_context_set_display (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_context_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkATContext *self = GTK_AT_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_ACCESSIBLE_ROLE:
      g_value_set_enum (value, self->accessible_role);
      break;

    case PROP_ACCESSIBLE:
      g_value_set_object (value, self->accessible);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_context_real_state_change (GtkATContext                *self,
                                  GtkAccessibleStateChange     changed_states,
                                  GtkAccessiblePropertyChange  changed_properties,
                                  GtkAccessibleRelationChange  changed_relations,
                                  GtkAccessibleAttributeSet   *states,
                                  GtkAccessibleAttributeSet   *properties,
                                  GtkAccessibleAttributeSet   *relations)
{
}

static void
gtk_at_context_real_platform_change (GtkATContext                *self,
                                     GtkAccessiblePlatformChange  change)
{
}

static void
gtk_at_context_real_bounds_change (GtkATContext *self)
{
}

static void
gtk_at_context_real_child_change (GtkATContext             *self,
                                  GtkAccessibleChildChange  change,
                                  GtkAccessible            *child)
{
}

static void
gtk_at_context_real_realize (GtkATContext *self)
{
}

static void
gtk_at_context_real_unrealize (GtkATContext *self)
{
}

static void
gtk_at_context_real_update_caret_position (GtkATContext *self)
{
}

static void
gtk_at_context_real_update_selection_bound (GtkATContext *self)
{
}

static void
gtk_at_context_real_update_text_contents (GtkATContext *self,
                                          GtkAccessibleTextContentChange change,
                                          unsigned int start,
                                          unsigned int end)
{
}

static void
gtk_at_context_class_init (GtkATContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_at_context_set_property;
  gobject_class->get_property = gtk_at_context_get_property;
  gobject_class->dispose = gtk_at_context_dispose;
  gobject_class->finalize = gtk_at_context_finalize;

  klass->realize = gtk_at_context_real_realize;
  klass->unrealize = gtk_at_context_real_unrealize;
  klass->state_change = gtk_at_context_real_state_change;
  klass->platform_change = gtk_at_context_real_platform_change;
  klass->bounds_change = gtk_at_context_real_bounds_change;
  klass->child_change = gtk_at_context_real_child_change;
  klass->update_caret_position = gtk_at_context_real_update_caret_position;
  klass->update_selection_bound = gtk_at_context_real_update_selection_bound;
  klass->update_text_contents = gtk_at_context_real_update_text_contents;

  /**
   * GtkATContext:accessible-role: (attributes org.gtk.Property.get=gtk_at_context_get_accessible_role)
   *
   * The accessible role used by the AT context.
   *
   * Depending on the given role, different states and properties can be
   * set or retrieved.
   */
  obj_props[PROP_ACCESSIBLE_ROLE] =
    g_param_spec_enum ("accessible-role", NULL, NULL,
                       GTK_TYPE_ACCESSIBLE_ROLE,
                       GTK_ACCESSIBLE_ROLE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GtkATContext:accessible: (attributes org.gtk.Property.get=gtk_at_context_get_accessible)
   *
   * The `GtkAccessible` that created the `GtkATContext` instance.
   */
  obj_props[PROP_ACCESSIBLE] =
    g_param_spec_object ("accessible", NULL, NULL,
                         GTK_TYPE_ACCESSIBLE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GtkATContext:display:
   *
   * The `GdkDisplay` for the `GtkATContext`.
   */
  obj_props[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkATContext::state-change:
   * @self: the `GtkATContext`
   *
   * Emitted when the attributes of the accessible for the
   * `GtkATContext` instance change.
   */
  obj_signals[STATE_CHANGE] =
    g_signal_new ("state-change",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

#define N_PROPERTIES    (GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT + 1)
#define N_RELATIONS     (GTK_ACCESSIBLE_RELATION_SET_SIZE + 1)
#define N_STATES        (GTK_ACCESSIBLE_STATE_SELECTED + 1)

static const char *property_attrs[] = {
  [GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE]        = "autocomplete",
  [GTK_ACCESSIBLE_PROPERTY_DESCRIPTION]         = "description",
  [GTK_ACCESSIBLE_PROPERTY_HAS_POPUP]           = "haspopup",
  [GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS]       = "keyshortcuts",
  [GTK_ACCESSIBLE_PROPERTY_LABEL]               = "label",
  [GTK_ACCESSIBLE_PROPERTY_LEVEL]               = "level",
  [GTK_ACCESSIBLE_PROPERTY_MODAL]               = "modal",
  [GTK_ACCESSIBLE_PROPERTY_MULTI_LINE]          = "multiline",
  [GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE]    = "multiselectable",
  [GTK_ACCESSIBLE_PROPERTY_ORIENTATION]         = "orientation",
  [GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER]         = "placeholder",
  [GTK_ACCESSIBLE_PROPERTY_READ_ONLY]           = "readonly",
  [GTK_ACCESSIBLE_PROPERTY_REQUIRED]            = "required",
  [GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION]    = "roledescription",
  [GTK_ACCESSIBLE_PROPERTY_SORT]                = "sort",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MAX]           = "valuemax",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MIN]           = "valuemin",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_NOW]           = "valuenow",
  [GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT]          = "valuetext",
};

/*< private >
 * gtk_accessible_property_get_attribute_name:
 * @property: a `GtkAccessibleProperty`
 *
 * Retrieves the name of a `GtkAccessibleProperty`.
 *
 * Returns: (transfer none): the name of the accessible property
 */
const char *
gtk_accessible_property_get_attribute_name (GtkAccessibleProperty property)
{
  g_return_val_if_fail (property >= GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE &&
                        property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
                        "<none>");

  return property_attrs[property];
}

static const char *relation_attrs[] = {
  [GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT]   = "activedescendant",
  [GTK_ACCESSIBLE_RELATION_COL_COUNT]           = "colcount",
  [GTK_ACCESSIBLE_RELATION_COL_INDEX]           = "colindex",
  [GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT]      = "colindextext",
  [GTK_ACCESSIBLE_RELATION_COL_SPAN]            = "colspan",
  [GTK_ACCESSIBLE_RELATION_CONTROLS]            = "controls",
  [GTK_ACCESSIBLE_RELATION_DESCRIBED_BY]        = "describedby",
  [GTK_ACCESSIBLE_RELATION_DETAILS]             = "details",
  [GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE]       = "errormessage",
  [GTK_ACCESSIBLE_RELATION_FLOW_TO]             = "flowto",
  [GTK_ACCESSIBLE_RELATION_LABELLED_BY]         = "labelledby",
  [GTK_ACCESSIBLE_RELATION_OWNS]                = "owns",
  [GTK_ACCESSIBLE_RELATION_POS_IN_SET]          = "posinset",
  [GTK_ACCESSIBLE_RELATION_ROW_COUNT]           = "rowcount",
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX]           = "rowindex",
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT]      = "rowindextext",
  [GTK_ACCESSIBLE_RELATION_ROW_SPAN]            = "rowspan",
  [GTK_ACCESSIBLE_RELATION_SET_SIZE]            = "setsize",
};

/*< private >
 * gtk_accessible_relation_get_attribute_name:
 * @relation: a `GtkAccessibleRelation`
 *
 * Retrieves the name of a `GtkAccessibleRelation`.
 *
 * Returns: (transfer none): the name of the accessible relation
 */
const char *
gtk_accessible_relation_get_attribute_name (GtkAccessibleRelation relation)
{
  g_return_val_if_fail (relation >= GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT &&
                        relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE,
                        "<none>");

  return relation_attrs[relation];
}

static const char *state_attrs[] = {
  [GTK_ACCESSIBLE_STATE_BUSY]           = "busy",
  [GTK_ACCESSIBLE_STATE_CHECKED]        = "checked",
  [GTK_ACCESSIBLE_STATE_DISABLED]       = "disabled",
  [GTK_ACCESSIBLE_STATE_EXPANDED]       = "expanded",
  [GTK_ACCESSIBLE_STATE_HIDDEN]         = "hidden",
  [GTK_ACCESSIBLE_STATE_INVALID]        = "invalid",
  [GTK_ACCESSIBLE_STATE_PRESSED]        = "pressed",
  [GTK_ACCESSIBLE_STATE_SELECTED]       = "selected",
  [GTK_ACCESSIBLE_STATE_VISITED] = "visited",
};

/*< private >
 * gtk_accessible_state_get_attribute_name:
 * @state: a `GtkAccessibleState`
 *
 * Retrieves the name of a `GtkAccessibleState`.
 *
 * Returns: (transfer none): the name of the accessible state
 */
const char *
gtk_accessible_state_get_attribute_name (GtkAccessibleState state)
{
  g_return_val_if_fail (state >= GTK_ACCESSIBLE_STATE_BUSY &&
                        state <= GTK_ACCESSIBLE_STATE_SELECTED,
                        "<none>");

  return state_attrs[state];
}

static void
gtk_at_context_init (GtkATContext *self)
{
  self->accessible_role = GTK_ACCESSIBLE_ROLE_NONE;

  self->properties =
    gtk_accessible_attribute_set_new (G_N_ELEMENTS (property_attrs),
                                      (GtkAccessibleAttributeNameFunc) gtk_accessible_property_get_attribute_name,
                                      (GtkAccessibleAttributeDefaultFunc) gtk_accessible_value_get_default_for_property);
  self->relations =
    gtk_accessible_attribute_set_new (G_N_ELEMENTS (relation_attrs),
                                      (GtkAccessibleAttributeNameFunc) gtk_accessible_relation_get_attribute_name,
                                      (GtkAccessibleAttributeDefaultFunc) gtk_accessible_value_get_default_for_relation);
  self->states =
    gtk_accessible_attribute_set_new (G_N_ELEMENTS (state_attrs),
                                      (GtkAccessibleAttributeNameFunc) gtk_accessible_state_get_attribute_name,
                                      (GtkAccessibleAttributeDefaultFunc) gtk_accessible_value_get_default_for_state);
}

/**
 * gtk_at_context_get_accessible: (attributes org.gtk.Method.get_property=accessible)
 * @self: a `GtkATContext`
 *
 * Retrieves the `GtkAccessible` using this context.
 *
 * Returns: (transfer none): a `GtkAccessible`
 */
GtkAccessible *
gtk_at_context_get_accessible (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return self->accessible;
}

/*< private >
 * gtk_at_context_set_accessible_role:
 * @self: a `GtkATContext`
 * @role: the accessible role for the context
 *
 * Sets the accessible role for the given `GtkATContext`.
 *
 * This function can only be called if the `GtkATContext` is unrealized.
 */
void
gtk_at_context_set_accessible_role (GtkATContext      *self,
                                    GtkAccessibleRole  role)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));
  g_return_if_fail (!self->realized);

  if (self->accessible_role == role)
    return;

  self->accessible_role = role;

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_ACCESSIBLE_ROLE]);
}

/**
 * gtk_at_context_get_accessible_role: (attributes org.gtk.Method.get_property=accessible-role)
 * @self: a `GtkATContext`
 *
 * Retrieves the accessible role of this context.
 *
 * Returns: a `GtkAccessibleRole`
 */
GtkAccessibleRole
gtk_at_context_get_accessible_role (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), GTK_ACCESSIBLE_ROLE_NONE);

  return self->accessible_role;
}

/*< private >
 * gtk_at_context_get_accessible_parent:
 * @self: a `GtkAtContext`
 *
 * Retrieves the parent accessible object of the given `GtkAtContext`.
 *
 * Returns: (nullable) (transfer none): the parent accessible object, or `NULL` if not set.
 */
GtkAccessible *
gtk_at_context_get_accessible_parent (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return self->accessible_parent;
}


static GtkATContext * get_parent_context (GtkATContext *self);

/*< private >
 * gtk_at_context_set_accessible_parent:
 * @self: a `GtkAtContext`
 * @parent: (nullable): the parent `GtkAccessible` to set
 *
 * Sets the parent accessible object of the given `GtkAtContext`.
 */
void
gtk_at_context_set_accessible_parent (GtkATContext *self,
                                      GtkAccessible *parent)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (self->accessible_parent != parent)
    {
      if (self->accessible_parent != NULL)
        g_object_remove_weak_pointer (G_OBJECT (self->accessible_parent),
                                      (gpointer *) &self->accessible_parent);

      self->accessible_parent = parent;
      if (self->accessible_parent != NULL)
        {
          GtkATContext *parent_context = NULL;

          g_object_add_weak_pointer (G_OBJECT (self->accessible_parent),
                                     (gpointer *) &self->accessible_parent);

          parent_context = get_parent_context (self);
          if (parent_context && parent_context->realized)
            gtk_at_context_realize (self);
          g_clear_object (&parent_context);
        }
    }
}

/*< private >
 * gtk_at_context_get_next_accessible_sibling:
 * @self: a `GtkAtContext`
 *
 * Retrieves the next accessible sibling of the given `GtkAtContext`.
 *
 * Returns: (nullable) (transfer none): the next accessible sibling.
 */
GtkAccessible *
gtk_at_context_get_next_accessible_sibling (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return self->next_accessible_sibling;
}

/*< private >
 * gtk_at_context_set_next_accessible_sibling:
 * @self: a `GtkAtContext`
 * @sibling: (nullable): the next accessible sibling
 *
 * Sets the next accessible sibling object of the given `GtkAtContext`.
 */
void
gtk_at_context_set_next_accessible_sibling (GtkATContext *self,
                                            GtkAccessible *sibling)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (self->next_accessible_sibling != sibling)
    {
      if (self->next_accessible_sibling != NULL)
        g_object_remove_weak_pointer (G_OBJECT (self->next_accessible_sibling),
                                      (gpointer *) &self->next_accessible_sibling);

      self->next_accessible_sibling = sibling;

      if (self->next_accessible_sibling != NULL)
        g_object_add_weak_pointer (G_OBJECT (self->next_accessible_sibling),
                                   (gpointer *) &self->next_accessible_sibling);
    }
}

/*< private >
 * gtk_at_context_set_display:
 * @self: a `GtkATContext`
 * @display: a `GdkDisplay`
 *
 * Sets the `GdkDisplay` used by the `GtkATContext`.
 *
 * This function can only be called if the `GtkATContext` is
 * not realized.
 */
void
gtk_at_context_set_display (GtkATContext *self,
                            GdkDisplay   *display)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  if (self->display == display)
    return;

  if (self->realized)
    return;

  self->display = display;

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_DISPLAY]);
}

/*< private >
 * gtk_at_context_get_display:
 * @self: a `GtkATContext`
 *
 * Retrieves the `GdkDisplay` used to create the context.
 *
 * Returns: (transfer none): a `GdkDisplay`
 */
GdkDisplay *
gtk_at_context_get_display (GtkATContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return self->display;
}

static const struct {
  const char *name;
  const char *env_name;
  GtkATContext * (* create_context) (GtkAccessibleRole accessible_role,
                                     GtkAccessible    *accessible,
                                     GdkDisplay       *display);
} a11y_backends[] = {
#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  { "AT-SPI", "atspi", gtk_at_spi_create_context },
#endif
  { "Test", "test", gtk_test_at_context_new },
};

/**
 * gtk_at_context_create: (constructor)
 * @accessible_role: the accessible role used by the `GtkATContext`
 * @accessible: the `GtkAccessible` implementation using the `GtkATContext`
 * @display: the `GdkDisplay` used by the `GtkATContext`
 *
 * Creates a new `GtkATContext` instance for the given accessible role,
 * accessible instance, and display connection.
 *
 * The `GtkATContext` implementation being instantiated will depend on the
 * platform.
 *
 * Returns: (nullable) (transfer full): the `GtkATContext`
 */
GtkATContext *
gtk_at_context_create (GtkAccessibleRole  accessible_role,
                       GtkAccessible     *accessible,
                       GdkDisplay        *display)
{
  static const char *gtk_a11y_env;
  GtkATContext *res = NULL;

  if (gtk_a11y_env == NULL)
    {
      gtk_a11y_env = g_getenv ("GTK_A11Y");
      if (gtk_a11y_env == NULL)
        gtk_a11y_env = "0";

      if (g_ascii_strcasecmp (gtk_a11y_env, "help") == 0)
        {
          g_print ("Supported arguments for GTK_A11Y environment variable:\n");

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
          g_print ("   atspi - Use the AT-SPI accessibility backend\n");
#endif
          g_print ("    test - Use the test accessibility backend\n");
          g_print ("    none - Disable the accessibility backend\n");
          g_print ("    help - Print this help\n\n");
          g_print ("Other arguments will cause a warning and be ignored.\n");

          gtk_a11y_env = "0";
        }
    }

  /* Short-circuit disabling the accessibility support */
  if (g_ascii_strcasecmp (gtk_a11y_env, "none") == 0)
    return NULL;

  for (size_t i = 0; i < G_N_ELEMENTS (a11y_backends); i++)
    {
      g_assert (a11y_backends[i].name != NULL);

      if (a11y_backends[i].create_context != NULL &&
          (*gtk_a11y_env == '0' || g_ascii_strcasecmp (a11y_backends[i].env_name, gtk_a11y_env) == 0))
        {
          res = a11y_backends[i].create_context (accessible_role, accessible, display);
          if (res != NULL)
            break;
        }
    }

  if (*gtk_a11y_env != '0' && res == NULL)
    g_warning ("Unrecognized accessibility backend \"%s\". Try GTK_A11Y=help", gtk_a11y_env);

  /* Fall back to the test context, so we can get debugging data */
  if (res == NULL)
    res = g_object_new (GTK_TYPE_TEST_AT_CONTEXT,
                        "accessible_role", accessible_role,
                        "accessible", accessible,
                        "display", display,
                        NULL);

  return res;
}

/*< private >
 * gtk_at_context_clone: (constructor)
 * @self: the `GtkATContext` to clone
 * @role: the accessible role of the clone, or %GTK_ACCESSIBLE_ROLE_NONE to
 *   use the same accessible role of @self
 * @accessible: (nullable): the accessible creating the context, or %NULL to
 *   use the same `GtkAccessible` of @self
 * @display: (nullable): the display connection, or %NULL to use the same
 *   `GdkDisplay` of @self
 *
 * Clones the state of the given `GtkATContext`, using @role, @accessible,
 * and @display.
 *
 * If @self is realized, the returned `GtkATContext` will also be realized.
 *
 * Returns: (transfer full): the newly created `GtkATContext`
 */
GtkATContext *
gtk_at_context_clone (GtkATContext      *self,
                      GtkAccessibleRole  role,
                      GtkAccessible     *accessible,
                      GdkDisplay        *display)
{
  g_return_val_if_fail (self == NULL || GTK_IS_AT_CONTEXT (self), NULL);
  g_return_val_if_fail (accessible == NULL || GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (display == NULL || GDK_IS_DISPLAY (display), NULL);

  if (self != NULL && role == GTK_ACCESSIBLE_ROLE_NONE)
    role = self->accessible_role;

  if (self != NULL && accessible == NULL)
    accessible = self->accessible;

  if (self != NULL && display == NULL)
    display = self->display;

  GtkATContext *res = gtk_at_context_create (role, accessible, display);

  if (self != NULL)
    {
      g_clear_pointer (&res->states, gtk_accessible_attribute_set_unref);
      g_clear_pointer (&res->properties, gtk_accessible_attribute_set_unref);
      g_clear_pointer (&res->relations, gtk_accessible_attribute_set_unref);

      res->states = gtk_accessible_attribute_set_ref (self->states);
      res->properties = gtk_accessible_attribute_set_ref (self->properties);
      res->relations = gtk_accessible_attribute_set_ref (self->relations);

      if (self->realized)
        gtk_at_context_realize (res);
    }

  return res;
}

gboolean
gtk_at_context_is_realized (GtkATContext *self)
{
  return self->realized;
}

void
gtk_at_context_realize (GtkATContext *self)
{
  if (self->realized)
    return;

  GTK_DEBUG (A11Y, "Realizing AT context '%s'", G_OBJECT_TYPE_NAME (self));
  GTK_AT_CONTEXT_GET_CLASS (self)->realize (self);

  self->realized = TRUE;
}

void
gtk_at_context_unrealize (GtkATContext *self)
{
  if (!self->realized)
    return;

  GTK_DEBUG (A11Y, "Unrealizing AT context '%s'", G_OBJECT_TYPE_NAME (self));
  GTK_AT_CONTEXT_GET_CLASS (self)->unrealize (self);

  self->realized = FALSE;
}

/*< private >
 * gtk_at_context_update:
 * @self: a `GtkATContext`
 *
 * Notifies the AT connected to this `GtkATContext` that the accessible
 * state and its properties have changed.
 */
void
gtk_at_context_update (GtkATContext *self)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  if (!self->realized)
    return;

  /* There's no point in notifying of state changes if there weren't any */
  if (self->updated_properties == 0 &&
      self->updated_relations == 0 &&
      self->updated_states == 0)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->state_change (self,
                                                 self->updated_states, self->updated_properties, self->updated_relations,
                                                 self->states, self->properties, self->relations);
  g_signal_emit (self, obj_signals[STATE_CHANGE], 0);

  self->updated_properties = 0;
  self->updated_relations = 0;
  self->updated_states = 0;
}

/*< private >
 * gtk_at_context_set_accessible_state:
 * @self: a `GtkATContext`
 * @state: a `GtkAccessibleState`
 * @value: (nullable): `GtkAccessibleValue`
 *
 * Sets the @value for the given @state of a `GtkATContext`.
 *
 * If @value is %NULL, the state is unset.
 *
 * This function will accumulate state changes until gtk_at_context_update()
 * is called.
 */
void
gtk_at_context_set_accessible_state (GtkATContext       *self,
                                     GtkAccessibleState  state,
                                     GtkAccessibleValue *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  gboolean res = FALSE;

  if (value != NULL)
    res = gtk_accessible_attribute_set_add (self->states, state, value);
  else
    res = gtk_accessible_attribute_set_remove (self->states, state);

  if (res)
    self->updated_states |= (1 << state);
}

/*< private >
 * gtk_at_context_has_accessible_state:
 * @self: a `GtkATContext`
 * @state: a `GtkAccessibleState`
 *
 * Checks whether a `GtkATContext` has the given @state set.
 *
 * Returns: %TRUE, if the accessible state is set
 */
gboolean
gtk_at_context_has_accessible_state (GtkATContext       *self,
                                     GtkAccessibleState  state)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), FALSE);

  return gtk_accessible_attribute_set_contains (self->states, state);
}

/*< private >
 * gtk_at_context_get_accessible_state:
 * @self: a `GtkATContext`
 * @state: a `GtkAccessibleState`
 *
 * Retrieves the value for the accessible state of a `GtkATContext`.
 *
 * Returns: (transfer none): the value for the given state
 */
GtkAccessibleValue *
gtk_at_context_get_accessible_state (GtkATContext       *self,
                                     GtkAccessibleState  state)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return gtk_accessible_attribute_set_get_value (self->states, state);
}

/*< private >
 * gtk_at_context_set_accessible_property:
 * @self: a `GtkATContext`
 * @property: a `GtkAccessibleProperty`
 * @value: (nullable): `GtkAccessibleValue`
 *
 * Sets the @value for the given @property of a `GtkATContext`.
 *
 * If @value is %NULL, the property is unset.
 *
 * This function will accumulate property changes until gtk_at_context_update()
 * is called.
 */
void
gtk_at_context_set_accessible_property (GtkATContext          *self,
                                        GtkAccessibleProperty  property,
                                        GtkAccessibleValue    *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  gboolean res = FALSE;

  if (value != NULL)
    res = gtk_accessible_attribute_set_add (self->properties, property, value);
  else
    res = gtk_accessible_attribute_set_remove (self->properties, property);

  if (res && self->realized)
    self->updated_properties |= (1 << property);
}

/*< private >
 * gtk_at_context_has_accessible_property:
 * @self: a `GtkATContext`
 * @property: a `GtkAccessibleProperty`
 *
 * Checks whether a `GtkATContext` has the given @property set.
 *
 * Returns: %TRUE, if the accessible property is set
 */
gboolean
gtk_at_context_has_accessible_property (GtkATContext          *self,
                                        GtkAccessibleProperty  property)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), FALSE);

  return gtk_accessible_attribute_set_contains (self->properties, property);
}

/*< private >
 * gtk_at_context_get_accessible_property:
 * @self: a `GtkATContext`
 * @property: a `GtkAccessibleProperty`
 *
 * Retrieves the value for the accessible property of a `GtkATContext`.
 *
 * Returns: (transfer none): the value for the given property
 */
GtkAccessibleValue *
gtk_at_context_get_accessible_property (GtkATContext          *self,
                                        GtkAccessibleProperty  property)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return gtk_accessible_attribute_set_get_value (self->properties, property);
}

/*< private >
 * gtk_at_context_set_accessible_relation:
 * @self: a `GtkATContext`
 * @relation: a `GtkAccessibleRelation`
 * @value: (nullable): `GtkAccessibleValue`
 *
 * Sets the @value for the given @relation of a `GtkATContext`.
 *
 * If @value is %NULL, the relation is unset.
 *
 * This function will accumulate relation changes until gtk_at_context_update()
 * is called.
 */
void
gtk_at_context_set_accessible_relation (GtkATContext          *self,
                                        GtkAccessibleRelation  relation,
                                        GtkAccessibleValue    *value)
{
  g_return_if_fail (GTK_IS_AT_CONTEXT (self));

  gboolean res = FALSE;

  if (value != NULL)
    res = gtk_accessible_attribute_set_add (self->relations, relation, value);
  else
    res = gtk_accessible_attribute_set_remove (self->relations, relation);

  if (res)
    self->updated_relations |= (1 << relation);
}

/*< private >
 * gtk_at_context_has_accessible_relation:
 * @self: a `GtkATContext`
 * @relation: a `GtkAccessibleRelation`
 *
 * Checks whether a `GtkATContext` has the given @relation set.
 *
 * Returns: %TRUE, if the accessible relation is set
 */
gboolean
gtk_at_context_has_accessible_relation (GtkATContext          *self,
                                        GtkAccessibleRelation  relation)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), FALSE);

  return gtk_accessible_attribute_set_contains (self->relations, relation);
}

/*< private >
 * gtk_at_context_get_accessible_relation:
 * @self: a `GtkATContext`
 * @relation: a `GtkAccessibleRelation`
 *
 * Retrieves the value for the accessible relation of a `GtkATContext`.
 *
 * Returns: (transfer none): the value for the given relation
 */
GtkAccessibleValue *
gtk_at_context_get_accessible_relation (GtkATContext          *self,
                                        GtkAccessibleRelation  relation)
{
  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  return gtk_accessible_attribute_set_get_value (self->relations, relation);
}

/* See ARIA 5.2.8.4, 5.2.8.5 and 5.2.8.6 for the prohibited, from author
 * and from content parts, and the table in
 * https://www.w3.org/WAI/ARIA/apg/practices/names-and-descriptions/
 * for the recommended / not recommended parts. We've made a few changes
 * to the recommendations:
 * - We don't recommend against labelling listitems, sincd GtkListView
 *   will put the focus on listitems sometimes.
 * - We don't recommend tab lists being labelled, since GtkNotebook does
 *   not have a practical way of doing that.
 */

#define NAME_FROM_AUTHOR  (1 << 6)
#define NAME_FROM_CONTENT (1 << 7)

static guint8 naming[] = {
  [GTK_ACCESSIBLE_ROLE_ALERT] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_ALERT_DIALOG] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_APPLICATION] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_ARTICLE] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_BANNER] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_BUTTON] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_CAPTION] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_CELL] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_CHECKBOX] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_COLUMN_HEADER] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_COMBO_BOX] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_COMMAND] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_COMMENT] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_COMPOSITE] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_DIALOG] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_DOCUMENT] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_FEED] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_FORM] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_GENERIC] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_GRID] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_GRID_CELL] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_GROUP] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_HEADING] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_IMG] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_INPUT] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_LABEL] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_LANDMARK] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_LEGEND] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_LINK] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_LIST] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_LIST_BOX] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_LIST_ITEM] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_LOG] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_MAIN] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_MARQUEE] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_MATH] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_METER] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_MENU] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_MENU_BAR] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_NAVIGATION] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_NONE] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_NOTE] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_OPTION] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_PARAGRAPH] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_PRESENTATION] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_PROGRESS_BAR] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_RADIO] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_RADIO_GROUP] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_RANGE] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_REGION] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_ROW] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_ROW_GROUP] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_NOT_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_ROW_HEADER] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_SCROLLBAR] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_SEARCH] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_SEARCH_BOX] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_SECTION] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_SECTION_HEAD] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_SELECT] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_SEPARATOR] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_SLIDER] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_SPIN_BUTTON] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_STATUS] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_STRUCTURE] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_SWITCH] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TAB] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TABLE] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TAB_LIST] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_TAB_PANEL] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TERMINAL] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TEXT_BOX] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TIME] = GTK_ACCESSIBLE_NAME_PROHIBITED,
  [GTK_ACCESSIBLE_ROLE_TIMER] = NAME_FROM_AUTHOR,
  [GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TOOLBAR] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_RECOMMENDED,
  [GTK_ACCESSIBLE_ROLE_TOOLTIP] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_TREE] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TREE_GRID] = NAME_FROM_AUTHOR|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_TREE_ITEM] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT|GTK_ACCESSIBLE_NAME_REQUIRED,
  [GTK_ACCESSIBLE_ROLE_WIDGET] = NAME_FROM_AUTHOR|NAME_FROM_CONTENT,
  [GTK_ACCESSIBLE_ROLE_WINDOW] = NAME_FROM_AUTHOR,
};

/* < private >
 * gtk_accessible_role_supports_name_from_author:
 * @role: a `GtkAccessibleRole`
 *
 * Returns whether this role supports setting the label and description
 * properties or the labelled-by and described-by relations.
 *
 * Returns: %TRUE if the role allows labelling
 */
gboolean
gtk_accessible_role_supports_name_from_author (GtkAccessibleRole role)
{
  return (naming[role] & NAME_FROM_AUTHOR) != 0;
}

/* < private >
 * gtk_accessible_role_supports_name_from_content:
 * @role: a `GtkAccessibleRole`
 *
 * Returns whether this role will use content of child widgets such
 * as labels for its accessible name and description if no explicit
 * labels are provided.
 *
 * Returns: %TRUE if the role content naming
 */
gboolean
gtk_accessible_role_supports_name_from_content (GtkAccessibleRole role)
{
  return (naming[role] & NAME_FROM_CONTENT) != 0;
}

/* < private >
 * gtk_accessible_role_get_nameing:
 * @role: a `GtkAccessibleRole`
 *
 * Returns naming information for this role.
 *
 * Returns: information about naming requirements for the role
 */
GtkAccessibleNaming
gtk_accessible_role_get_naming (GtkAccessibleRole role)
{
  return (GtkAccessibleNaming) (naming[role] & ~(NAME_FROM_AUTHOR|NAME_FROM_CONTENT));
}

static gboolean
is_nested_button (GtkATContext *self)
{
  GtkAccessible *accessible;
  GtkWidget *widget, *parent;

  accessible = gtk_at_context_get_accessible (self);

  if (!GTK_IS_WIDGET (accessible))
    return FALSE;

  widget = GTK_WIDGET (accessible);
  parent = gtk_widget_get_parent (widget);

  if ((GTK_IS_TOGGLE_BUTTON (widget) && GTK_IS_DROP_DOWN (parent)) ||
      (GTK_IS_TOGGLE_BUTTON (widget) && GTK_IS_MENU_BUTTON (parent)) ||
      (GTK_IS_BUTTON (widget) && GTK_IS_COLOR_DIALOG_BUTTON (parent)) ||
      (GTK_IS_BUTTON (widget) && GTK_IS_FONT_DIALOG_BUTTON (parent)) ||
      (GTK_IS_BUTTON (widget) && GTK_IS_SCALE_BUTTON (parent))
#ifdef G_OS_UNIX
      || (GTK_IS_PRINTER_OPTION_WIDGET (parent) &&
          (GTK_IS_CHECK_BUTTON (widget) ||
           GTK_IS_DROP_DOWN (widget) ||
           GTK_IS_ENTRY (widget) ||
           GTK_IS_IMAGE (widget) ||
           GTK_IS_LABEL (widget) ||
           GTK_IS_BUTTON (widget)))
#endif
      )
    return TRUE;

  return FALSE;
}

static GtkATContext *
get_parent_context (GtkATContext *self)
{
  GtkAccessible *accessible, *parent;

  accessible = gtk_at_context_get_accessible (self);
  parent = gtk_accessible_get_accessible_parent (accessible);
  if (parent)
    {
      GtkATContext *context = gtk_accessible_get_at_context (parent);
      g_object_unref (parent);
      return context;
    }

  return g_object_ref (self);
}

static inline gboolean
not_just_space (const char *text)
{
  for (const char *p = text; *p; p = g_utf8_next_char (p))
    {
      if (!g_unichar_isspace (g_utf8_get_char (p)))
        return TRUE;
    }

  return FALSE;
}

static inline void
append_with_space (GString    *str,
                   const char *text)
{
  if (str->len > 0)
    g_string_append (str, " ");
  g_string_append (str, text);
}

/* See the WAI-ARIA § 4.3, "Accessible Name and Description Computation",
 * and https://www.w3.org/TR/accname-1.2/
 */

static void
gtk_at_context_get_text_accumulate (GtkATContext          *self,
                                    GPtrArray             *nodes,
                                    GString               *res,
                                    GtkAccessibleProperty  property,
                                    GtkAccessibleRelation  relation,
                                    gboolean               is_ref,
                                    gboolean               is_child,
                                    gboolean               check_duplicates)
{
  GtkAccessibleValue *value = NULL;

  /* Step 2.A */
  if (!is_ref)
    {
      if (gtk_accessible_attribute_set_contains (self->states, GTK_ACCESSIBLE_STATE_HIDDEN))
        {
          value = gtk_accessible_attribute_set_get_value (self->states, GTK_ACCESSIBLE_STATE_HIDDEN);

          if (gtk_boolean_accessible_value_get (value))
            return;
        }
    }

  if (gtk_accessible_role_supports_name_from_author (self->accessible_role))
    {
      /* Step 2.B */
      if (!is_ref && gtk_accessible_attribute_set_contains (self->relations, relation))
        {
          value = gtk_accessible_attribute_set_get_value (self->relations, relation);

          GList *list = gtk_reference_list_accessible_value_get (value);

          for (GList *l = list; l != NULL; l = l->next)
            {
              GtkAccessible *rel = GTK_ACCESSIBLE (l->data);
              if (!g_ptr_array_find (nodes, rel, NULL))
                {
                  GtkATContext *rel_context = gtk_accessible_get_at_context (rel);

                  g_ptr_array_add (nodes, rel);
                  gtk_at_context_get_text_accumulate (rel_context, nodes, res, property, relation, TRUE, FALSE, check_duplicates);

                  g_object_unref (rel_context);
                }
            }

          return;
        }

      /* Step 2.C */
      if (gtk_accessible_attribute_set_contains (self->properties, property))
        {
          value = gtk_accessible_attribute_set_get_value (self->properties, property);

          char *str = (char *) gtk_string_accessible_value_get (value);
          if (str[0] != '\0')
            {
              append_with_space (res, gtk_string_accessible_value_get (value));
              return;
            }
        }
    }

  /* Step 2.E */
  if ((property == GTK_ACCESSIBLE_PROPERTY_LABEL && is_child) || (relation == GTK_ACCESSIBLE_RELATION_LABELLED_BY && is_ref))
    {
      if (self->accessible_role == GTK_ACCESSIBLE_ROLE_TEXT_BOX)
        {
          if (GTK_IS_EDITABLE (self->accessible))
            {
              const char *text = gtk_editable_get_text (GTK_EDITABLE (self->accessible));
            if (text && not_just_space (text))
              append_with_space (res, text);
          }
        return;
      }
      else if (gtk_accessible_role_is_range_subclass (self->accessible_role))
        {
          if (gtk_accessible_attribute_set_contains (self->properties, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT))
            {
              value = gtk_accessible_attribute_set_get_value (self->properties, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT);
              append_with_space (res, gtk_string_accessible_value_get (value));
            }
          else if (gtk_accessible_attribute_set_contains (self->properties, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW))
            {
              value = gtk_accessible_attribute_set_get_value (self->properties, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW);
              if (res->len > 0)
                g_string_append (res, " ");
              g_string_append_printf (res, "%g", gtk_number_accessible_value_get (value));
            }

          return;
        }
      }

  /* Step 2.F */
  if (gtk_accessible_role_supports_name_from_content (self->accessible_role) || is_ref || is_child)
    {
      if (GTK_IS_WIDGET (self->accessible))
        {
          GString *s = g_string_new ("");

          for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->accessible));
               child != NULL;
               child = gtk_widget_get_next_sibling (child))
            {
              GtkAccessible *rel = GTK_ACCESSIBLE (child);
              GtkATContext *rel_context = gtk_accessible_get_at_context (rel);

              gtk_at_context_get_text_accumulate (rel_context, nodes, s, property, relation, FALSE, TRUE, check_duplicates);

              g_object_unref (rel_context);
            }

           if (s->len > 0)
             {
               append_with_space (res, s->str);
               g_string_free (s, TRUE);
               return;
             }

           g_string_free (s, TRUE);
        }
    }

  /* Step 2.I */
  if (GTK_IS_WIDGET (self->accessible))
    {
      const char *text = gtk_widget_get_tooltip_text (GTK_WIDGET (self->accessible));
      if (text && not_just_space (text))
        {
          gboolean append = !check_duplicates;

          if (!append)
            {
              char *description = gtk_at_context_get_description_internal (self, FALSE);
              char *name = gtk_at_context_get_name_internal (self, FALSE);

              append =
                (property == GTK_ACCESSIBLE_PROPERTY_LABEL && strcmp (text, description) != 0) ||
                (property == GTK_ACCESSIBLE_PROPERTY_DESCRIPTION && strcmp (text, name) != 0);

              g_free (description);
              g_free (name);
            }

          if (append)
            append_with_space (res, text);
        }
    }
}

static char *
gtk_at_context_get_text (GtkATContext          *self,
                         GtkAccessibleProperty  property,
                         GtkAccessibleRelation  relation,
gboolean              check_duplicates)
{
  GtkATContext *parent = NULL;

  g_return_val_if_fail (GTK_IS_AT_CONTEXT (self), NULL);

  /* Step 1 */
  if (gtk_accessible_role_get_naming (self->accessible_role) == GTK_ACCESSIBLE_NAME_PROHIBITED)
    return g_strdup ("");

  /* We special case this here since it is a common pattern:
   * We have a 'wrapper' object, like a GtkDropdown which
   * contains a toggle button. The dropdown appears in the
   * ui file and carries all the a11y attributes, but the
   * focus ends up on the toggle button.
   */
  if (is_nested_button (self))
    {
      parent = get_parent_context (self);
      self = parent;
      if (is_nested_button (self))
        {
          parent = get_parent_context (parent);
          g_object_unref (self);
          self = parent;
        }
    }

  GPtrArray *nodes = g_ptr_array_new ();
  GString *res = g_string_new ("");

  /* Step 2 */
  gtk_at_context_get_text_accumulate (self, nodes, res, property, relation, FALSE, FALSE, check_duplicates);

  g_ptr_array_unref (nodes);

  g_clear_object (&parent);

  return g_string_free (res, FALSE);
}

static char *
gtk_at_context_get_name_internal (GtkATContext *self, gboolean check_duplicates)
{
  return gtk_at_context_get_text (self, GTK_ACCESSIBLE_PROPERTY_LABEL, GTK_ACCESSIBLE_RELATION_LABELLED_BY, check_duplicates);
}

/*< private >
 * gtk_at_context_get_name:
 * @self: a `GtkATContext`
 *
 * Retrieves the accessible name of the `GtkATContext`.
 *
 * This is a convenience function meant to be used by `GtkATContext` implementations.
 *
 * Returns: (transfer full): the label of the `GtkATContext`
 */
char *
gtk_at_context_get_name (GtkATContext *self)
{
  /*
  * We intentionally don't check for duplicates here, as the name
  * is more important, and we want the tooltip as the name
  * if everything else fails.
  */
  return gtk_at_context_get_name_internal (self, FALSE);
}

static char *
gtk_at_context_get_description_internal (GtkATContext *self, gboolean check_duplicates)
{
  return gtk_at_context_get_text (self, GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, check_duplicates);
}

/*< private >
 * gtk_at_context_get_description:
 * @self: a `GtkATContext`
 *
 * Retrieves the accessible description of the `GtkATContext`.
 *
 * This is a convenience function meant to be used by `GtkATContext` implementations.
 *
 * Returns: (transfer full): the label of the `GtkATContext`
 */
char *
gtk_at_context_get_description (GtkATContext *self)
{
  return gtk_at_context_get_description_internal (self, TRUE);
}

void
gtk_at_context_platform_changed (GtkATContext                *self,
                                 GtkAccessiblePlatformChange  change)
{
  gtk_at_context_realize (self);

  GTK_AT_CONTEXT_GET_CLASS (self)->platform_change (self, change);
}

void
gtk_at_context_bounds_changed (GtkATContext *self)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->bounds_change (self);
}

void
gtk_at_context_child_changed (GtkATContext             *self,
                              GtkAccessibleChildChange  change,
                              GtkAccessible            *child)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->child_change (self, change, child);
}

void
gtk_at_context_announce (GtkATContext                      *self,
                         const char                        *message,
                         GtkAccessibleAnnouncementPriority  priority)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->announce (self, message, priority);
}

void
gtk_at_context_update_caret_position (GtkATContext *self)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->update_caret_position (self);
}

void
gtk_at_context_update_selection_bound (GtkATContext *self)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->update_selection_bound (self);
}

void
gtk_at_context_update_text_contents (GtkATContext *self,
                                     GtkAccessibleTextContentChange change,
                                     unsigned int start,
                                     unsigned int end)
{
  if (!self->realized)
    return;

  GTK_AT_CONTEXT_GET_CLASS (self)->update_text_contents (self, change, start, end);
}
