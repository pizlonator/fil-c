/* gtkatspicontext.c: AT-SPI GtkATContext implementation
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

#include "gtkatspicontextprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkaccessibletextprivate.h"

#include "gtkatspiactionprivate.h"
#include "gtkatspieditabletextprivate.h"
#include "gtkatspiprivate.h"
#include "gtkatspirootprivate.h"
#include "gtkatspiselectionprivate.h"
#include "gtkatspisocketprivate.h"
#include "gtkatspitextprivate.h"
#include "gtkatspiutilsprivate.h"
#include "gtkatspivalueprivate.h"
#include "gtkatspicomponentprivate.h"
#include "a11y/atspi/atspi-accessible.h"
#include "a11y/atspi/atspi-action.h"
#include "a11y/atspi/atspi-editabletext.h"
#include "a11y/atspi/atspi-text.h"
#include "a11y/atspi/atspi-value.h"
#include "a11y/atspi/atspi-selection.h"
#include "a11y/atspi/atspi-component.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkeditable.h"
#include "gtkentryprivate.h"
#include "gtkroot.h"
#include "gtkstack.h"
#include "gtktextview.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"

#include <gio/gio.h>

#include <locale.h>

#if defined(GDK_WINDOWING_WAYLAND)
# include <gdk/wayland/gdkwaylanddisplay.h>
#endif
#if defined(GDK_WINDOWING_X11)
# include <gdk/x11/gdkx11display.h>
# include <gdk/x11/gdkx11property.h>
#endif

/* We create a GtkAtSpiContext object for (almost) every widget.
 *
 * Each context implements a number of Atspi interfaces on a D-Bus
 * object. The context objects are connected into a tree by the
 * Parent property and GetChildAtIndex method of the Accessible
 * interface.
 *
 * The tree is an almost perfect mirror image of the widget tree,
 * with a few notable exceptions:
 *
 * - We don't create contexts for the GtkText widgets inside
 *   entry wrappers, since the Text functionality is represented
 *   on the entry contexts.
 *
 * - We insert non-widget backed context objects for each page
 *   of a stack. The main purpose of these extra context is to
 *   hold the TAB_PANEL role and be the target of the CONTROLS
 *   relation with their corresponding tabs (in the stack
 *   switcher or notebook).
 *
 * These are the exceptions implemented by GTK itself, but note that application
 * developers can customize the accessibility tree by implementing the
 * [iface@Gtk.Accessible] interface in any way they choose.
 */

struct _GtkAtSpiContext
{
  GtkATContext parent_instance;

  /* The root object, used as a entry point */
  GtkAtSpiRoot *root;

  /* The object path of the ATContext on the bus */
  char *context_path;

  /* Just a pointer; the connection is owned by the GtkAtSpiRoot
   * associated to the GtkATContext
   */
  GDBusConnection *connection;

  /* Accerciser refuses to work unless we implement a GetInterface
   * call that returns a list of all implemented interfaces. We
   * collect the answer here.
   */
  GVariant *interfaces;

  guint registration_ids[20];
  guint n_registered_objects;
};

G_DEFINE_TYPE (GtkAtSpiContext, gtk_at_spi_context, GTK_TYPE_AT_CONTEXT)

/* {{{ State handling */
static void
set_atspi_state (guint64        *states,
                 AtspiStateType  state)
{
  *states |= (G_GUINT64_CONSTANT (1) << state);
}

static void
unset_atspi_state (guint64        *states,
                   AtspiStateType  state)
{
  *states &= ~(G_GUINT64_CONSTANT (1) << state);
}

