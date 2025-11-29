/* gtkfontchooserutils.h - Private utility functions for implementing a
 *                           GtkFontChooser interface
 *
 * Copyright (C) 2006 Emmanuele Bassi
 *
 * All rights reserved
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Based on gtkfilechooserutils.h:
 *	Copyright (C) 2003 Red Hat, Inc.
 */
 
#pragma once

#include "deprecated/gtkfontchooserprivate.h"

G_BEGIN_DECLS

#define GTK_FONT_CHOOSER_DELEGATE_QUARK	(_gtk_font_chooser_delegate_get_quark ())

typedef enum {
  GTK_FONT_CHOOSER_PROP_FIRST           = 0x4000,
  GTK_FONT_CHOOSER_PROP_FONT,
  GTK_FONT_CHOOSER_PROP_FONT_DESC,
  GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT,
  GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY,
  GTK_FONT_CHOOSER_PROP_LEVEL,
  GTK_FONT_CHOOSER_PROP_FONT_FEATURES,
  GTK_FONT_CHOOSER_PROP_LANGUAGE,
  GTK_FONT_CHOOSER_PROP_LAST
} GtkFontChooserProp;

void   _gtk_font_chooser_install_properties  (GObjectClass          *klass);
void   _gtk_font_chooser_delegate_iface_init (GtkFontChooserIface *iface);
void   _gtk_font_chooser_set_delegate        (GtkFontChooser      *receiver,
                                              GtkFontChooser      *delegate);

GQuark _gtk_font_chooser_delegate_get_quark  (void) G_GNUC_CONST;

G_END_DECLS

