/*
 * Copyright © 2019 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#pragma once

#include "gtkcssenums.h"
#include "gtkcsstokenizerprivate.h"

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GtkCssParser GtkCssParser;

typedef struct _GtkCssParseOption GtkCssParseOption;

struct _GtkCssParseOption
{
  gboolean (* can_parse)  (GtkCssParser *parser,
                           gpointer      option_data,
                           gpointer      user_data);
  gboolean (* parse)      (GtkCssParser *parser,
                           gpointer      option_data,
                           gpointer      user_data);
  gpointer data;
};

typedef void            (* GtkCssParserErrorFunc)               (GtkCssParser                   *parser,
                                                                 const GtkCssLocation           *start,
                                                                 const GtkCssLocation           *end,
                                                                 const GError                   *error,
                                                                 gpointer                        user_data);

GtkCssParser *          gtk_css_parser_new_for_file             (GFile                          *file,
                                                                 GtkCssParserErrorFunc           error_func,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy,
                                                                 GError                        **error);
GtkCssParser *          gtk_css_parser_new_for_bytes            (GBytes                         *bytes,
                                                                 GFile                          *file,
                                                                 GtkCssParserErrorFunc           error_func,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GtkCssParser *          gtk_css_parser_ref                      (GtkCssParser                   *self);
void                    gtk_css_parser_unref                    (GtkCssParser                   *self);

GFile *                 gtk_css_parser_get_file                 (GtkCssParser                   *self) G_GNUC_PURE;
GFile *                 gtk_css_parser_resolve_url              (GtkCssParser                   *self,
                                                                 const char                     *url);

const GtkCssLocation *  gtk_css_parser_get_start_location       (GtkCssParser                   *self) G_GNUC_PURE;
const GtkCssLocation *  gtk_css_parser_get_end_location         (GtkCssParser                   *self) G_GNUC_PURE;
const GtkCssLocation *  gtk_css_parser_get_block_location       (GtkCssParser                   *self) G_GNUC_PURE;

const GtkCssToken *     gtk_css_parser_peek_token               (GtkCssParser                   *self);
const GtkCssToken *     gtk_css_parser_get_token                (GtkCssParser                   *self);
void                    gtk_css_parser_consume_token            (GtkCssParser                   *self);

void                    gtk_css_parser_start_block              (GtkCssParser                   *self); 
void                    gtk_css_parser_start_semicolon_block    (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 alternative_token);
void                    gtk_css_parser_end_block_prelude        (GtkCssParser                   *self);
void                    gtk_css_parser_end_block                (GtkCssParser                   *self); 
void                    gtk_css_parser_skip                     (GtkCssParser                   *self);
void                    gtk_css_parser_skip_until               (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 token_type);

void                    gtk_css_parser_emit_error               (GtkCssParser                   *self,
                                                                 const GtkCssLocation           *start,
                                                                 const GtkCssLocation           *end,
                                                                 const GError                   *error);
void                    gtk_css_parser_error                    (GtkCssParser                   *self,
                                                                 GtkCssParserError               code,
                                                                 const GtkCssLocation           *start,
                                                                 const GtkCssLocation           *end,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(5, 6);
void                    gtk_css_parser_error_syntax             (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_parser_error_value              (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_parser_error_import             (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gtk_css_parser_warn                     (GtkCssParser                   *self,
                                                                 GtkCssParserWarning             code,
                                                                 const GtkCssLocation           *start,
                                                                 const GtkCssLocation           *end,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(5, 6);
void                    gtk_css_parser_warn_syntax              (GtkCssParser                   *self,
                                                                 const char                     *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);


gboolean                gtk_css_parser_has_token                (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 token_type);
gboolean                gtk_css_parser_has_ident                (GtkCssParser                   *self,
                                                                 const char                     *ident);
gboolean                gtk_css_parser_has_url                  (GtkCssParser                   *self);
gboolean                gtk_css_parser_has_number               (GtkCssParser                   *self);
gboolean                gtk_css_parser_has_integer              (GtkCssParser                   *self);
gboolean                gtk_css_parser_has_function             (GtkCssParser                   *self,
                                                                 const char                     *name);

gboolean                gtk_css_parser_try_delim                (GtkCssParser                   *self,
                                                                 gunichar                        codepoint);
gboolean                gtk_css_parser_try_ident                (GtkCssParser                   *self,
                                                                 const char                     *ident);
gboolean                gtk_css_parser_try_at_keyword           (GtkCssParser                   *self,
                                                                 const char                     *keyword);
gboolean                gtk_css_parser_try_token                (GtkCssParser                   *self,
                                                                 GtkCssTokenType                 token_type);

char *                  gtk_css_parser_consume_ident            (GtkCssParser                   *self) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
char *                  gtk_css_parser_consume_string           (GtkCssParser                   *self) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
char *                  gtk_css_parser_consume_url              (GtkCssParser                   *self) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
gboolean                gtk_css_parser_consume_number           (GtkCssParser                   *self,
                                                                 double                         *number);
gboolean                gtk_css_parser_consume_integer          (GtkCssParser                   *self,
                                                                 int                            *number);
gboolean                gtk_css_parser_consume_percentage       (GtkCssParser                   *self,
                                                                 double                         *number);
gboolean                gtk_css_parser_consume_function         (GtkCssParser                   *self,
                                                                 guint                           min_args,
                                                                 guint                           max_args,
                                                                 guint (* parse_func) (GtkCssParser *, guint, gpointer),
                                                                 gpointer                        data);
gsize                   gtk_css_parser_consume_any              (GtkCssParser                   *parser,
                                                                 const GtkCssParseOption        *options,
                                                                 gsize                           n_options,
                                                                 gpointer                        user_data);

G_END_DECLS