static void
collect_states (GtkAtSpiContext    *self,
                GVariantBuilder *builder)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  GtkAccessibleValue *value;
  GtkAccessible *accessible;
  guint64 states = 0;

  accessible = gtk_at_context_get_accessible (ctx);

  set_atspi_state (&states, ATSPI_STATE_VISIBLE);
  set_atspi_state (&states, ATSPI_STATE_SHOWING);

  if (ctx->accessible_role == GTK_ACCESSIBLE_ROLE_APPLICATION)
    {
      if (gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE))
        set_atspi_state (&states, ATSPI_STATE_ACTIVE);
    }

  if (ctx->accessible_role == GTK_ACCESSIBLE_ROLE_TEXT_BOX ||
      ctx->accessible_role == GTK_ACCESSIBLE_ROLE_SEARCH_BOX ||
      ctx->accessible_role == GTK_ACCESSIBLE_ROLE_SPIN_BUTTON)
    set_atspi_state (&states, ATSPI_STATE_EDITABLE);

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_READ_ONLY))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_READ_ONLY);
      if (gtk_boolean_accessible_value_get (value))
        {
          set_atspi_state (&states, ATSPI_STATE_READ_ONLY);
          unset_atspi_state (&states, ATSPI_STATE_EDITABLE);
        }
    }

  if (gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE))
    set_atspi_state (&states, ATSPI_STATE_FOCUSABLE);

  if (gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED))
    set_atspi_state (&states, ATSPI_STATE_FOCUSED);

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_ORIENTATION))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_ORIENTATION);
      if (gtk_orientation_accessible_value_get (value) == GTK_ORIENTATION_HORIZONTAL)
        set_atspi_state (&states, ATSPI_STATE_HORIZONTAL);
      else
        set_atspi_state (&states, ATSPI_STATE_VERTICAL);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MODAL))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MODAL);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_MODAL);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MULTI_LINE))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MULTI_LINE);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_MULTI_LINE);
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_BUSY);
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED))
    {
      set_atspi_state (&states, ATSPI_STATE_CHECKABLE);
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          set_atspi_state (&states, ATSPI_STATE_CHECKED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          set_atspi_state (&states, ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED);
      if (!gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_SENSITIVE);
    }
  else
    set_atspi_state (&states, ATSPI_STATE_SENSITIVE);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          set_atspi_state (&states, ATSPI_STATE_EXPANDABLE);
          if (gtk_boolean_accessible_value_get (value))
            set_atspi_state (&states, ATSPI_STATE_EXPANDED);
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID);
      switch (gtk_invalid_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_INVALID_TRUE:
        case GTK_ACCESSIBLE_INVALID_GRAMMAR:
        case GTK_ACCESSIBLE_INVALID_SPELLING:
          set_atspi_state (&states, ATSPI_STATE_INVALID_ENTRY);
          break;
        case GTK_ACCESSIBLE_INVALID_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          set_atspi_state (&states, ATSPI_STATE_PRESSED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          set_atspi_state (&states, ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          set_atspi_state (&states, ATSPI_STATE_SELECTABLE);
          if (gtk_boolean_accessible_value_get (value))
            set_atspi_state (&states, ATSPI_STATE_SELECTED);
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_VISITED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_VISITED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          if (gtk_boolean_accessible_value_get (value))
            set_atspi_state (&states, ATSPI_STATE_VISITED);
        }
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_REQUIRED))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_REQUIRED);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_REQUIRED);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_MULTISELECTABLE);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_HAS_POPUP))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_HAS_POPUP);
      if (gtk_boolean_accessible_value_get (value))
        set_atspi_state (&states, ATSPI_STATE_HAS_POPUP);
    }

  if (gtk_at_context_has_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE))
    {
      value = gtk_at_context_get_accessible_property (ctx, GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE);
      if (gtk_autocomplete_accessible_value_get (value) != GTK_ACCESSIBLE_AUTOCOMPLETE_NONE)
        set_atspi_state (&states, ATSPI_STATE_SUPPORTS_AUTOCOMPLETION);
    }

  g_variant_builder_add (builder, "u", (guint32) (states & 0xffffffff));
  g_variant_builder_add (builder, "u", (guint32) (states >> 32));
}
/* }}} */
/* {{{ Relation handling */
static void
collect_relations (GtkAtSpiContext *self,
                   GVariantBuilder *builder)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  struct {
    GtkAccessibleRelation r;
    AtspiRelationType s;
  } map[] = {
    { GTK_ACCESSIBLE_RELATION_LABELLED_BY, ATSPI_RELATION_LABELLED_BY },
    { GTK_ACCESSIBLE_RELATION_CONTROLS, ATSPI_RELATION_CONTROLLER_FOR },
    { GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, ATSPI_RELATION_DESCRIBED_BY },
    { GTK_ACCESSIBLE_RELATION_DETAILS, ATSPI_RELATION_DETAILS },
    { GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE, ATSPI_RELATION_ERROR_MESSAGE},
    { GTK_ACCESSIBLE_RELATION_FLOW_TO, ATSPI_RELATION_FLOWS_TO},
  };
  GtkAccessibleValue *value;
  GList *list, *l;
  int i;

  for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
      if (!gtk_at_context_has_accessible_relation (ctx, map[i].r))
        continue;

      GVariantBuilder b = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      value = gtk_at_context_get_accessible_relation (ctx, map[i].r);
      list = gtk_reference_list_accessible_value_get (value);

      for (l = list; l; l = l->next)
        {
          GtkATContext *target_ctx =
            gtk_accessible_get_at_context (GTK_ACCESSIBLE (l->data));

          /* Realize the ATContext of the target, so we can ask for its ref */
          gtk_at_context_realize (target_ctx);

          g_variant_builder_add (&b, "@(so)",
                                 gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (target_ctx)));

          g_object_unref (target_ctx);
        }

      g_variant_builder_add (builder, "(ua(so))", map[i].s, &b);
    }
}
/* }}} */
/* {{{ Accessible implementation */
static int
get_index_in (GtkAccessible *parent,
              GtkAccessible *child)
{
  if (parent == NULL)
    return -1;

  guint res = 0;
  GtkAccessible *candidate;
  for (candidate = gtk_accessible_get_first_accessible_child (parent);
       candidate != NULL;
       candidate = gtk_accessible_get_next_accessible_sibling (candidate))
    {
      g_object_unref (candidate);

      if (candidate == child)
        return res;

      if (!gtk_accessible_should_present (candidate))
        continue;

      res++;
    }

  return -1;
}

static int
get_index_in_parent (GtkAccessible *accessible)
{
  GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);

  if (parent != NULL)
    {
      int res = get_index_in (parent, accessible);

      g_object_unref (parent);

      return res;
    }

  return -1;
}

static int
get_index_in_toplevels (GtkWidget *widget)
{
  GListModel *toplevels = gtk_window_get_toplevels ();
  guint n_toplevels = g_list_model_get_n_items (toplevels);
  GtkWidget *window;
  int idx;

  idx = 0;
  for (guint i = 0; i < n_toplevels; i++)
    {
      window = g_list_model_get_item (toplevels, i);

      g_object_unref (window);

      if (window == widget)
        return idx;

      if (!gtk_accessible_should_present (GTK_ACCESSIBLE (window)))
        continue;

      idx += 1;
    }

  return -1;
}

static GVariant *
get_parent_context_ref (GtkAccessible *accessible)
{
  GVariant *res = NULL;
  GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);

  if (parent == NULL)
    {
      GtkATContext *context = gtk_accessible_get_at_context (accessible);
      GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);

      res = gtk_at_spi_root_to_ref (self->root);

      g_object_unref (context);
    }
  else
    {
      GtkATContext *parent_context = gtk_accessible_get_at_context (parent);

      gtk_at_context_realize (parent_context);

      res = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (parent_context));

      g_object_unref (parent_context);
      g_object_unref (parent);
    }

  if (res == NULL)
    res = gtk_at_spi_null_ref ();

  return res;
}

