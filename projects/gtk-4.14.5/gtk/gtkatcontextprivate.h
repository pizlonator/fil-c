/* gtkatcontextprivate.h: Private header for GtkATContext
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

#pragma once

#include "gtkatcontext.h"

#include "gtkaccessibleprivate.h"
#include "gtkaccessibleattributesetprivate.h"
#include "gtkaccessibletext.h"

G_BEGIN_DECLS

typedef enum {
  GTK_ACCESSIBLE_PROPERTY_CHANGE_AUTOCOMPLETE     = 1 << GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_DESCRIPTION      = 1 << GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_HAS_POPUP        = 1 << GTK_ACCESSIBLE_PROPERTY_HAS_POPUP,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_KEY_SHORTCUTS    = 1 << GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_LABEL            = 1 << GTK_ACCESSIBLE_PROPERTY_LABEL,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_LEVEL            = 1 << GTK_ACCESSIBLE_PROPERTY_LEVEL,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_MODAL            = 1 << GTK_ACCESSIBLE_PROPERTY_MODAL,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_MULTI_LINE       = 1 << GTK_ACCESSIBLE_PROPERTY_MULTI_LINE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_MULTI_SELECTABLE = 1 << GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_ORIENTATION      = 1 << GTK_ACCESSIBLE_PROPERTY_ORIENTATION,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_PLACEHOLDER      = 1 << GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_READ_ONLY        = 1 << GTK_ACCESSIBLE_PROPERTY_READ_ONLY,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_REQUIRED         = 1 << GTK_ACCESSIBLE_PROPERTY_REQUIRED,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_ROLE_DESCRIPTION = 1 << GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_SORT             = 1 << GTK_ACCESSIBLE_PROPERTY_SORT,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_MAX        = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_MIN        = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_NOW        = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_NOW,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_TEXT       = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT
} GtkAccessiblePropertyChange;

typedef enum {
  GTK_ACCESSIBLE_RELATION_CHANGE_ACTIVE_DESCENDANT = 1 << GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT,
  GTK_ACCESSIBLE_RELATION_CHANGE_COL_COUNT         = 1 << GTK_ACCESSIBLE_RELATION_COL_COUNT,
  GTK_ACCESSIBLE_RELATION_CHANGE_COL_INDEX         = 1 << GTK_ACCESSIBLE_RELATION_COL_INDEX,
  GTK_ACCESSIBLE_RELATION_CHANGE_COL_INDEX_TEXT    = 1 << GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT,
  GTK_ACCESSIBLE_RELATION_CHANGE_COL_SPAN          = 1 << GTK_ACCESSIBLE_RELATION_COL_SPAN,
  GTK_ACCESSIBLE_RELATION_CHANGE_CONTROLS          = 1 << GTK_ACCESSIBLE_RELATION_CONTROLS,
  GTK_ACCESSIBLE_RELATION_CHANGE_DESCRIBED_BY      = 1 << GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
  GTK_ACCESSIBLE_RELATION_CHANGE_DETAILS           = 1 << GTK_ACCESSIBLE_RELATION_DETAILS,
  GTK_ACCESSIBLE_RELATION_CHANGE_ERROR_MESSAGE     = 1 << GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE,
  GTK_ACCESSIBLE_RELATION_CHANGE_FLOW_TO           = 1 << GTK_ACCESSIBLE_RELATION_FLOW_TO,
  GTK_ACCESSIBLE_RELATION_CHANGE_LABELLED_BY       = 1 << GTK_ACCESSIBLE_RELATION_LABELLED_BY,
  GTK_ACCESSIBLE_RELATION_CHANGE_OWNS              = 1 << GTK_ACCESSIBLE_RELATION_OWNS,
  GTK_ACCESSIBLE_RELATION_CHANGE_POS_IN_SET        = 1 << GTK_ACCESSIBLE_RELATION_POS_IN_SET,
  GTK_ACCESSIBLE_RELATION_CHANGE_ROW_COUNT         = 1 << GTK_ACCESSIBLE_RELATION_ROW_COUNT,
  GTK_ACCESSIBLE_RELATION_CHANGE_ROW_INDEX         = 1 << GTK_ACCESSIBLE_RELATION_ROW_INDEX,
  GTK_ACCESSIBLE_RELATION_CHANGE_ROW_INDEX_TEXT    = 1 << GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT,
  GTK_ACCESSIBLE_RELATION_CHANGE_ROW_SPAN          = 1 << GTK_ACCESSIBLE_RELATION_ROW_SPAN,
  GTK_ACCESSIBLE_RELATION_CHANGE_SET_SIZE          = 1 << GTK_ACCESSIBLE_RELATION_SET_SIZE
} GtkAccessibleRelationChange;

typedef enum {
  GTK_ACCESSIBLE_STATE_CHANGE_BUSY     = 1 << GTK_ACCESSIBLE_STATE_BUSY,
  GTK_ACCESSIBLE_STATE_CHANGE_CHECKED  = 1 << GTK_ACCESSIBLE_STATE_CHECKED,
  GTK_ACCESSIBLE_STATE_CHANGE_DISABLED = 1 << GTK_ACCESSIBLE_STATE_DISABLED,
  GTK_ACCESSIBLE_STATE_CHANGE_EXPANDED = 1 << GTK_ACCESSIBLE_STATE_EXPANDED,
  GTK_ACCESSIBLE_STATE_CHANGE_HIDDEN   = 1 << GTK_ACCESSIBLE_STATE_HIDDEN,
  GTK_ACCESSIBLE_STATE_CHANGE_INVALID  = 1 << GTK_ACCESSIBLE_STATE_INVALID,
  GTK_ACCESSIBLE_STATE_CHANGE_PRESSED  = 1 << GTK_ACCESSIBLE_STATE_PRESSED,
  GTK_ACCESSIBLE_STATE_CHANGE_SELECTED = 1 << GTK_ACCESSIBLE_STATE_SELECTED,
  GTK_ACCESSIBLE_STATE_CHANGE_VISITED = 1 << GTK_ACCESSIBLE_STATE_VISITED
} GtkAccessibleStateChange;

struct _GtkATContext
{
  GObject parent_instance;

  GtkAccessibleRole accessible_role;
  GtkAccessible *accessible;
  GtkAccessible *accessible_parent;
  GtkAccessible *next_accessible_sibling;
  GdkDisplay *display;

  GtkAccessibleAttributeSet *states;
  GtkAccessibleAttributeSet *properties;
  GtkAccessibleAttributeSet *relations;

  GtkAccessibleStateChange updated_states;
  GtkAccessiblePropertyChange updated_properties;
  GtkAccessibleRelationChange updated_relations;
  GtkAccessiblePlatformChange updated_platform;

  guint realized : 1;
};

struct _GtkATContextClass
{
  GObjectClass parent_class;

  void (* state_change) (GtkATContext                *self,
                         GtkAccessibleStateChange     changed_states,
                         GtkAccessiblePropertyChange  changed_properties,
                         GtkAccessibleRelationChange  changed_relations,
                         GtkAccessibleAttributeSet   *states,
                         GtkAccessibleAttributeSet   *properties,
                         GtkAccessibleAttributeSet   *relations);

  void (* platform_change) (GtkATContext                *self,
                            GtkAccessiblePlatformChange  changed_platform);

  void (* bounds_change) (GtkATContext                *self);

  void (* child_change) (GtkATContext             *self,
                         GtkAccessibleChildChange  changed_child,
                         GtkAccessible            *child);

  void (* realize)       (GtkATContext *self);
  void (* unrealize)     (GtkATContext *self);

  void (* announce)      (GtkATContext *self,
                          const char   *message,
                          GtkAccessibleAnnouncementPriority priority);

  /* Text interface */
  void (* update_caret_position) (GtkATContext *self);
  void (* update_selection_bound) (GtkATContext *self);
  void (* update_text_contents) (GtkATContext *self,
                                 GtkAccessibleTextContentChange change,
                                 unsigned int start,
                                 unsigned int end);
};

