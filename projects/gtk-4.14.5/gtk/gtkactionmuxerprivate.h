/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include <gio/gio.h>
#include "gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_ACTION_MUXER                               (gtk_action_muxer_get_type ())
#define GTK_ACTION_MUXER(inst)                              (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER, GtkActionMuxer))
#define GTK_IS_ACTION_MUXER(inst)                           (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             GTK_TYPE_ACTION_MUXER))

typedef struct _GtkWidgetAction GtkWidgetAction;
typedef struct _GtkActionMuxer GtkActionMuxer;

struct _GtkWidgetAction
{
  GtkWidgetAction *next;

  char *name;
  GType owner;

  const GVariantType *parameter_type;
  GtkWidgetActionActivateFunc activate;

  const GVariantType *state_type;
  GParamSpec *pspec;
};

GType                   gtk_action_muxer_get_type                       (void);
GtkActionMuxer *        gtk_action_muxer_new                            (GtkWidget      *widget);

void                    gtk_action_muxer_insert                         (GtkActionMuxer *muxer,
                                                                         const char     *prefix,
                                                                         GActionGroup   *action_group);

void                    gtk_action_muxer_remove                         (GtkActionMuxer *muxer,
                                                                         const char     *prefix);
GActionGroup *          gtk_action_muxer_find                           (GtkActionMuxer *muxer,
                                                                         const char     *action_name,
                                                                         const char    **unprefixed_name);
GActionGroup *          gtk_action_muxer_get_group                      (GtkActionMuxer *muxer,
                                                                         const char     *group_name);
GtkActionMuxer *        gtk_action_muxer_get_parent                     (GtkActionMuxer *muxer);

void                    gtk_action_muxer_set_parent                     (GtkActionMuxer *muxer,
                                                                         GtkActionMuxer *parent);

/* GActionGroup equivalent api */
gboolean                gtk_action_muxer_query_action                   (GtkActionMuxer      *muxer,
                                                                         const char          *action_name,
                                                                         gboolean            *enabled,
                                                                         const GVariantType **parameter_type,
                                                                         const GVariantType **state_type,
                                                                         GVariant           **state_hint,
                                                                         GVariant           **state) G_GNUC_WARN_UNUSED_RESULT;
void                    gtk_action_muxer_activate_action                (GtkActionMuxer      *muxer,
                                                                         const char          *action_name,
                                                                         GVariant            *parameter);
void                    gtk_action_muxer_change_action_state            (GtkActionMuxer      *muxer,
                                                                         const char          *action_name,
                                                                         GVariant            *state);
gboolean                gtk_action_muxer_has_action                     (GtkActionMuxer      *muxer,
                                                                         const char          *action_name);
char **                 gtk_action_muxer_list_actions                   (GtkActionMuxer      *muxer,
                                                                         gboolean             local_only);

/* api for class actions */
void                    gtk_action_muxer_action_enabled_changed         (GtkActionMuxer      *muxer,
                                                                         const char          *action_name,
                                                                         gboolean             enabled);
void                    gtk_action_muxer_action_state_changed           (GtkActionMuxer      *muxer,
                                                                         const char          *action_name,
                                                                         GVariant            *state);
void                    gtk_action_muxer_connect_class_actions          (GtkActionMuxer      *muxer);

/* api for accels */
void                    gtk_action_muxer_set_primary_accel              (GtkActionMuxer      *muxer,
                                                                         const char          *action_and_target,
                                                                         const char          *primary_accel);
const char *            gtk_action_muxer_get_primary_accel              (GtkActionMuxer      *muxer,
                                                                         const char          *action_and_target);

/* No better place for these... */
char *                  gtk_print_action_and_target                     (const char          *action_namespace,
                                                                         const char          *action_name,
                                                                         GVariant            *target);
char *                  gtk_normalise_detailed_action_name              (const char          *detailed_action_name);

G_END_DECLS