static void
handle_accessible_method (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  GTK_DEBUG (A11Y, "handling %s on %s", method_name, object_path);

  if (g_strcmp0 (method_name, "GetRole") == 0)
    {
      guint atspi_role = gtk_atspi_role_for_context (GTK_AT_CONTEXT (self));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", atspi_role));
    }
  else if (g_strcmp0 (method_name, "GetRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, NULL);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetLocalizedRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, GETTEXT_PACKAGE);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetState") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(au)"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("au"));
      collect_states (self, &builder);
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(a{ss})"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
      g_variant_builder_add (&builder, "{ss}", "toolkit", "GTK");

      if (gtk_at_context_has_accessible_property (GTK_AT_CONTEXT (self), GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER))
        {
          GtkAccessibleValue *value;

          value = gtk_at_context_get_accessible_property (GTK_AT_CONTEXT (self), GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER);

          g_variant_builder_add (&builder, "{ss}",
                                 "placeholder-text", gtk_string_accessible_value_get (value));
        }

      if (gtk_at_context_has_accessible_relation (GTK_AT_CONTEXT (self), GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT))
        {
          GtkAccessibleValue *value = gtk_at_context_get_accessible_relation (GTK_AT_CONTEXT (self),
                                                                              GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT);

          g_variant_builder_add (&builder, "{ss}",
                                 "colindextext", gtk_string_accessible_value_get (value));
        }

      if (gtk_at_context_has_accessible_relation (GTK_AT_CONTEXT (self), GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT))
        {
          GtkAccessibleValue *value = gtk_at_context_get_accessible_relation (GTK_AT_CONTEXT (self),
                                                                              GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT);

          g_variant_builder_add (&builder, "{ss}",
                                 "rowindextext", gtk_string_accessible_value_get (value));
        }

      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetApplication") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(@(so))", gtk_at_spi_root_to_ref (self->root)));
    }
  else if (g_strcmp0 (method_name, "GetChildAtIndex") == 0)
    {
      GtkATContext *context = NULL;
      GtkAccessible *accessible;
      GtkAccessible *child = NULL;
      int idx, presentable_idx;

      g_variant_get (parameters, "(i)", &idx);

      accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));

      if (GTK_IS_AT_SPI_SOCKET (accessible))
        {
          GtkAtSpiSocket *socket = GTK_AT_SPI_SOCKET (accessible);
          GVariant *ref = gtk_at_spi_socket_to_ref (socket);

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", ref));
          return;
        }

      presentable_idx = 0;

      for (child = gtk_accessible_get_first_accessible_child (accessible);
           child != NULL;
           child = gtk_accessible_get_next_accessible_sibling (child))
        {
          g_object_unref (child);

          if (!gtk_accessible_should_present (child))
            continue;

          if (presentable_idx == idx)
            break;

          presentable_idx += 1;
        }

      if (child != NULL)
        context = gtk_accessible_get_at_context (child);

      if (context == NULL)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "No child with index %d", idx);
          return;
        }

      /* Realize the child ATContext in order to get its ref */
      gtk_at_context_realize (context);

      GVariant *ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (context));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", ref));

      g_object_unref (context);
    }
  else if (g_strcmp0 (method_name, "GetChildren") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkAccessible *child = NULL;

      if (GTK_IS_AT_SPI_SOCKET (accessible))
        {
          GtkAtSpiSocket *socket = GTK_AT_SPI_SOCKET (accessible);
          GVariant *ref = gtk_at_spi_socket_to_ref (socket);
          g_variant_builder_add (&builder, "@(so)", ref);
        }

      for (child = gtk_accessible_get_first_accessible_child (accessible);
           child != NULL;
           child = gtk_accessible_get_next_accessible_sibling (child))
        {
          g_object_unref (child);

          if (!gtk_accessible_should_present (child))
            continue;

          GtkATContext *context = gtk_accessible_get_at_context (child);

          /* Realize the child ATContext in order to get its ref */
          gtk_at_context_realize (context);

          GVariant *ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (context));

          if (ref != NULL)
            g_variant_builder_add (&builder, "@(so)", ref);

          g_object_unref (context);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(so))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetIndexInParent") == 0)
    {
      int idx = gtk_at_spi_context_get_index_in_parent (self);

      if (idx == -1)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Not found");
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", idx));
    }
  else if (g_strcmp0 (method_name, "GetRelationSet") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(ua(so))"));
      collect_relations (self, &builder);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(ua(so)))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetInterfaces") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@as)", self->interfaces));
    }

}

static GVariant *
handle_accessible_get_property (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *property_name,
                                GError               **error,
                                gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;
  GVariant *res = NULL;

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));

  GTK_DEBUG (A11Y, "handling GetProperty %s on %s", property_name, object_path);

  if (g_strcmp0 (property_name, "Name") == 0)
    {
      char *label = gtk_at_context_get_name (GTK_AT_CONTEXT (self));
      res = g_variant_new_string (label ? label : "");
      g_free (label);
    }
  else if (g_strcmp0 (property_name, "Description") == 0)
    {
      char *label = gtk_at_context_get_description (GTK_AT_CONTEXT (self));
      res = g_variant_new_string (label ? label : "");
      g_free (label);
    }
  else if (g_strcmp0 (property_name, "Locale") == 0)
    res = g_variant_new_string (setlocale (LC_MESSAGES, NULL));
  else if (g_strcmp0 (property_name, "AccessibleId") == 0)
    res = g_variant_new_string ("");
  else if (g_strcmp0 (property_name, "Parent") == 0)
    res = get_parent_context_ref (accessible);
  else if (g_strcmp0 (property_name, "ChildCount") == 0)
    res = g_variant_new_int32 (gtk_at_spi_context_get_child_count (self));
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable accessible_vtable = {
  handle_accessible_method,
  handle_accessible_get_property,
  NULL,
};
/* }}} */
/* {{{ Change notification */
static void
emit_text_changed (GtkAtSpiContext *self,
                   const char      *kind,
                   int              start,
                   int              end,
                   const char      *text)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "TextChanged",
                                 g_variant_new ("(siiva{sv})",
                                                kind, start, end,
                                                g_variant_new_string (text),
                                                NULL),
                                 NULL);
}

static void
emit_text_selection_changed (GtkAtSpiContext *self,
                             const char      *kind,
                             int              cursor_position)
{
  if (self->connection == NULL)
    return;

  if (strcmp (kind, "text-caret-moved") == 0)
    g_dbus_connection_emit_signal (self->connection,
                                   NULL,
                                   self->context_path,
                                   "org.a11y.atspi.Event.Object",
                                   "TextCaretMoved",
                                   g_variant_new ("(siiva{sv})",
                                                  "", cursor_position, 0, g_variant_new_int32 (0), NULL),
                                 NULL);
  else
    g_dbus_connection_emit_signal (self->connection,
                                   NULL,
                                   self->context_path,
                                   "org.a11y.atspi.Event.Object",
                                   "TextSelectionChanged",
                                   g_variant_new ("(siiva{sv})",
                                                  "", 0, 0, g_variant_new_string (""), NULL),
                                   NULL);
}