GtkATContext *          gtk_at_context_clone                    (GtkATContext          *self,
                                                                 GtkAccessibleRole      role,
                                                                 GtkAccessible         *accessible,
                                                                 GdkDisplay            *display);

void                    gtk_at_context_set_display              (GtkATContext          *self,
                                                                 GdkDisplay            *display);
GdkDisplay *            gtk_at_context_get_display              (GtkATContext          *self);
void                    gtk_at_context_set_accessible_role      (GtkATContext          *self,
                                                                 GtkAccessibleRole      role);

void                    gtk_at_context_realize                  (GtkATContext          *self);
void                    gtk_at_context_unrealize                (GtkATContext          *self);
gboolean                gtk_at_context_is_realized              (GtkATContext          *self);

void                    gtk_at_context_update                   (GtkATContext          *self);

void                    gtk_at_context_set_accessible_state     (GtkATContext          *self,
                                                                 GtkAccessibleState     state,
                                                                 GtkAccessibleValue    *value);
gboolean                gtk_at_context_has_accessible_state     (GtkATContext          *self,
                                                                 GtkAccessibleState     state);
GtkAccessibleValue *    gtk_at_context_get_accessible_state     (GtkATContext          *self,
                                                                 GtkAccessibleState     state);
void                    gtk_at_context_set_accessible_property  (GtkATContext          *self,
                                                                 GtkAccessibleProperty  property,
                                                                 GtkAccessibleValue    *value);
