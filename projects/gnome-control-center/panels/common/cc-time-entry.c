/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-time-entry.c
 *
 * Copyright 2020 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-time-entry"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-time-entry.h"

#define SEPARATOR_INDEX      2
#define END_INDEX            4
#define EMIT_CHANGED_TIMEOUT 100


struct _CcTimeEntry
{
  GtkWidget  parent_instance;

  GtkWidget   *text;

  guint      insert_text_id;
  guint      time_changed_id;
  int        hour; /* Range: 0-23 in 24H and 1-12 in 12H with is_am set/unset */
  int        minute;
  gboolean   is_am_pm;
  gboolean   is_am; /* AM if TRUE. PM if FALSE. valid iff is_am_pm set */
};


static void editable_insert_text_cb (GtkText     *text,
                                     char        *new_text,
                                     gint         new_text_length,
                                     gint        *position,
                                     CcTimeEntry *self);

static void gtk_editable_interface_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CcTimeEntry, cc_time_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_editable_interface_init));

enum {
  CHANGE_VALUE,
  TIME_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
emit_time_changed (CcTimeEntry *self)
{
  self->time_changed_id = 0;

  g_signal_emit (self, signals[TIME_CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
time_entry_fill_time (CcTimeEntry *self)
{
  g_autofree gchar *str = NULL;

  g_assert (CC_IS_TIME_ENTRY (self));

  str = g_strdup_printf ("%02d∶%02d", self->hour, self->minute);

  g_signal_handlers_block_by_func (self->text, editable_insert_text_cb, self);
  gtk_editable_set_text (GTK_EDITABLE (self->text), str);
  g_signal_handlers_unblock_by_func (self->text, editable_insert_text_cb, self);
}

static void
cursor_position_changed_cb (CcTimeEntry *self)
{
  int current_pos;

  g_assert (CC_IS_TIME_ENTRY (self));

  current_pos = gtk_editable_get_position (GTK_EDITABLE (self));

  g_signal_handlers_block_by_func (self->text, cursor_position_changed_cb, self);

  /* If cursor is on ‘:’ move to the next field */
  if (current_pos == SEPARATOR_INDEX)
    gtk_editable_set_position (GTK_EDITABLE (self->text), current_pos + 1);

  /* If cursor is after the last digit and without selection, move to last digit */
  if (current_pos > END_INDEX &&
      !gtk_editable_get_selection_bounds (GTK_EDITABLE (self->text), NULL, NULL))
    gtk_editable_set_position (GTK_EDITABLE (self->text), END_INDEX);

  g_signal_handlers_unblock_by_func (self->text, cursor_position_changed_cb, self);
}

static void
entry_selection_changed_cb (CcTimeEntry *self)
{
  GtkEditable *editable;

  g_assert (CC_IS_TIME_ENTRY (self));

  editable = GTK_EDITABLE (self->text);

  g_signal_handlers_block_by_func (self->text, cursor_position_changed_cb, self);

  /* If cursor is after the last digit and without selection, move to last digit */
  if (gtk_editable_get_position (editable) > END_INDEX &&
      !gtk_editable_get_selection_bounds (editable, NULL, NULL))
    gtk_editable_set_position (editable, END_INDEX);

  g_signal_handlers_unblock_by_func (self->text, cursor_position_changed_cb, self);
}

static void
editable_insert_text_cb (GtkText     *text,
                         char        *new_text,
                         gint         new_text_length,
                         gint        *position,
                         CcTimeEntry *self)
{
  g_assert (CC_IS_TIME_ENTRY (self));

  if (new_text_length == -1)
    new_text_length = strlen (new_text);

  if (new_text_length == 5)
    {
      const gchar *text = gtk_editable_get_text (GTK_EDITABLE (self));
      guint16 text_length;

      text_length = g_utf8_strlen (text, -1);

      /* Return if the text matches XX:XX template (where X is a number) */
      if (text_length == 0 &&
          strstr (new_text, "0123456789:") == new_text + new_text_length &&
          strchr (new_text, ':') == strrchr (new_text, ':'))
        return;
    }

  /* Insert text if single digit number */
  if (new_text_length == 1 &&
      strspn (new_text, "0123456789"))
    {
      int pos, number;

      pos = *position;
      number = *new_text - '0';

      if (pos == 0)
        self->hour = self->hour % 10 + number * 10;
      else if (pos == 1)
        self->hour = self->hour / 10 * 10 + number;
      else if (pos == 3)
        self->minute = self->minute % 10 + number * 10;
      else if (pos == 4)
        self->minute = self->minute / 10 * 10 + number;

      if (self->is_am_pm)
        self->hour = CLAMP (self->hour, 1, 12);
      else
        self->hour = CLAMP (self->hour, 0, 23);

      self->minute = CLAMP (self->minute, 0, 59);

      g_signal_stop_emission_by_name (text, "insert-text");
      time_entry_fill_time (self);
      *position = pos + 1;

      g_clear_handle_id (&self->time_changed_id, g_source_remove);
      self->time_changed_id = g_timeout_add (EMIT_CHANGED_TIMEOUT,
                                             (GSourceFunc)emit_time_changed, self);
      return;
    }

  /* Warn otherwise */
  g_signal_stop_emission_by_name (text, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (self));
}


static gboolean
change_value_cb (GtkWidget *widget,
                 GVariant  *arguments,
                 gpointer   user_data)
{
  CcTimeEntry *self = CC_TIME_ENTRY (widget);
  GtkScrollType type;
  int position;

  g_assert (CC_IS_TIME_ENTRY (self));

  type = g_variant_get_int32 (arguments);
  position = gtk_editable_get_position (GTK_EDITABLE (self));

  if (position > SEPARATOR_INDEX)
    {
      if (type == GTK_SCROLL_STEP_UP)
        self->minute++;
      else
        self->minute--;

      if (self->minute >= 60)
        self->minute = 0;
      else if (self->minute <= -1)
        self->minute = 59;
    }
  else
    {
      if (type == GTK_SCROLL_STEP_UP)
        self->hour++;
      else
        self->hour--;

      if (self->is_am_pm)
        {
          if (self->hour > 12)
            self->hour = 1;
          else if (self->hour < 1)
            self->hour = 12;
        }
      else
        {
          if (self->hour >= 24)
            self->hour = 0;
          else if (self->hour <= -1)
            self->hour = 23;
        }
    }

  time_entry_fill_time (self);
  gtk_editable_set_position (GTK_EDITABLE (self), position);

  g_clear_handle_id (&self->time_changed_id, g_source_remove);
  self->time_changed_id = g_timeout_add (EMIT_CHANGED_TIMEOUT,
                                         (GSourceFunc)emit_time_changed, self);

  return GDK_EVENT_STOP;
}

static void
value_changed_cb (CcTimeEntry   *self,
                  GtkScrollType  type)
{
  g_autoptr(GVariant) value;

  g_assert (CC_IS_TIME_ENTRY (self));

  value = g_variant_new_int32 (type);

  change_value_cb (GTK_WIDGET (self), value, NULL);
}

static void
on_text_cut_clipboard_cb (GtkText     *text,
                          CcTimeEntry *self)
{
  gtk_widget_error_bell (GTK_WIDGET (self));
  g_signal_stop_emission_by_name (text, "cut-clipboard");
}

static void
on_text_delete_from_cursor_cb (GtkText       *text,
                               GtkDeleteType *type,
                               gint           count,
                               CcTimeEntry   *self)
{
  gtk_widget_error_bell (GTK_WIDGET (self));
  g_signal_stop_emission_by_name (text, "delete-from-cursor");
}

static void
on_text_move_cursor_cb (GtkText         *text,
                        GtkMovementStep  step,
                        gint             count,
                        gboolean         extend,
                        CcTimeEntry     *self)
{
  int current_pos;

  current_pos = gtk_editable_get_position (GTK_EDITABLE (self));

  /* If cursor is on ‘:’ move backward/forward depending on the current movement */
  if ((step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
       step == GTK_MOVEMENT_VISUAL_POSITIONS) &&
      current_pos + count == SEPARATOR_INDEX)
    count > 0 ? count++ : count--;

  g_signal_handlers_block_by_func (text, on_text_move_cursor_cb, self);
  gtk_editable_set_position (GTK_EDITABLE (text), current_pos + count);
  g_signal_handlers_unblock_by_func (text, on_text_move_cursor_cb, self);

  g_signal_stop_emission_by_name (text, "move-cursor");
}

static void
on_text_paste_clipboard_cb (GtkText     *text,
                            CcTimeEntry *self)
{
  gtk_widget_error_bell (GTK_WIDGET (self));
  g_signal_stop_emission_by_name (text, "paste-clipboard");
}

static void
on_text_toggle_overwrite_cb (GtkText     *text,
                             CcTimeEntry *self)
{
  gtk_widget_error_bell (GTK_WIDGET (self));
  g_signal_stop_emission_by_name (text, "toggle-overwrite");
}

static gboolean
on_key_pressed_cb (CcTimeEntry           *self,
                   guint                  keyval,
                   guint                  keycode,
                   GdkModifierType        state)
{
  /* Allow entering numbers */
  if (!(state & GDK_SHIFT_MASK) &&
      ((keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9) ||
       (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)))
    return GDK_EVENT_PROPAGATE;

  /* Allow navigation keys */
  if ((keyval >= GDK_KEY_Left && keyval <= GDK_KEY_Down) ||
      (keyval >= GDK_KEY_KP_Left && keyval <= GDK_KEY_KP_Down) ||
      keyval == GDK_KEY_Home ||
      keyval == GDK_KEY_End ||
      keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  if (state & (GDK_CONTROL_MASK | GDK_ALT_MASK))
    return GDK_EVENT_PROPAGATE;

  if (keyval == GDK_KEY_Tab)
    {
      /* If focus is on Hour field skip to minute field */
      if (gtk_editable_get_position (GTK_EDITABLE (self)) <= 1)
        {
          gtk_editable_set_position (GTK_EDITABLE (self), SEPARATOR_INDEX + 1);

          return GDK_EVENT_STOP;
        }

      return GDK_EVENT_PROPAGATE;
    }

  /* Shift-Tab */
  if (keyval == GDK_KEY_ISO_Left_Tab)
    {
      /* If focus is on Minute field skip back to Hour field */
      if (gtk_editable_get_position (GTK_EDITABLE (self)) >= 2)
        {
          gtk_editable_set_position (GTK_EDITABLE (self), 0);

          return GDK_EVENT_STOP;
        }

      return GDK_EVENT_PROPAGATE;
    }

  return GDK_EVENT_STOP;
}

static GtkEditable *
cc_time_entry_get_delegate (GtkEditable *editable)
{
  CcTimeEntry *self = CC_TIME_ENTRY (editable);
  return GTK_EDITABLE (self->text);
}

static void
gtk_editable_interface_init (GtkEditableInterface *iface)
{
  iface->get_delegate = cc_time_entry_get_delegate;
}

static void
cc_time_entry_constructed (GObject *object)
{
  CcTimeEntry *self = CC_TIME_ENTRY (object);
  PangoAttrList *list;
  PangoAttribute *attribute;

  G_OBJECT_CLASS (cc_time_entry_parent_class)->constructed (object);

  gtk_widget_set_direction (GTK_WIDGET (self->text), GTK_TEXT_DIR_LTR);
  time_entry_fill_time (CC_TIME_ENTRY (object));

  list = pango_attr_list_new ();

  attribute = pango_attr_size_new (PANGO_SCALE * 32);
  pango_attr_list_insert (list, attribute);

  attribute = pango_attr_weight_new (PANGO_WEIGHT_LIGHT);
  pango_attr_list_insert (list, attribute);

  /* Use tabular(monospace) letters */
  attribute = pango_attr_font_features_new ("tnum");
  pango_attr_list_insert (list, attribute);

  gtk_text_set_attributes (GTK_TEXT (self->text), list);

  pango_attr_list_unref (list);
}

static void
cc_time_entry_dispose (GObject *object)
{
  CcTimeEntry *self = CC_TIME_ENTRY (object);

  gtk_editable_finish_delegate (GTK_EDITABLE (self));
  g_clear_pointer (&self->text, gtk_widget_unparent);

  G_OBJECT_CLASS (cc_time_entry_parent_class)->dispose (object);
}

static void
cc_time_entry_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  if (gtk_editable_delegate_get_property (object, property_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_time_entry_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  if (gtk_editable_delegate_set_property (object, property_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_time_entry_class_init (CcTimeEntryClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_time_entry_constructed;
  object_class->dispose = cc_time_entry_dispose;
  object_class->get_property = cc_time_entry_get_property;
  object_class->set_property = cc_time_entry_set_property;

  signals[CHANGE_VALUE] =
    g_signal_new ("change-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  signals[TIME_CHANGED] =
    g_signal_new ("time-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_editable_install_properties (object_class, 1);

  gtk_widget_class_set_css_name (widget_class, "entry");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Up, 0,
                                change_value_cb, "i", GTK_SCROLL_STEP_UP);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Up, 0,
                                change_value_cb, "i", GTK_SCROLL_STEP_UP);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Down, 0,
                                change_value_cb, "i", GTK_SCROLL_STEP_DOWN);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Down, 0,
                                change_value_cb, "i", GTK_SCROLL_STEP_DOWN);
}

static void
cc_time_entry_init (CcTimeEntry *self)
{
  GtkEventController *key_controller;

  key_controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (key_controller, GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (key_controller, "key-pressed", G_CALLBACK (on_key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), key_controller);

  self->text = g_object_new (GTK_TYPE_TEXT,
                             "input-purpose", GTK_INPUT_PURPOSE_DIGITS,
                             "input-hints", GTK_INPUT_HINT_NO_EMOJI,
                             "overwrite-mode", TRUE,
                             "xalign", 0.5,
                             "max-length", 5,
                             NULL);
  gtk_widget_set_parent (self->text, GTK_WIDGET (self));
  gtk_editable_init_delegate (GTK_EDITABLE (self));
  g_object_connect (self->text,
                    "signal::cut-clipboard", on_text_cut_clipboard_cb, self,
                    "signal::delete-from-cursor", on_text_delete_from_cursor_cb, self,
                    "signal::insert-text", editable_insert_text_cb, self,
                    "signal::move-cursor", on_text_move_cursor_cb, self,
                    "swapped-signal::notify::cursor-position", cursor_position_changed_cb, self,
                    "swapped-signal::notify::selection-bound", entry_selection_changed_cb, self,
                    "signal::paste-clipboard", on_text_paste_clipboard_cb, self,
                    "signal::toggle-overwrite", on_text_toggle_overwrite_cb, self,
                    NULL);
  g_signal_connect (self, "change-value",
                    G_CALLBACK (value_changed_cb), self);
}

GtkWidget *
cc_time_entry_new (void)
{
  return g_object_new (CC_TYPE_TIME_ENTRY, NULL);
}

void
cc_time_entry_set_time (CcTimeEntry *self,
                        guint        hour,
                        guint        minute)
{
  gboolean is_am_pm;

  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  if (cc_time_entry_get_hour (self) == hour &&
      cc_time_entry_get_minute (self) == minute)
    return;

  is_am_pm = cc_time_entry_get_am_pm (self);
  cc_time_entry_set_am_pm (self, FALSE);

  self->hour = CLAMP (hour, 0, 23);
  self->minute = CLAMP (minute, 0, 59);

  cc_time_entry_set_am_pm (self, is_am_pm);

  g_signal_emit (self, signals[TIME_CHANGED], 0);
  time_entry_fill_time (self);
}

guint
cc_time_entry_get_hour (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), 0);

  if (!self->is_am_pm)
    return self->hour;

  if (self->is_am && self->hour == 12)
    return 0;
  else if (self->is_am || self->hour == 12)
    return self->hour;
  else
    return self->hour + 12;
}

guint
cc_time_entry_get_minute (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), 0);

  return self->minute;
}

gboolean
cc_time_entry_get_is_am (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), FALSE);

  if (self->is_am_pm)
    return self->is_am;

  return self->hour < 12;
}

void
cc_time_entry_set_is_am (CcTimeEntry *self,
                         gboolean     is_am)
{
  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  self->is_am = !!is_am;
  g_signal_emit (self, signals[TIME_CHANGED], 0);
}

gboolean
cc_time_entry_get_am_pm (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), FALSE);

  return self->is_am_pm;
}

void
cc_time_entry_set_am_pm (CcTimeEntry *self,
                         gboolean     is_am_pm)
{
  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  if (self->is_am_pm == !!is_am_pm)
    return;

  if (self->hour < 12)
    self->is_am = TRUE;
  else
    self->is_am = FALSE;

  if (is_am_pm)
    {
      if (self->hour == 0)
        self->hour = 12;
      else if (self->hour > 12)
        self->hour = self->hour - 12;
    }
  else
    {
      if (self->hour == 12 && self->is_am)
        self->hour = 0;
      else if (!self->is_am)
        self->hour = self->hour + 12;
    }

  self->is_am_pm = !!is_am_pm;
  time_entry_fill_time (self);
}
