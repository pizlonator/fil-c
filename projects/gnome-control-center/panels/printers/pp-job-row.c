/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <config.h>
#include <glib/gi18n.h>

#include "pp-job-row.h"
#include "cc-printers-resources.h"

struct _PpJobRow
{
  GtkListBoxRow parent;

  GtkButton *pause_button;
  GtkButton *priority_button;
  GtkLabel  *state_label;
  GtkLabel  *title_label;

  PpJob *job;
};

G_DEFINE_TYPE (PpJobRow, pp_job_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PRIORITY_CHANGED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
update_pause_button (PpJobRow *self, gboolean paused)
{
  gtk_button_set_icon_name (self->pause_button,
                            paused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->pause_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  paused ? _("Resume") : _("Pause"),
                                  -1);
}

static void
pause_cb (PpJobRow *self)
{
  pp_job_set_hold_until_async (self->job, pp_job_get_state (self->job) == IPP_JOB_HELD ? "no-hold" : "indefinite");
  update_pause_button (self,
                       pp_job_get_state (self->job) == IPP_JOB_HELD);
                                              }

static void
stop_cb (PpJobRow *self)
{
  pp_job_cancel_purge_async (self->job, FALSE);
}

static void
priority_cb (PpJobRow *self)
{
  g_signal_emit_by_name (self, "priority-changed");
}

static void
pp_job_row_dispose (GObject *object)
{
  PpJobRow *self = PP_JOB_ROW (object);

  g_clear_object (&self->job);

  G_OBJECT_CLASS (pp_job_row_parent_class)->dispose (object);
}

static void
pp_job_row_class_init (PpJobRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pp_job_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/pp-job-row.ui");

  gtk_widget_class_bind_template_child (widget_class, PpJobRow, pause_button);
  gtk_widget_class_bind_template_child (widget_class, PpJobRow, priority_button);
  gtk_widget_class_bind_template_child (widget_class, PpJobRow, state_label);
  gtk_widget_class_bind_template_child (widget_class, PpJobRow, title_label);

  gtk_widget_class_bind_template_callback (widget_class, pause_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_cb);
  gtk_widget_class_bind_template_callback (widget_class, priority_cb);

  signals[PRIORITY_CHANGED] =
    g_signal_new ("priority-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
pp_job_row_init (PpJobRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

PpJob *
pp_job_row_get_job (PpJobRow *self)
{
  return self->job;
}

PpJobRow *
pp_job_row_new (PpJob *job)
{
  PpJobRow *self;
  gboolean  status;
  g_autofree gchar *state_string = NULL;

  self = g_object_new (PP_TYPE_JOB_ROW, NULL);

  self->job = g_object_ref (job);

  switch (pp_job_get_state (job))
    {
      case IPP_JOB_PENDING:
        /* Translators: Job's state (job is waiting to be printed) */
        state_string = g_strdup (C_("print job", "Pending"));
        break;
      case IPP_JOB_HELD:
        if (pp_job_get_auth_info_required (job) == NULL)
          {
            /* Translators: Job's state (job is held for printing) */
            state_string = g_strdup (C_("print job", "Paused"));
          }
        else
          {
            /* Translators: Job's state (job needs authentication to proceed further) */
            state_string = g_strdup_printf ("<span foreground=\"#ff0000\">%s</span>", C_("print job", "Authentication required"));
          }
        break;
      case IPP_JOB_PROCESSING:
        /* Translators: Job's state (job is currently printing) */
        state_string = g_strdup (C_("print job", "Processing"));
        break;
      case IPP_JOB_STOPPED:
        /* Translators: Job's state (job has been stopped) */
        state_string = g_strdup (C_("print job", "Stopped"));
        break;
      case IPP_JOB_CANCELED:
        /* Translators: Job's state (job has been canceled) */
        state_string = g_strdup (C_("print job", "Canceled"));
        break;
      case IPP_JOB_ABORTED:
        /* Translators: Job's state (job has aborted due to error) */
        state_string = g_strdup (C_("print job", "Aborted"));
        break;
      case IPP_JOB_COMPLETED:
        /* Translators: Job's state (job has completed successfully) */
        state_string = g_strdup (C_("print job", "Completed"));
        break;
    }
  gtk_label_set_text (self->title_label, pp_job_get_title (job));
  gtk_label_set_markup (self->state_label, state_string);
  gtk_widget_set_sensitive (GTK_WIDGET (self->pause_button), pp_job_get_auth_info_required (job) == NULL);
  status = pp_job_priority_get_sensitive (job);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priority_button), status);
  if (status)
    /* Translators: Clicking this button prioritizes printing of this print job */
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->priority_button), _("Move this job to the top of the queue"));
  update_pause_button (self,
                       pp_job_get_state (self->job) == IPP_JOB_HELD);
  return self;
}