gboolean                gtk_at_context_has_accessible_property  (GtkATContext          *self,
                                                                 GtkAccessibleProperty  property);
GtkAccessibleValue *    gtk_at_context_get_accessible_property  (GtkATContext          *self,
                                                                 GtkAccessibleProperty  property);
void                    gtk_at_context_set_accessible_relation  (GtkATContext          *self,
                                                                 GtkAccessibleRelation  property,
                                                                 GtkAccessibleValue    *value);
gboolean                gtk_at_context_has_accessible_relation  (GtkATContext          *self,
                                                                 GtkAccessibleRelation  relation);
GtkAccessibleValue *    gtk_at_context_get_accessible_relation  (GtkATContext          *self,
                                                                 GtkAccessibleRelation  relation);

char *                  gtk_at_context_get_name                 (GtkATContext          *self);
char *                  gtk_at_context_get_description          (GtkATContext          *self);

void                    gtk_at_context_platform_changed         (GtkATContext                *self,
                                                                 GtkAccessiblePlatformChange  change);
void                    gtk_at_context_bounds_changed           (GtkATContext                *self);
void                    gtk_at_context_child_changed            (GtkATContext                *self,
                                                                 GtkAccessibleChildChange     change,
                                                                 GtkAccessible               *child);

const char *    gtk_accessible_property_get_attribute_name      (GtkAccessibleProperty property);
const char *    gtk_accessible_relation_get_attribute_name      (GtkAccessibleRelation relation);
const char *    gtk_accessible_state_get_attribute_name         (GtkAccessibleState    state);

GtkAccessible *
gtk_at_context_get_accessible_parent (GtkATContext *self);
void
gtk_at_context_set_accessible_parent (GtkATContext *self,
                                      GtkAccessible *parent);
GtkAccessible *
gtk_at_context_get_next_accessible_sibling (GtkATContext *self);
void
gtk_at_context_set_next_accessible_sibling (GtkATContext *self,
                                            GtkAccessible *sibling);

void gtk_at_context_announce (GtkATContext                     *self,
                              const char                       *message,
                              GtkAccessibleAnnouncementPriority priority);
void
gtk_at_context_update_caret_position (GtkATContext *self);
void
gtk_at_context_update_selection_bound (GtkATContext *self);
void
gtk_at_context_update_text_contents (GtkATContext *self,
                                     GtkAccessibleTextContentChange change,
                                     unsigned int start,
                                     unsigned int end);

G_END_DECLS
