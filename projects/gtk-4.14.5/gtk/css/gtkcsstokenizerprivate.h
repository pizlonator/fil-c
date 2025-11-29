/* GSK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#pragma once

#include <glib.h>

#include <gtk/css/gtkcsslocation.h>

G_BEGIN_DECLS

typedef enum {
  /* no content */
  GTK_CSS_TOKEN_EOF,
  GTK_CSS_TOKEN_WHITESPACE,
  GTK_CSS_TOKEN_OPEN_PARENS,
  GTK_CSS_TOKEN_CLOSE_PARENS,
  GTK_CSS_TOKEN_OPEN_SQUARE,
  GTK_CSS_TOKEN_CLOSE_SQUARE,
  GTK_CSS_TOKEN_OPEN_CURLY,
  GTK_CSS_TOKEN_CLOSE_CURLY,
  GTK_CSS_TOKEN_COMMA,
  GTK_CSS_TOKEN_COLON,
  GTK_CSS_TOKEN_SEMICOLON,
  GTK_CSS_TOKEN_CDO,
  GTK_CSS_TOKEN_CDC,
  GTK_CSS_TOKEN_INCLUDE_MATCH,
  GTK_CSS_TOKEN_DASH_MATCH,
  GTK_CSS_TOKEN_PREFIX_MATCH,
  GTK_CSS_TOKEN_SUFFIX_MATCH,
  GTK_CSS_TOKEN_SUBSTRING_MATCH,
  GTK_CSS_TOKEN_COLUMN,
  GTK_CSS_TOKEN_BAD_STRING,
  GTK_CSS_TOKEN_BAD_URL,
  GTK_CSS_TOKEN_COMMENT,
  /* delim */
  GTK_CSS_TOKEN_DELIM,
  /* string */
  GTK_CSS_TOKEN_STRING,
  GTK_CSS_TOKEN_IDENT,
  GTK_CSS_TOKEN_FUNCTION,
  GTK_CSS_TOKEN_AT_KEYWORD,
  GTK_CSS_TOKEN_HASH_UNRESTRICTED,
  GTK_CSS_TOKEN_HASH_ID,
  GTK_CSS_TOKEN_URL,
  /* number */
  GTK_CSS_TOKEN_SIGNED_INTEGER,
  GTK_CSS_TOKEN_SIGNLESS_INTEGER,
  GTK_CSS_TOKEN_SIGNED_NUMBER,
  GTK_CSS_TOKEN_SIGNLESS_NUMBER,
  GTK_CSS_TOKEN_PERCENTAGE,
  /* dimension */
  GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION,
  GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION,
  GTK_CSS_TOKEN_SIGNED_DIMENSION,
  GTK_CSS_TOKEN_SIGNLESS_DIMENSION
} GtkCssTokenType;

typedef union _GtkCssToken GtkCssToken;
typedef struct _GtkCssTokenizer GtkCssTokenizer;

typedef struct _GtkCssStringToken GtkCssStringToken;
typedef struct _GtkCssDelimToken GtkCssDelimToken;
typedef struct _GtkCssNumberToken GtkCssNumberToken;
typedef struct _GtkCssDimensionToken GtkCssDimensionToken;

struct _GtkCssStringToken {
  GtkCssTokenType  type;
  int len;
  union {
    char             buf[16];
    char            *string;
  } u;
};

struct _GtkCssDelimToken {
  GtkCssTokenType  type;
  gunichar         delim;
};

struct _GtkCssNumberToken {
  GtkCssTokenType  type;
  double           number;
};

struct _GtkCssDimensionToken {
  GtkCssTokenType  type;
  double           value;
  char             dimension[8];
};

union _GtkCssToken {
  GtkCssTokenType type;
  GtkCssStringToken string;
  GtkCssDelimToken delim;
  GtkCssNumberToken number;
  GtkCssDimensionToken dimension;
};

static inline const char *
gtk_css_token_get_string (const GtkCssToken *token)
{
  if (token->string.len < 16)
    return token->string.u.buf;
  else
    return token->string.u.string;
}

void                    gtk_css_token_clear                     (GtkCssToken            *token);

gboolean                gtk_css_token_is_finite                 (const GtkCssToken      *token) G_GNUC_PURE;
gboolean                gtk_css_token_is_preserved              (const GtkCssToken      *token,
                                                                 GtkCssTokenType        *out_closing) G_GNUC_PURE;
#define gtk_css_token_is(token, _type) ((token)->type == (_type))
gboolean                gtk_css_token_is_ident                  (const GtkCssToken      *token,
                                                                 const char             *ident) G_GNUC_PURE;
gboolean                gtk_css_token_is_function               (const GtkCssToken      *token,
                                                                 const char             *ident) G_GNUC_PURE;
gboolean                gtk_css_token_is_delim                  (const GtkCssToken      *token,
                                                                 gunichar                delim) G_GNUC_PURE;

void                    gtk_css_token_print                     (const GtkCssToken      *token,
                                                                 GString                *string);
char *                  gtk_css_token_to_string                 (const GtkCssToken      *token);

GtkCssTokenizer *       gtk_css_tokenizer_new                   (GBytes                 *bytes);

GtkCssTokenizer *       gtk_css_tokenizer_ref                   (GtkCssTokenizer        *tokenizer);
void                    gtk_css_tokenizer_unref                 (GtkCssTokenizer        *tokenizer);

const GtkCssLocation *  gtk_css_tokenizer_get_location          (GtkCssTokenizer        *tokenizer) G_GNUC_CONST;

gboolean                gtk_css_tokenizer_read_token            (GtkCssTokenizer        *tokenizer,
                                                                 GtkCssToken            *token,
                                                                 GError                **error);

G_END_DECLS

