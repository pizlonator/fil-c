/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "pp-ppd-option-widget.h"
#include "pp-utils.h"

static void pp_ppd_option_widget_finalize (GObject *object);

static gboolean construct_widget   (PpPPDOptionWidget *self);
static void     update_widget      (PpPPDOptionWidget *self);
static void     update_widget_real (PpPPDOptionWidget *self);

struct _PpPPDOptionWidget
{
  GtkBox parent_instance;

  GtkWidget *switch_button;
  GtkWidget *dropdown;
  GtkWidget *image;
  GtkWidget *box;

  ppd_option_t *option;

  gchar *printer_name;
  gchar *option_name;

  cups_dest_t *destination;
  gboolean     destination_set;

  gchar    *ppd_filename;
  gboolean  ppd_filename_set;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (PpPPDOptionWidget, pp_ppd_option_widget, GTK_TYPE_BOX)

/* This list comes from Gtk+ */
static const struct {
  const char *keyword;
  const char *choice;
  const char *translation;
} ppd_choice_translations[] = {
  { "Duplex", "None", N_("One Sided") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexNoTumble", N_("Long Edge (Standard)") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexTumble", N_("Short Edge (Flip)") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Auto", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "AutoSelect", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Default", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "None", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "PrinterDefault", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Unspecified", N_("Auto Select") },
  /* Translators: this is an option of "Resolution" */
  { "Resolution", "default", N_("Printer Default") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "EmbedFonts", N_("Embed GhostScript fonts only") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level1", N_("Convert to PS level 1") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level2", N_("Convert to PS level 2") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "No", N_("No pre-filtering") },
};

static ppd_option_t *
cups_option_copy (ppd_option_t *option)
{
  ppd_option_t *result;
  gint          i;

  result = g_new0 (ppd_option_t, 1);

  *result = *option;

  result->choices = g_new (ppd_choice_t, result->num_choices);
  for (i = 0; i < result->num_choices; i++)
    {
      result->choices[i] = option->choices[i];
      result->choices[i].code = g_strdup (option->choices[i].code);
      result->choices[i].option = result;
    }

  return result;
}

static void
cups_option_free (ppd_option_t *option)
{
  gint i;

  if (option)
    {
      for (i = 0; i < option->num_choices; i++)
        g_free (option->choices[i].code);

      g_free (option->choices);
      g_free (option);
    }
}

static void
pp_ppd_option_widget_class_init (PpPPDOptionWidgetClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->finalize = pp_ppd_option_widget_finalize;
}

static void
pp_ppd_option_widget_init (PpPPDOptionWidget *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);
}

static void
pp_ppd_option_widget_finalize (GObject *object)
{
  PpPPDOptionWidget *self = PP_PPD_OPTION_WIDGET (object);

  g_cancellable_cancel (self->cancellable);
  if (self->ppd_filename)
    g_unlink (self->ppd_filename);

  g_clear_pointer (&self->option, cups_option_free);
  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->option_name, g_free);
  if (self->destination)
    {
      cupsFreeDests (1, self->destination);
      self->destination = NULL;
    }
  g_clear_pointer (&self->ppd_filename, g_free);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (pp_ppd_option_widget_parent_class)->finalize (object);
}

static const gchar *
ppd_choice_translate (ppd_choice_t *choice)
{
  const gchar *keyword = choice->option->keyword;
  gint         i;

  for (i = 0; i < G_N_ELEMENTS (ppd_choice_translations); i++)
    {
      if (g_strcmp0 (ppd_choice_translations[i].keyword, keyword) == 0 &&
          g_strcmp0 (ppd_choice_translations[i].choice, choice->choice) == 0)
        {
          return _(ppd_choice_translations[i].translation);
        }
    }

  return choice->text;
}

GtkWidget *
pp_ppd_option_widget_new (ppd_option_t *option,
                          const gchar  *printer_name)
{
  PpPPDOptionWidget *self = NULL;

  if (option && printer_name)
    {
      self = g_object_new (PP_TYPE_PPD_OPTION_WIDGET, NULL);

      self->printer_name = g_strdup (printer_name);
      self->option = cups_option_copy (option);
      self->option_name = g_strdup (option->keyword);

      if (construct_widget (self))
        {
          update_widget_real (self);
        }
      else
        {
          g_object_ref_sink (self);
          g_object_unref (self);
          self = NULL;
        }
    }

  return (GtkWidget *) self;
}

static GtkWidget *
dropdown_new (void)
{
  GtkStringList *store = NULL;
  GtkWidget     *dropdown;

  store = gtk_string_list_new (NULL);

  dropdown = gtk_drop_down_new (G_LIST_MODEL (store), NULL);

  return dropdown;
}

static void
dropdown_append (GtkWidget   *dropdown,
                 const gchar *display_text)
{
  GtkStringList *store;

  store = GTK_STRING_LIST (gtk_drop_down_get_model (GTK_DROP_DOWN (dropdown)));

  gtk_string_list_append (store, display_text);
}

static void
dropdown_set (GtkWidget    *dropdown,
              ppd_option_t *option,
              const gchar  *value)
{
  for (guint i = 0; i < option->num_choices; i++)
    {
      if (g_strcmp0 (option->choices[i].choice, value) == 0)
        {
          gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), i);
          break;
        }
    }
}

static char *
dropdown_get (GtkWidget    *dropdown,
              ppd_option_t *option)
{
  guint          selected_item;
  gchar         *value = NULL;

  selected_item = gtk_drop_down_get_selected (GTK_DROP_DOWN (dropdown));

  if (selected_item != GTK_INVALID_LIST_POSITION)
    {
      value = option->choices[selected_item].choice;
    }

  return value;
}