static void
emit_selection_changed (GtkAtSpiContext *self,
                        const char      *kind)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "SelectionChanged",
                                 g_variant_new ("(siiva{sv})",
                                                "", 0, 0, g_variant_new_string (""), NULL),
                                 NULL);
}

static void
emit_state_changed (GtkAtSpiContext *self,
                    const char      *name,
                    gboolean         enabled)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "StateChanged",
                                 g_variant_new ("(siiva{sv})",
                                                name, enabled, 0, g_variant_new_string ("0"), NULL),
                                 NULL);
}

static void
emit_defunct (GtkAtSpiContext *self)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "StateChanged",
                                 g_variant_new ("(siiva{sv})", "defunct", TRUE, 0, g_variant_new_string ("0"), NULL),
                                 NULL);
}

static void
emit_property_changed (GtkAtSpiContext *self,
                       const char      *name,
                       GVariant        *value)
{
  GVariant *value_owned = g_variant_ref_sink (value);

  if (self->connection == NULL)
    {
      g_variant_unref (value_owned);
      return;
    }

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "PropertyChange",
                                 g_variant_new ("(siiva{sv})",
                                                name, 0, 0, value_owned, NULL),
                                 NULL);
  g_variant_unref (value_owned);
}

static void
emit_bounds_changed (GtkAtSpiContext *self,
                     int              x,
                     int              y,
                     int              width,
                     int              height)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "BoundsChanged",
                                 g_variant_new ("(siiva{sv})",
                                                "", 0, 0, g_variant_new ("(iiii)", x, y, width, height), NULL),
                                 NULL);
}

static void
emit_children_changed (GtkAtSpiContext         *self,
                       GtkAtSpiContext         *child_context,
                       int                      idx,
                       GtkAccessibleChildState  state)
{
  /* If we don't have a connection on either contexts, we cannot emit a signal */
  if (self->connection == NULL || child_context->connection == NULL)
    return;

  GVariant *context_ref = gtk_at_spi_context_to_ref (self);
  GVariant *child_ref = gtk_at_spi_context_to_ref (child_context);

  gtk_at_spi_emit_children_changed (self->connection,
                                    self->context_path,
                                    state,
                                    idx,
                                    child_ref,
                                    context_ref);
}

static void
emit_window_event (GtkAtSpiContext *self,
                   const char      *event_type)
{
  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Window",
                                 event_type,
                                 g_variant_new ("(siiva{sv})",
                                                "", 0, 0,
                                                g_variant_new_string("0"),
                                                NULL),
                                 NULL);
}

