/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbutton.h>
#include <gtk/gtkfontdialog.h>

G_BEGIN_DECLS

#define GTK_TYPE_FONT_DIALOG_BUTTON (gtk_font_dialog_button_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkFontDialogButton, gtk_font_dialog_button, GTK, FONT_DIALOG_BUTTON, GtkWidget)

GDK_AVAILABLE_IN_4_10
GtkWidget *      gtk_font_dialog_button_new              (GtkFontDialog       *dialog);

GDK_AVAILABLE_IN_4_10
GtkFontDialog *  gtk_font_dialog_button_get_dialog       (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_dialog       (GtkFontDialogButton *self,
                                                          GtkFontDialog       *dialog);

/**
 * GtkFontLevel:
 * @GTK_FONT_LEVEL_FAMILY: Select a font family
 * @GTK_FONT_LEVEL_FACE: Select a font face (i.e. a family and a style)
 * @GTK_FONT_LEVEL_FONT: Select a font (i.e. a face with a size, and possibly font variations)
 * @GTK_FONT_LEVEL_FEATURES: Select a font and font features
 *
 * The level of granularity for the font selection.
 *
 * Depending on this value, the `PangoFontDescription` that
 * is returned by [method@Gtk.FontDialogButton.get_font_desc]
 * will have more or less fields set.
 *
 * Since: 4.10
 */
typedef enum
{
  GTK_FONT_LEVEL_FAMILY,
  GTK_FONT_LEVEL_FACE,
  GTK_FONT_LEVEL_FONT,
  GTK_FONT_LEVEL_FEATURES
} GtkFontLevel;

GDK_AVAILABLE_IN_4_10
GtkFontLevel     gtk_font_dialog_button_get_level        (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_level        (GtkFontDialogButton *self,
                                                          GtkFontLevel         level);

GDK_AVAILABLE_IN_4_10
PangoFontDescription *
                 gtk_font_dialog_button_get_font_desc    (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_font_desc    (GtkFontDialogButton        *self,
                                                          const PangoFontDescription *font_desc);

GDK_AVAILABLE_IN_4_10
const char *     gtk_font_dialog_button_get_font_features
                                                         (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_font_features
                                                         (GtkFontDialogButton  *self,
                                                          const char           *font_features);

GDK_AVAILABLE_IN_4_10
PangoLanguage *  gtk_font_dialog_button_get_language     (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_language     (GtkFontDialogButton  *self,
                                                          PangoLanguage        *language);

GDK_AVAILABLE_IN_4_10
gboolean         gtk_font_dialog_button_get_use_font     (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_use_font     (GtkFontDialogButton  *self,
                                                          gboolean              use_font);

GDK_AVAILABLE_IN_4_10
gboolean         gtk_font_dialog_button_get_use_size     (GtkFontDialogButton *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_button_set_use_size     (GtkFontDialogButton  *self,
                                                          gboolean              use_size);

G_END_DECLS