static void
printer_add_option_async_cb (gboolean success,
                             gpointer user_data)
{
  PpPPDOptionWidget *self = user_data;

  update_widget (user_data);
  g_clear_object (&self->cancellable);
}

static void
switch_changed_cb (PpPPDOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);

  if (gtk_switch_get_active (GTK_SWITCH (self->switch_button)))
    values[0] = g_strdup ("True");
  else
    values[0] = g_strdup ("False");

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            FALSE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static void
dropdown_changed_cb (PpPPDOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = g_strdup (dropdown_get (self->dropdown, self->option));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            FALSE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static gboolean
construct_widget (PpPPDOptionWidget *self)
{
  gint                      i;

  /* Don't show options which has only one choice */
  if (self->option && self->option->num_choices > 1)
    {
      switch (self->option->ui)
        {
          case PPD_UI_BOOLEAN:
              self->switch_button = gtk_switch_new ();

              g_signal_connect_object (self->switch_button, "notify::active", G_CALLBACK (switch_changed_cb), self, G_CONNECT_SWAPPED);
              gtk_box_append (GTK_BOX (self), self->switch_button);
              break;

          case PPD_UI_PICKONE:
              self->dropdown = dropdown_new ();

              for (i = 0; i < self->option->num_choices; i++)
                {
                  dropdown_append (self->dropdown,
                                   ppd_choice_translate (&self->option->choices[i]));
                }

              gtk_box_append (GTK_BOX (self), self->dropdown);
              g_signal_connect_object (self->dropdown, "notify::selected", G_CALLBACK (dropdown_changed_cb), self, G_CONNECT_SWAPPED);
              break;

          case PPD_UI_PICKMANY:
              self->dropdown = dropdown_new ();

              for (i = 0; i < self->option->num_choices; i++)
                {
                  dropdown_append (self->dropdown,
                                   ppd_choice_translate (&self->option->choices[i]));
                }

              gtk_box_append (GTK_BOX (self), self->dropdown);
              g_signal_connect_object (self->dropdown, "notify::selected", G_CALLBACK (dropdown_changed_cb), self, G_CONNECT_SWAPPED);
              break;

          default:
              break;
        }

      self->image = gtk_image_new_from_icon_name ("dialog-warning-symbolic");
      if (!self->image)
        self->image = gtk_image_new_from_icon_name ("dialog-warning");
      gtk_box_append (GTK_BOX (self), self->image);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
update_widget_real (PpPPDOptionWidget *self)
{
  ppd_option_t             *option = NULL, *iter;
  ppd_file_t               *ppd_file;
  gint                      i;

  if (self->ppd_filename_set && self->ppd_filename)
    {
      ppd_file = ppdOpenFile (self->ppd_filename);
      ppdLocalize (ppd_file);

      if (ppd_file)
        {
          ppdMarkDefaults (ppd_file);

          for (iter = ppdFirstOption(ppd_file); iter; iter = ppdNextOption(ppd_file))
            {
              if (g_str_equal (iter->keyword, self->option_name))
                {
                  g_clear_pointer (&self->option, cups_option_free);
                  self->option = cups_option_copy (iter);
                  break;
                }
            }

          ppdClose (ppd_file);
        }

      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
      self->ppd_filename = NULL;
    }

  option = self->option;

  if (option)
    {
      g_autofree gchar *value = NULL;

      for (i = 0; i < option->num_choices; i++)
        if (option->choices[i].marked)
          value = g_strdup (option->choices[i].choice);

      if (value == NULL)
        value = g_strdup (option->defchoice);

      if (value)
        {
          switch (option->ui)
            {
              case PPD_UI_BOOLEAN:
                g_signal_handlers_block_by_func (self->switch_button, switch_changed_cb, self);
                if (g_ascii_strcasecmp (value, "True") == 0)
                  gtk_switch_set_active (GTK_SWITCH (self->switch_button), TRUE);
                else
                  gtk_switch_set_active (GTK_SWITCH (self->switch_button), FALSE);
                g_signal_handlers_unblock_by_func (self->switch_button, switch_changed_cb, self);
                break;

              case PPD_UI_PICKONE:
                g_signal_handlers_block_by_func (self->dropdown, dropdown_changed_cb, self);
                dropdown_set (self->dropdown, option, value);
                g_signal_handlers_unblock_by_func (self->dropdown, dropdown_changed_cb, self);
                break;

              case PPD_UI_PICKMANY:
                g_signal_handlers_block_by_func (self->dropdown, dropdown_changed_cb, self);
                dropdown_set (self->dropdown, option, value);
                g_signal_handlers_unblock_by_func (self->dropdown, dropdown_changed_cb, self);
                break;

              default:
                break;
            }
        }

      gtk_widget_set_visible (self->image, option->conflicted);
    }
}

static void
get_named_dest_cb (cups_dest_t *dest,
                   gpointer     user_data)
{
  PpPPDOptionWidget *self = user_data;

  if (self->destination)
    cupsFreeDests (1, self->destination);

  self->destination = dest;
  self->destination_set = TRUE;

  if (self->ppd_filename_set)
    {
      update_widget_real (self);
    }
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  PpPPDOptionWidget *self = user_data;

  if (self->ppd_filename)
    {
      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
    }

  self->ppd_filename = g_strdup (ppd_filename);
  self->ppd_filename_set = TRUE;

  if (self->destination_set)
    {
      update_widget_real (self);
    }
}

static void
update_widget (PpPPDOptionWidget *self)
{
  self->ppd_filename_set = FALSE;
  self->destination_set = FALSE;

  get_named_dest_async (self->printer_name,
                        get_named_dest_cb,
                        self);

  printer_get_ppd_async (self->printer_name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         self);
}