static void
gtk_at_spi_context_state_change (GtkATContext                *ctx,
                                 GtkAccessibleStateChange     changed_states,
                                 GtkAccessiblePropertyChange  changed_properties,
                                 GtkAccessibleRelationChange  changed_relations,
                                 GtkAccessibleAttributeSet   *states,
                                 GtkAccessibleAttributeSet   *properties,
                                 GtkAccessibleAttributeSet   *relations)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (ctx);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  GtkAccessibleValue *value;

  if (GTK_IS_WIDGET (accessible) && !gtk_widget_get_realized (GTK_WIDGET (accessible)))
    return;

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_HIDDEN)
    {
      GtkAccessibleChildChange change;

      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_HIDDEN);
      if (gtk_boolean_accessible_value_get (value))
        change = GTK_ACCESSIBLE_CHILD_CHANGE_REMOVED;
      else
        change = GTK_ACCESSIBLE_CHILD_CHANGE_ADDED;

      if (GTK_IS_ROOT (accessible))
        {
          gtk_at_spi_root_child_changed (self->root, change, accessible);
          emit_state_changed (self, "showing", gtk_boolean_accessible_value_get (value));
          emit_state_changed (self, "visible", gtk_boolean_accessible_value_get (value));
        }
      else
        {
          GtkAccessible *parent =
            gtk_accessible_get_accessible_parent (accessible);
          GtkATContext *context =
            gtk_accessible_get_at_context (parent);

          gtk_at_context_child_changed (context, change, accessible);

          g_object_unref (context);
          g_object_unref (parent);
        }
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_BUSY)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_BUSY);
      emit_state_changed (self, "busy", gtk_boolean_accessible_value_get (value));
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_CHECKED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_CHECKED);

      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_TRISTATE)
        {
          switch (gtk_tristate_accessible_value_get (value))
            {
            case GTK_ACCESSIBLE_TRISTATE_TRUE:
              emit_state_changed (self, "checked", TRUE);
              emit_state_changed (self, "indeterminate", FALSE);
              break;
            case GTK_ACCESSIBLE_TRISTATE_MIXED:
              emit_state_changed (self, "checked", FALSE);
              emit_state_changed (self, "indeterminate", TRUE);
              break;
            case GTK_ACCESSIBLE_TRISTATE_FALSE:
              emit_state_changed (self, "checked", FALSE);
              emit_state_changed (self, "indeterminate", FALSE);
              break;
            default:
              break;
            }
        }
      else
        {
          emit_state_changed (self, "checked", FALSE);
          emit_state_changed (self, "indeterminate", TRUE);
        }
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_DISABLED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_DISABLED);
      emit_state_changed (self, "sensitive", !gtk_boolean_accessible_value_get (value));
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_EXPANDED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_EXPANDED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          emit_state_changed (self, "expandable", TRUE);
          emit_state_changed (self, "expanded",gtk_boolean_accessible_value_get (value));
        }
      else
        emit_state_changed (self, "expandable", FALSE);
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_INVALID)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_INVALID);
      switch (gtk_invalid_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_INVALID_TRUE:
        case GTK_ACCESSIBLE_INVALID_GRAMMAR:
        case GTK_ACCESSIBLE_INVALID_SPELLING:
          emit_state_changed (self, "invalid", TRUE);
          break;
        case GTK_ACCESSIBLE_INVALID_FALSE:
          emit_state_changed (self, "invalid", FALSE);
          break;
        default:
          break;
        }
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_PRESSED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_PRESSED);

      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_TRISTATE)
        {
          switch (gtk_tristate_accessible_value_get (value))
            {
            case GTK_ACCESSIBLE_TRISTATE_TRUE:
              emit_state_changed (self, "pressed", TRUE);
              emit_state_changed (self, "indeterminate", FALSE);
              break;
            case GTK_ACCESSIBLE_TRISTATE_MIXED:
              emit_state_changed (self, "pressed", FALSE);
              emit_state_changed (self, "indeterminate", TRUE);
              break;
            case GTK_ACCESSIBLE_TRISTATE_FALSE:
              emit_state_changed (self, "pressed", FALSE);
              emit_state_changed (self, "indeterminate", FALSE);
              break;
            default:
              break;
            }
        }
      else
        {
          emit_state_changed (self, "pressed", FALSE);
          emit_state_changed (self, "indeterminate", TRUE);
        }
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_SELECTED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_SELECTED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          emit_state_changed (self, "selectable", TRUE);
          emit_state_changed (self, "selected",gtk_boolean_accessible_value_get (value));
        }
      else
        emit_state_changed (self, "selectable", FALSE);
    }

  if (changed_states & GTK_ACCESSIBLE_STATE_CHANGE_VISITED)
    {
      value = gtk_accessible_attribute_set_get_value (states, GTK_ACCESSIBLE_STATE_VISITED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          emit_state_changed (self, "visited",gtk_boolean_accessible_value_get (value));
        }
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_READ_ONLY)
    {
      gboolean readonly;

      value = gtk_accessible_attribute_set_get_value (properties, GTK_ACCESSIBLE_PROPERTY_READ_ONLY);
      readonly = gtk_boolean_accessible_value_get (value);

      emit_state_changed (self, "read-only", readonly);
      if (ctx->accessible_role == GTK_ACCESSIBLE_ROLE_TEXT_BOX)
        emit_state_changed (self, "editable", !readonly);
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_ORIENTATION)
    {
      value = gtk_accessible_attribute_set_get_value (properties, GTK_ACCESSIBLE_PROPERTY_ORIENTATION);
      if (gtk_orientation_accessible_value_get (value) == GTK_ORIENTATION_HORIZONTAL)
        {
          emit_state_changed (self, "horizontal", TRUE);
          emit_state_changed (self, "vertical", FALSE);
        }
      else
        {
          emit_state_changed (self, "horizontal", FALSE);
          emit_state_changed (self, "vertical", TRUE);
        }
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_MODAL)
    {
      value = gtk_accessible_attribute_set_get_value (properties, GTK_ACCESSIBLE_PROPERTY_MODAL);
      emit_state_changed (self, "modal", gtk_boolean_accessible_value_get (value));
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_MULTI_LINE)
    {
      value = gtk_accessible_attribute_set_get_value (properties, GTK_ACCESSIBLE_PROPERTY_MULTI_LINE);
      emit_state_changed (self, "multi-line", gtk_boolean_accessible_value_get (value));
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_LABEL)
    {
      char *label = gtk_at_context_get_name (GTK_AT_CONTEXT (self));
      GVariant *v = g_variant_new_take_string (label);
      emit_property_changed (self, "accessible-name", v);
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_DESCRIPTION)
  {
      char *label = gtk_at_context_get_description (GTK_AT_CONTEXT (self));
      GVariant *v = g_variant_new_take_string (label);
      emit_property_changed (self, "accessible-description", v);
    }

  if (changed_properties & GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_NOW)
    {
      value = gtk_accessible_attribute_set_get_value (properties, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW);
      emit_property_changed (self,
                             "accessible-value",
                             g_variant_new_double (gtk_number_accessible_value_get (value)));
    }
}

static void
gtk_at_spi_context_platform_change (GtkATContext                *ctx,
                                    GtkAccessiblePlatformChange  changed_platform)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (ctx);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  GtkWidget *widget;

  if (!GTK_IS_WIDGET (accessible))
    return;

  widget = GTK_WIDGET (accessible);
  if (!gtk_widget_get_realized (widget))
    return;

  if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSABLE)
    {
      gboolean state = gtk_accessible_get_platform_state (GTK_ACCESSIBLE (widget),
                                                          GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSABLE);
      emit_state_changed (self, "focusable", state);
    }

  if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_FOCUSED)
    {
      gboolean state = gtk_accessible_get_platform_state (GTK_ACCESSIBLE (widget),
                                                          GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED);
      emit_state_changed (self, "focused", state);
    }

  if (changed_platform & GTK_ACCESSIBLE_PLATFORM_CHANGE_ACTIVE)
    {
      gboolean state = gtk_accessible_get_platform_state (GTK_ACCESSIBLE (widget),
                                                          GTK_ACCESSIBLE_PLATFORM_STATE_ACTIVE);
      emit_state_changed (self, "active", state);

      /* Orca tracks the window:activate and window:deactivate events on top
       * levels to decide whether to track other AT-SPI events
       */
      if (gtk_accessible_get_accessible_role (accessible) == GTK_ACCESSIBLE_ROLE_APPLICATION)
        {
          if (state)
            emit_window_event (self, "activate");
          else
            emit_window_event (self, "deactivate");
        }
    }
}

static void
gtk_at_spi_context_bounds_change (GtkATContext *ctx)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (ctx);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  int x, y, width, height;
  if (gtk_accessible_get_bounds (accessible, &x, &y, &width, &height))
    emit_bounds_changed (self, x, y, width, height);
}

