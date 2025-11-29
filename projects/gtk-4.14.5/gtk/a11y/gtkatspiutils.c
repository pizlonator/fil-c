/* gtkatspiutils.c: Shared utilities for ATSPI
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

#include "gtkatspiutilsprivate.h"

#include "gtkenums.h"
#include "gtkpasswordentry.h"
#include "gtkscrolledwindow.h"

/*< private >
 * gtk_accessible_role_to_atspi_role:
 * @role: a `GtkAccessibleRole`
 *
 * Converts a `GtkAccessibleRole` value to the equivalent ATSPI role.
 *
 * Returns: an #AtspiRole
 */
static AtspiRole
gtk_accessible_role_to_atspi_role (GtkAccessibleRole role)
{
  switch (role)
    {
    case GTK_ACCESSIBLE_ROLE_ALERT:
      return ATSPI_ROLE_ALERT;

    case GTK_ACCESSIBLE_ROLE_ALERT_DIALOG:
      return ATSPI_ROLE_ALERT;

    case GTK_ACCESSIBLE_ROLE_APPLICATION:
      return ATSPI_ROLE_FRAME;

    case GTK_ACCESSIBLE_ROLE_ARTICLE:
      return ATSPI_ROLE_ARTICLE;

    case GTK_ACCESSIBLE_ROLE_BANNER:
      break;

    case GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE:
      return ATSPI_ROLE_BLOCK_QUOTE;

    case GTK_ACCESSIBLE_ROLE_BUTTON:
      return ATSPI_ROLE_PUSH_BUTTON;

    case GTK_ACCESSIBLE_ROLE_CAPTION:
      return ATSPI_ROLE_CAPTION;

    case GTK_ACCESSIBLE_ROLE_CELL:
      return ATSPI_ROLE_TABLE_CELL;

    case GTK_ACCESSIBLE_ROLE_CHECKBOX:
      return ATSPI_ROLE_CHECK_BOX;

    case GTK_ACCESSIBLE_ROLE_COLUMN_HEADER:
      break;

    case GTK_ACCESSIBLE_ROLE_COMBO_BOX:
      return ATSPI_ROLE_COMBO_BOX;

    case GTK_ACCESSIBLE_ROLE_COMMAND:
      break;

    case GTK_ACCESSIBLE_ROLE_COMMENT:
      return ATSPI_ROLE_COMMENT;

    case GTK_ACCESSIBLE_ROLE_COMPOSITE:
      break;

    case GTK_ACCESSIBLE_ROLE_DIALOG:
      return ATSPI_ROLE_DIALOG;

    case GTK_ACCESSIBLE_ROLE_DOCUMENT:
      return ATSPI_ROLE_DOCUMENT_TEXT;

    case GTK_ACCESSIBLE_ROLE_FEED:
      break;

    case GTK_ACCESSIBLE_ROLE_FORM:
      return ATSPI_ROLE_FORM;

    case GTK_ACCESSIBLE_ROLE_GENERIC:
      return ATSPI_ROLE_PANEL;

    case GTK_ACCESSIBLE_ROLE_GRID:
      return ATSPI_ROLE_TABLE;

    case GTK_ACCESSIBLE_ROLE_GRID_CELL:
      return ATSPI_ROLE_TABLE_CELL;

    case GTK_ACCESSIBLE_ROLE_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_HEADING:
      return ATSPI_ROLE_HEADING;

    case GTK_ACCESSIBLE_ROLE_IMG:
      return ATSPI_ROLE_IMAGE;

    case GTK_ACCESSIBLE_ROLE_INPUT:
      return ATSPI_ROLE_ENTRY;

    case GTK_ACCESSIBLE_ROLE_LABEL:
      return ATSPI_ROLE_LABEL;

    case GTK_ACCESSIBLE_ROLE_LANDMARK:
      return ATSPI_ROLE_LANDMARK;

    case GTK_ACCESSIBLE_ROLE_LEGEND:
      return ATSPI_ROLE_LABEL;

    case GTK_ACCESSIBLE_ROLE_LINK:
      return ATSPI_ROLE_LINK;

    case GTK_ACCESSIBLE_ROLE_LIST:
      return ATSPI_ROLE_LIST;

    case GTK_ACCESSIBLE_ROLE_LIST_BOX:
      return ATSPI_ROLE_LIST_BOX;

    case GTK_ACCESSIBLE_ROLE_LIST_ITEM:
      return ATSPI_ROLE_LIST_ITEM;

    case GTK_ACCESSIBLE_ROLE_LOG:
      return ATSPI_ROLE_LOG;

    case GTK_ACCESSIBLE_ROLE_MAIN:
      break;

    case GTK_ACCESSIBLE_ROLE_MARQUEE:
      return ATSPI_ROLE_MARQUEE;

    case GTK_ACCESSIBLE_ROLE_MATH:
      return ATSPI_ROLE_MATH;

    case GTK_ACCESSIBLE_ROLE_METER:
      return ATSPI_ROLE_LEVEL_BAR;

    case GTK_ACCESSIBLE_ROLE_MENU:
      return ATSPI_ROLE_MENU;

    case GTK_ACCESSIBLE_ROLE_MENU_BAR:
      return ATSPI_ROLE_MENU_BAR;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM:
      return ATSPI_ROLE_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX:
      return ATSPI_ROLE_CHECK_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO:
      return ATSPI_ROLE_RADIO_MENU_ITEM;

    case GTK_ACCESSIBLE_ROLE_NAVIGATION:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_NONE:
      return ATSPI_ROLE_INVALID;

    case GTK_ACCESSIBLE_ROLE_NOTE:
      return ATSPI_ROLE_FOOTNOTE;

    case GTK_ACCESSIBLE_ROLE_OPTION:
      return ATSPI_ROLE_OPTION_PANE;

    case GTK_ACCESSIBLE_ROLE_PARAGRAPH:
      return ATSPI_ROLE_PARAGRAPH;

    case GTK_ACCESSIBLE_ROLE_PRESENTATION:
      return ATSPI_ROLE_INVALID;

    case GTK_ACCESSIBLE_ROLE_PROGRESS_BAR:
      return ATSPI_ROLE_PROGRESS_BAR;

    case GTK_ACCESSIBLE_ROLE_RADIO:
      return ATSPI_ROLE_RADIO_BUTTON;

    case GTK_ACCESSIBLE_ROLE_RADIO_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_RANGE:
      break;

    case GTK_ACCESSIBLE_ROLE_REGION:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_ROW:
      return ATSPI_ROLE_TABLE_ROW;

    case GTK_ACCESSIBLE_ROLE_ROW_GROUP:
      return ATSPI_ROLE_GROUPING;

    case GTK_ACCESSIBLE_ROLE_ROW_HEADER:
      return ATSPI_ROLE_ROW_HEADER;

    case GTK_ACCESSIBLE_ROLE_SCROLLBAR:
      return ATSPI_ROLE_SCROLL_BAR;

    case GTK_ACCESSIBLE_ROLE_SEARCH:
      return ATSPI_ROLE_FORM;

    case GTK_ACCESSIBLE_ROLE_SEARCH_BOX:
      return ATSPI_ROLE_ENTRY;

    case GTK_ACCESSIBLE_ROLE_SECTION:
      return ATSPI_ROLE_SECTION;

    case GTK_ACCESSIBLE_ROLE_SECTION_HEAD:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SELECT:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SEPARATOR:
      return ATSPI_ROLE_SEPARATOR;

    case GTK_ACCESSIBLE_ROLE_SLIDER:
      return ATSPI_ROLE_SLIDER;

    case GTK_ACCESSIBLE_ROLE_SPIN_BUTTON:
      return ATSPI_ROLE_SPIN_BUTTON;

    case GTK_ACCESSIBLE_ROLE_STATUS:
      return ATSPI_ROLE_STATUS_BAR;

    case GTK_ACCESSIBLE_ROLE_STRUCTURE:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_SWITCH:
      return ATSPI_ROLE_CHECK_BOX;

    case GTK_ACCESSIBLE_ROLE_TAB:
      return ATSPI_ROLE_PAGE_TAB;

    case GTK_ACCESSIBLE_ROLE_TABLE:
      return ATSPI_ROLE_TABLE;

    case GTK_ACCESSIBLE_ROLE_TAB_LIST:
      return ATSPI_ROLE_PAGE_TAB_LIST;

    case GTK_ACCESSIBLE_ROLE_TAB_PANEL:
      return ATSPI_ROLE_PANEL;

    case GTK_ACCESSIBLE_ROLE_TEXT_BOX:
      return ATSPI_ROLE_TEXT;

    case GTK_ACCESSIBLE_ROLE_TIME:
      return ATSPI_ROLE_TEXT;

    case GTK_ACCESSIBLE_ROLE_TIMER:
      return ATSPI_ROLE_TIMER;

    case GTK_ACCESSIBLE_ROLE_TOOLBAR:
      return ATSPI_ROLE_TOOL_BAR;

    case GTK_ACCESSIBLE_ROLE_TOOLTIP:
      return ATSPI_ROLE_TOOL_TIP;

    case GTK_ACCESSIBLE_ROLE_TREE:
      return ATSPI_ROLE_TREE;

    case GTK_ACCESSIBLE_ROLE_TREE_GRID:
      return ATSPI_ROLE_TREE_TABLE;

    case GTK_ACCESSIBLE_ROLE_TREE_ITEM:
      return ATSPI_ROLE_TREE_ITEM;

    case GTK_ACCESSIBLE_ROLE_WIDGET:
      return ATSPI_ROLE_FILLER;

    case GTK_ACCESSIBLE_ROLE_WINDOW:
      return ATSPI_ROLE_FRAME;

    case GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON:
      return ATSPI_ROLE_TOGGLE_BUTTON;

    case GTK_ACCESSIBLE_ROLE_TERMINAL:
      return ATSPI_ROLE_TERMINAL;

    default:
      break;
    }

  return ATSPI_ROLE_FILLER;
}

