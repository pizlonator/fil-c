/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <glib.h>
#include <pango/pangocairo.h>
#include "gtkbuildable.h"
#include "gtkaccessibletext.h"

G_BEGIN_DECLS

PangoAttrList *_gtk_pango_attr_list_merge (PangoAttrList *into,
                                           PangoAttrList *from) G_GNUC_WARN_UNUSED_RESULT;

gboolean gtk_buildable_attribute_tag_start (GtkBuildable       *buildable,
                                            GtkBuilder         *builder,
                                            GObject            *child,
                                            const char         *tagname,
                                            GtkBuildableParser *parser,
                                            gpointer           *data);

typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  PangoAttrList *attrs;
} GtkPangoAttributeParserData;

void
gtk_pango_attribute_start_element (GtkBuildableParseContext  *context,
                                   const char                *element_name,
                                   const char               **names,
                                   const char               **values,
                                   gpointer                   user_data,
                                   GError                   **error);

const char *pango_wrap_mode_to_string (PangoWrapMode mode);
const char *pango_underline_to_string (PangoUnderline underline);
const char *pango_overline_to_string (PangoOverline underline);
const char *pango_stretch_to_string (PangoStretch stretch);
const char *pango_style_to_string (PangoStyle style);
const char *pango_variant_to_string (PangoVariant variant);
const char *pango_align_to_string (PangoAlignment align);

void gtk_pango_get_font_attributes (PangoFontDescription   *font,
                                    char                 ***attribute_names,
                                    char                 ***attribute_values);
void gtk_pango_get_default_attributes (PangoLayout     *layout,
                                       char          ***attribute_names,
                                       char          ***attribute_values);
void gtk_pango_get_run_attributes     (PangoLayout     *layout,
                                       unsigned int     offset,
                                       char          ***attribute_names,
                                       char          ***attribute_values,
                                       unsigned int    *start_offset,
                                       unsigned int    *end_offset);

char *gtk_pango_get_string_at   (PangoLayout                  *layout,
                                 unsigned int                  offset,
                                 GtkAccessibleTextGranularity  granularity,
                                 unsigned int                 *start_offset,
                                 unsigned int                 *end_offset);

G_END_DECLS