static void
gtk_at_spi_context_child_change (GtkATContext             *ctx,
                                 GtkAccessibleChildChange  change,
                                 GtkAccessible            *child)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (ctx);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  GtkATContext *child_context = gtk_accessible_get_at_context (child);

  if (child_context == NULL)
    return;

  GtkAccessible *parent = gtk_accessible_get_accessible_parent (child);
  int idx = 0;

  if (parent == NULL)
    {
      idx = -1;
    }
  else if (parent == accessible)
    {
      idx = get_index_in (accessible, child);
      g_object_unref (parent);
    }
  else
    {
      g_object_unref (parent);
    }

  if (change & GTK_ACCESSIBLE_CHILD_CHANGE_ADDED)
  {
    gtk_at_context_realize (child_context);
    emit_children_changed (self,
                           GTK_AT_SPI_CONTEXT (child_context),
                           idx,
                           GTK_ACCESSIBLE_CHILD_STATE_ADDED);
  }
  else if (change & GTK_ACCESSIBLE_CHILD_CHANGE_REMOVED)
    emit_children_changed (self,
                           GTK_AT_SPI_CONTEXT (child_context),
                           idx,
                           GTK_ACCESSIBLE_CHILD_STATE_REMOVED);

  g_object_unref (child_context);
}
/* }}} */
/* {{{ D-Bus Registration */
static void
gtk_at_spi_context_register_object (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GVariantBuilder interfaces = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_STRING_ARRAY);
  const GDBusInterfaceVTable *vtable;

  g_variant_builder_add (&interfaces, "s", atspi_accessible_interface.name);
  self->registration_ids[self->n_registered_objects] =
      g_dbus_connection_register_object (self->connection,
                                         self->context_path,
                                         (GDBusInterfaceInfo *) &atspi_accessible_interface,
                                         &accessible_vtable,
                                         self,
                                         NULL,
                                         NULL);
  self->n_registered_objects++;

  vtable = gtk_atspi_get_component_vtable (accessible);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_component_interface.name);
      self->registration_ids[self->n_registered_objects] =
          g_dbus_connection_register_object (self->connection,
                                             self->context_path,
                                             (GDBusInterfaceInfo *) &atspi_component_interface,
                                             vtable,
                                             self,
                                             NULL,
                                             NULL);
      self->n_registered_objects++;
    }

  vtable = gtk_atspi_get_text_vtable (accessible);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_text_interface.name);
      self->registration_ids[self->n_registered_objects] =
          g_dbus_connection_register_object (self->connection,
                                             self->context_path,
                                             (GDBusInterfaceInfo *) &atspi_text_interface,
                                             vtable,
                                             self,
                                             NULL,
                                             NULL);
      self->n_registered_objects++;
    }

  vtable = gtk_atspi_get_editable_text_vtable (accessible);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_editable_text_interface.name);
      self->registration_ids[self->n_registered_objects] =
          g_dbus_connection_register_object (self->connection,
                                             self->context_path,
                                             (GDBusInterfaceInfo *) &atspi_editable_text_interface,
                                             vtable,
                                             self,
                                             NULL,
                                             NULL);
      self->n_registered_objects++;
    }
  vtable = gtk_atspi_get_value_vtable (accessible);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_value_interface.name);
      self->registration_ids[self->n_registered_objects] =
          g_dbus_connection_register_object (self->connection,
                                             self->context_path,
                                             (GDBusInterfaceInfo *) &atspi_value_interface,
                                             vtable,
                                             self,
                                             NULL,
                                             NULL);
      self->n_registered_objects++;
    }

  /* Calling gtk_accessible_get_accessible_role() in here will recurse,
   * so pass the role in explicitly.
   */
  vtable = gtk_atspi_get_selection_vtable (accessible,
                                           GTK_AT_CONTEXT (self)->accessible_role);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_selection_interface.name);
      self->registration_ids[self->n_registered_objects] =
          g_dbus_connection_register_object (self->connection,
                                             self->context_path,
                                             (GDBusInterfaceInfo *) &atspi_selection_interface,
                                             vtable,
                                             self,
                                             NULL,
                                             NULL);
      self->n_registered_objects++;
    }

  vtable = gtk_atspi_get_action_vtable (accessible);
  if (vtable)
    {
      g_variant_builder_add (&interfaces, "s", atspi_action_interface.name);
      self->registration_ids[self->n_registered_objects] =
        g_dbus_connection_register_object (self->connection,
                                           self->context_path,
                                           (GDBusInterfaceInfo *) &atspi_action_interface,
                                           vtable,
                                           self,
                                           NULL,
                                           NULL);
      self->n_registered_objects++;
    }

  self->interfaces = g_variant_ref_sink (g_variant_builder_end (&interfaces));

  GTK_DEBUG (A11Y, "Registered %d interfaces on object path '%s'",
                   self->n_registered_objects,
                   self->context_path);

  if (GTK_IS_AT_SPI_SOCKET (accessible))
    {
      GtkAtSpiSocket *socket = GTK_AT_SPI_SOCKET (accessible);

      gtk_at_spi_socket_embed (socket, self->connection);

      GTK_DEBUG (A11Y, "Embedded plug %s:%s in socket %s",
                 gtk_at_spi_socket_get_bus_name (socket),
                 gtk_at_spi_socket_get_object_path (socket),
                 self->context_path);
    }
}

static void
gtk_at_spi_context_unregister_object (GtkAtSpiContext *self)
{
  while (self->n_registered_objects > 0)
    {
      self->n_registered_objects--;
      g_dbus_connection_unregister_object (self->connection,
                                           self->registration_ids[self->n_registered_objects]);
      self->registration_ids[self->n_registered_objects] = 0;
    }

  g_clear_pointer (&self->interfaces, g_variant_unref);
}
/* }}} */
/* {{{ GObject boilerplate */
static void
gtk_at_spi_context_finalize (GObject *gobject)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  gtk_at_spi_context_unregister_object (self);

  g_clear_object (&self->root);

  g_free (self->context_path);

  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->finalize (gobject);
}

static const char *get_bus_address (GdkDisplay *display);

static void
register_object (GtkAtSpiRoot *root,
                 GtkAtSpiContext *context)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (context));

  gtk_atspi_connect_text_signals (accessible,
                                  (GtkAtspiTextChangedCallback *)emit_text_changed,
                                  (GtkAtspiTextSelectionCallback *)emit_text_selection_changed,
                                  context);
  gtk_atspi_connect_selection_signals (accessible,
                                       (GtkAtspiSelectionCallback *)emit_selection_changed,
                                       context);
  gtk_at_spi_context_register_object (context);
}