/*<private>
 * gtk_atspi_role_for_context:
 * @context: a `GtkATContext`
 *
 * Returns a suitable ATSPI role for a context, taking into account
 * both the `GtkAccessibleRole` set on the context and the type
 * of accessible.
 *
 * Returns: an #AtspiRole
 */
AtspiRole
gtk_atspi_role_for_context (GtkATContext *context)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (context);
  GtkAccessibleRole role = gtk_at_context_get_accessible_role (context);

  /* ARIA does not have a "password entry" role, so we need to fudge it here */
  if (GTK_IS_PASSWORD_ENTRY (accessible))
    return ATSPI_ROLE_PASSWORD_TEXT;

  /* ARIA does not have a "scroll area" role */
  if (GTK_IS_SCROLLED_WINDOW (accessible))
    return ATSPI_ROLE_SCROLL_PANE;

  return gtk_accessible_role_to_atspi_role (role);
}

GVariant *
gtk_at_spi_null_ref (void)
{
  return g_variant_new ("(so)", "", "/org/a11y/atspi/null");
}

void
gtk_at_spi_emit_children_changed (GDBusConnection         *connection,
                                  const char              *path,
                                  GtkAccessibleChildState  state,
                                  int                      idx,
                                  GVariant                *child_ref,
                                  GVariant                *sender_ref)
{
  const char *change;

  switch (state)
    {
    case GTK_ACCESSIBLE_CHILD_STATE_ADDED:
      change = "add";
      break;

    case GTK_ACCESSIBLE_CHILD_STATE_REMOVED:
      change = "remove";
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  g_dbus_connection_emit_signal (connection,
                                 NULL,
                                 path,
                                 "org.a11y.atspi.Event.Object",
                                 "ChildrenChanged",
                                 g_variant_new ("(siiv@(so))", change, idx, 0, child_ref, sender_ref),
                                 NULL);
}