static void
gtk_at_spi_context_realize (GtkATContext *context)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);
  GdkDisplay *display = gtk_at_context_get_display (context);

  /* Every GTK application has a single root AT-SPI object, which
   * handles all the global state, including the cache of accessible
   * objects. We use the GdkDisplay to store it, so it's guaranteed
   * to be a unique per-display connection
   */
  self->root =
    g_object_get_data (G_OBJECT (display), "-gtk-atspi-root");

  if (self->root == NULL)
    {
      self->root = gtk_at_spi_root_new (get_bus_address (display));
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-root",
                              g_object_ref (self->root),
                              g_object_unref);
    }
  else
    {
      g_object_ref (self->root);
    }

  /* UUIDs use '-' as the separator, but that's not a valid character
   * for a DBus object path
   */
  char *uuid = g_uuid_string_random ();
  size_t len = strlen (uuid);
  for (size_t i = 0; i < len; i++)
    {
      if (uuid[i] == '-')
        uuid[i] = '_';
    }

  self->context_path =
    g_strconcat (gtk_at_spi_root_get_base_path (self->root), "/", uuid, NULL);

  g_free (uuid);

  self->connection = gtk_at_spi_root_get_connection (self->root);
  if (self->connection == NULL)
    return;

  if (GTK_DEBUG_CHECK (A11Y))
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (context);
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (context);
      char *role_name = g_enum_to_string (GTK_TYPE_ACCESSIBLE_ROLE, role);
      g_message ("Realizing ATSPI context “%s” for accessible “%s”, with role: “%s”",
                 self->context_path,
                 G_OBJECT_TYPE_NAME (accessible),
                 role_name);
      g_free (role_name);
    }

  gtk_at_spi_root_queue_register (self->root, self, register_object);
}

static void
gtk_at_spi_context_unrealize (GtkATContext *context)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);

  GTK_DEBUG (A11Y, "Unrealizing ATSPI context at '%s' for accessible '%s'",
                   self->context_path,
                   G_OBJECT_TYPE_NAME (accessible));

  /* Notify ATs that the accessible object is going away */
  emit_defunct (self);
  gtk_at_spi_root_unregister (self->root, self);

  gtk_atspi_disconnect_text_signals (accessible);
  gtk_atspi_disconnect_selection_signals (accessible);
  gtk_at_spi_context_unregister_object (self);

  g_clear_pointer (&self->context_path, g_free);
  g_clear_object (&self->root);
}

static void
gtk_at_spi_context_announce (GtkATContext                      *context,
                             const char                        *message,
                             GtkAccessibleAnnouncementPriority  priority)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);
  AtspiLive live;

  if (self->connection == NULL)
    return;

  switch (priority)
    {
    case GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_LOW:
    case GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM:
      live = ATSPI_LIVE_POLITE;
      break;
    case GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH:
      live = ATSPI_LIVE_ASSERTIVE;
      break;
    default:
      g_assert_not_reached ();
    }

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "Announcement",
                                 g_variant_new ("(siiva{sv})",
                                                "", live, 0,
                                                g_variant_new_string (message),
                                                NULL),
                                 NULL);
}

static void
gtk_at_spi_context_update_caret_position (GtkATContext *context)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);
  GtkAccessibleText *accessible_text = GTK_ACCESSIBLE_TEXT (accessible);
  guint offset;

  if (self->connection == NULL)
    return;

  offset = gtk_accessible_text_get_caret_position (accessible_text);

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "TextCaretMoved",
                                 g_variant_new ("(siiva{sv})",
                                                "",
                                                (int) offset,
                                                0,
                                                g_variant_new_int32 (0),
                                                NULL),
                                 NULL);
}

static void
gtk_at_spi_context_update_selection_bound (GtkATContext *context)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);

  if (self->connection == NULL)
    return;

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "TextSelectionChanged",
                                 g_variant_new ("(siiva{sv})",
                                                "",
                                                0,
                                                0,
                                                g_variant_new_string (""),
                                                NULL),
                                 NULL);
}

static void
gtk_at_spi_context_update_text_contents (GtkATContext *context,
                                         GtkAccessibleTextContentChange change,
                                         unsigned int start,
                                         unsigned int end)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (context);

  if (self->connection == NULL)
    return;

  GtkAccessible *accessible = gtk_at_context_get_accessible (context);
  if (!GTK_IS_ACCESSIBLE_TEXT (accessible))
    return;

  const char *kind = "";

  switch (change)
    {
    case GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT:
      kind = "insert";
      break;

    case GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE:
      kind = "delete";
      break;

    default:
      g_assert_not_reached ();
    }

  /* Retrieve the text using the given range */
  GBytes *contents = gtk_accessible_text_get_contents (GTK_ACCESSIBLE_TEXT (accessible),
                                                       start, end);
  if (contents == NULL)
    goto out;

  const char *text = g_bytes_get_data (contents, NULL);
  if (text == NULL)
    goto out;

  /* Using G_MAXUINT in GTK maps to the text length */
  if (end == G_MAXUINT)
    end = g_utf8_strlen (text, -1);

  g_dbus_connection_emit_signal (self->connection,
                                 NULL,
                                 self->context_path,
                                 "org.a11y.atspi.Event.Object",
                                 "TextChanged",
                                 g_variant_new ("(siiva{sv})",
                                                kind,
                                                start,
                                                end - start,
                                                g_variant_new_string (text),
                                                NULL),
                                 NULL);

out:
  g_clear_pointer (&contents, g_bytes_unref);
}

static void
gtk_at_spi_context_class_init (GtkAtSpiContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->finalize = gtk_at_spi_context_finalize;

  context_class->realize = gtk_at_spi_context_realize;
  context_class->unrealize = gtk_at_spi_context_unrealize;
  context_class->state_change = gtk_at_spi_context_state_change;
  context_class->platform_change = gtk_at_spi_context_platform_change;
  context_class->bounds_change = gtk_at_spi_context_bounds_change;
  context_class->child_change = gtk_at_spi_context_child_change;
  context_class->announce = gtk_at_spi_context_announce;
  context_class->update_caret_position = gtk_at_spi_context_update_caret_position;
  context_class->update_selection_bound = gtk_at_spi_context_update_selection_bound;
  context_class->update_text_contents = gtk_at_spi_context_update_text_contents;
}

static void
gtk_at_spi_context_init (GtkAtSpiContext *self)
{
}
/* }}} */
/* {{{ Bus address discovery */
#ifdef GDK_WINDOWING_X11
static char *
get_bus_address_x11 (GdkDisplay *display)
{
  GTK_DEBUG (A11Y, "Acquiring a11y bus via X11...");

  Display *xdisplay = gdk_x11_display_get_xdisplay (display);
  Atom type_return;
  int format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;
  char *address = NULL;

  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (xdisplay, DefaultRootWindow (xdisplay),
                      gdk_x11_get_xatom_by_name_for_display (display, "AT_SPI_BUS"),
                      0L, BUFSIZ, False,
                      (Atom) 31,
                      &type_return, &format_return, &nitems_return,
                      &bytes_after_return, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  address = g_strdup ((char *) data);

  XFree (data);

  return address;
}
#endif

#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static char *
get_bus_address_dbus (GdkDisplay *display)
{
  GTK_DEBUG (A11Y, "Acquiring a11y bus via DBus...");

  GError *error = NULL;
  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Unable to acquire session bus: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  GVariant *res =
    g_dbus_connection_call_sync (connection, "org.a11y.Bus",
                                  "/org/a11y/bus",
                                  "org.a11y.Bus",
                                  "GetAddress",
                                  NULL, NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  if (error != NULL)
    {
      g_warning ("Unable to acquire the address of the accessibility bus: %s. "
                 "If you are attempting to run GTK without a11y support, "
                 "GTK_A11Y should be set to 'none'.",
                 error->message);
      g_error_free (error);
    }

  char *address = NULL;
  if (res != NULL)
    {
      g_variant_get (res, "(s)", &address);
      g_variant_unref (res);
    }

  g_object_unref (connection);

  return address;
}
#endif

static const char *
get_bus_address (GdkDisplay *display)
{
  const char *bus_address;

  bus_address = g_object_get_data (G_OBJECT (display), "-gtk-atspi-bus-address");
  if (bus_address != NULL)
    return bus_address;

  /* The bus address environment variable takes precedence; this is the
   * mechanism used by Flatpak to handle the accessibility bus portal
   * between the sandbox and the outside world
   */
  bus_address = g_getenv ("AT_SPI_BUS_ADDRESS");
  if (bus_address != NULL && *bus_address != '\0')
    {
      GTK_DEBUG (A11Y, "Using ATSPI bus address from environment: %s", bus_address);
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                              g_strdup (bus_address),
                              g_free);
      goto out;
    }

#if defined(GDK_WINDOWING_WAYLAND)
  if (bus_address == NULL)
    {
      if (GDK_IS_WAYLAND_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          GTK_DEBUG (A11Y, "Using ATSPI bus address from D-Bus: %s", addr);
          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif
#if defined(GDK_WINDOWING_X11)
  if (bus_address == NULL)
    {
      if (GDK_IS_X11_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          if (addr == NULL)
            {
              addr = get_bus_address_x11 (display);
              GTK_DEBUG (A11Y, "Using ATSPI bus address from X11: %s", addr);
            }
          else
            {
              GTK_DEBUG (A11Y, "Using ATSPI bus address from D-Bus: %s", addr);
            }

          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif

out:

  if (bus_address == NULL)
    g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                            g_strdup (""),
                            g_free);

  return bus_address;
}

/* }}} */
/* {{{ API */
GtkATContext *
gtk_at_spi_create_context (GtkAccessibleRole  accessible_role,
                           GtkAccessible     *accessible,
                           GdkDisplay        *display)
{
  const char *bus_address;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  bus_address = get_bus_address (display);

  if (bus_address == NULL)
    bus_address = "";

  if (*bus_address == '\0')
    return NULL;

#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         NULL);
#endif
#if defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         NULL);
#endif

  return NULL;
}

const char *
gtk_at_spi_context_get_context_path (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  return self->context_path;
}

/*< private >
 * gtk_at_spi_context_to_ref:
 * @self: a `GtkAtSpiContext`
 *
 * Returns an ATSPI object reference for the `GtkAtSpiContext`.
 *
 * Returns: (transfer floating): a `GVariant` with the reference
 */
GVariant *
gtk_at_spi_context_to_ref (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  if (self->context_path == NULL)
    return gtk_at_spi_null_ref ();

  const char *name = g_dbus_connection_get_unique_name (self->connection);

  return g_variant_new ("(so)", name, self->context_path);
}

GVariant *
gtk_at_spi_context_get_interfaces (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  return self->interfaces;
}

GVariant *
gtk_at_spi_context_get_states (GtkAtSpiContext *self)
{
  GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("au"));

  collect_states (self, &builder);

  return g_variant_builder_end (&builder);
}

GVariant *
gtk_at_spi_context_get_parent_ref (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));

  return get_parent_context_ref (accessible);
}

GtkAtSpiRoot *
gtk_at_spi_context_get_root (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  return self->root;
}

int
gtk_at_spi_context_get_index_in_parent (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), -1);

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  int idx;

  if (GTK_IS_ROOT (accessible))
    idx = get_index_in_toplevels (GTK_WIDGET (accessible));
  else
    idx = get_index_in_parent  (accessible);

  return idx;
}

int
gtk_at_spi_context_get_child_count (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), -1);

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  int n_children = 0;

  /* A socket always has exactly one child: the remote plug */
  if (GTK_IS_AT_SPI_SOCKET (accessible))
    return 1;

  GtkAccessible *child = NULL;
  for (child = gtk_accessible_get_first_accessible_child (accessible);
       child != NULL;
       child = gtk_accessible_get_next_accessible_sibling (child))
    {
      g_object_unref (child);

      if (!gtk_accessible_should_present (child))
        continue;

      n_children += 1;
    }

  return n_children;
}
/* }}} */

/* vim:set foldmethod=marker expandtab: */
