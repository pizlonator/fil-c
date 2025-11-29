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


#include "config.h"

#include "gtkcssparserprivate.h"

#include "gtkcssenums.h"
#include "gtkcsserror.h"
#include "gtkcsslocationprivate.h"

typedef struct _GtkCssParserBlock GtkCssParserBlock;

struct _GtkCssParser
{
  volatile int ref_count;

  GtkCssTokenizer *tokenizer;
  GFile *file;
  GFile *directory;
  GtkCssParserErrorFunc error_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GArray *blocks;
  GtkCssLocation location;
  GtkCssToken token;
};

struct _GtkCssParserBlock
{
  GtkCssLocation start_location;
  GtkCssTokenType end_token;
  GtkCssTokenType inherited_end_token;
  GtkCssTokenType alternative_token;
};

static GtkCssParser *
gtk_css_parser_new (GtkCssTokenizer       *tokenizer,
                    GFile                 *file,
                    GtkCssParserErrorFunc  error_func,
                    gpointer               user_data,
                    GDestroyNotify         user_destroy)
{
  GtkCssParser *self;

  self = g_new0 (GtkCssParser, 1);

  self->ref_count = 1;
  self->tokenizer = gtk_css_tokenizer_ref (tokenizer);
  if (file)
    {
      self->file = g_object_ref (file);
      self->directory = g_file_get_parent (file);
    }

  self->error_func = error_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;
  self->blocks = g_array_new (FALSE, FALSE, sizeof (GtkCssParserBlock));

  return self;
}

GtkCssParser *
gtk_css_parser_new_for_file (GFile                 *file,
                             GtkCssParserErrorFunc  error_func,
                             gpointer               user_data,
                             GDestroyNotify         user_destroy,
                             GError               **error)
{
  GBytes *bytes;
  GtkCssParser *result;

  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (bytes == NULL)
    return NULL;

  result = gtk_css_parser_new_for_bytes (bytes, file, error_func, user_data, user_destroy);

  g_bytes_unref (bytes);

  return result;
}

GtkCssParser *
gtk_css_parser_new_for_bytes (GBytes                *bytes,
                              GFile                 *file,
                              GtkCssParserErrorFunc  error_func,
                              gpointer               user_data,
                              GDestroyNotify         user_destroy)
{
  GtkCssTokenizer *tokenizer;
  GtkCssParser *result;
  
  tokenizer = gtk_css_tokenizer_new (bytes);
  result = gtk_css_parser_new (tokenizer, file, error_func, user_data, user_destroy);
  gtk_css_tokenizer_unref (tokenizer);

  return result;
}

static void
gtk_css_parser_finalize (GtkCssParser *self)
{
  if (self->user_destroy)
    self->user_destroy (self->user_data);

  g_clear_pointer (&self->tokenizer, gtk_css_tokenizer_unref);
  g_clear_object (&self->file);
  g_clear_object (&self->directory);
  if (self->blocks->len)
    g_critical ("Finalizing CSS parser with %u remaining blocks", self->blocks->len);
  g_array_free (self->blocks, TRUE);

  g_free (self);
}

GtkCssParser *
gtk_css_parser_ref (GtkCssParser *self)
{
  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
gtk_css_parser_unref (GtkCssParser *self)
{
  if (g_atomic_int_dec_and_test (&self->ref_count))
    gtk_css_parser_finalize (self);
}

/**
 * gtk_css_parser_get_file:
 * @self: a `GtkCssParser`
 *
 * Gets the file being parsed. If no file is associated with @self -
 * for example when raw data is parsed - %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The file being parsed
 */
GFile *
gtk_css_parser_get_file (GtkCssParser *self)
{
  return self->file;
}

/**
 * gtk_css_parser_resolve_url:
 * @self: a `GtkCssParser`
 * @url: the URL to resolve
 *
 * Resolves a given URL against the parser's location.
 *
 * Returns: (nullable) (transfer full): a new `GFile` for the
 *   resolved URL
 */
GFile *
gtk_css_parser_resolve_url (GtkCssParser *self,
                            const char   *url)
{
  char *scheme;

  scheme = g_uri_parse_scheme (url);
  if (scheme != NULL)
    {
      GFile *file = g_file_new_for_uri (url);
      g_free (scheme);
      return file;
    }

  if (self->directory == NULL)
    return NULL;

  return g_file_resolve_relative_path (self->directory, url);
}

/**
 * gtk_css_parser_get_start_location:
 * @self: a `GtkCssParser`
 *
 * Queries the location of the current token.
 *
 * This function will return the location of the start of the
 * current token. In the case a token has been consumed, but no
 * new token has been queried yet via gtk_css_parser_peek_token()
 * or gtk_css_parser_get_token(), the previous token's start
 * location will be returned.
 *
 * This function may return the same location as
 * gtk_css_parser_get_end_location() - in particular at the
 * beginning and end of the document.
 *
 * Returns: the start location
 **/
const GtkCssLocation *
gtk_css_parser_get_start_location (GtkCssParser *self)
{
  return &self->location;
}

/**
 * gtk_css_parser_get_end_location:
 * @self: a `GtkCssParser`
 * @out_location: (caller-allocates) Place to store the location
 *
 * Queries the location of the current token.
 *
 * This function will return the location of the end of the
 * current token. In the case a token has been consumed, but no
 * new token has been queried yet via gtk_css_parser_peek_token()
 * or gtk_css_parser_get_token(), the previous token's end location
 * will be returned.
 *
 * This function may return the same location as
 * gtk_css_parser_get_start_location() - in particular at the
 * beginning and end of the document.
 *
 * Returns: the end location
 **/
const GtkCssLocation *
gtk_css_parser_get_end_location (GtkCssParser *self)
{
  return gtk_css_tokenizer_get_location (self->tokenizer);
}

/**
 * gtk_css_parser_get_block_location:
 * @self: a `GtkCssParser`
 *
 * Queries the start location of the token that started the current
 * block that is being parsed.
 *
 * If no block is currently parsed, the beginning of the document
 * is returned.
 *
 * Returns: The start location of the current block
 */
const GtkCssLocation *
gtk_css_parser_get_block_location (GtkCssParser *self)
{
  const GtkCssParserBlock *block;

  if (self->blocks->len == 0)
    {
      static const GtkCssLocation start_of_document = { 0, };
      return &start_of_document;
    }

  block = &g_array_index (self->blocks, GtkCssParserBlock, self->blocks->len - 1);
  return &block->start_location;
}

static void
gtk_css_parser_ensure_token (GtkCssParser *self)
{
  GError *error = NULL;

  if (!gtk_css_token_is (&self->token, GTK_CSS_TOKEN_EOF))
    return;

  self->location = *gtk_css_tokenizer_get_location (self->tokenizer);
  if (!gtk_css_tokenizer_read_token (self->tokenizer, &self->token, &error))
    {
      /* We ignore the error here, because the resulting token will
       * likely already trigger an error in the parsing code and
       * duplicate errors are rather useless.
       */
      g_clear_error (&error);
    }
}

const GtkCssToken *
gtk_css_parser_peek_token (GtkCssParser *self)
{
  static const GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF, };

  gtk_css_parser_ensure_token (self);

  if (self->blocks->len)
    {
      const GtkCssParserBlock *block = &g_array_index (self->blocks, GtkCssParserBlock, self->blocks->len - 1);
      if (gtk_css_token_is (&self->token, block->end_token) ||
          gtk_css_token_is (&self->token, block->inherited_end_token) ||
          gtk_css_token_is (&self->token, block->alternative_token))
        return &eof_token;
    }

  return &self->token;
}

const GtkCssToken *
gtk_css_parser_get_token (GtkCssParser *self)
{
  const GtkCssToken *token;

  for (token = gtk_css_parser_peek_token (self);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT) ||
       gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
       token = gtk_css_parser_peek_token (self))
    {
      gtk_css_parser_consume_token (self);
    }

  return token;
}

void
gtk_css_parser_consume_token (GtkCssParser *self)
{
  gtk_css_parser_ensure_token (self);

  /* unpreserved tokens MUST be consumed via start_block() */
  g_assert (gtk_css_token_is_preserved (&self->token, NULL));

  /* Don't consume any tokens at the end of a block */
  if (!gtk_css_token_is (gtk_css_parser_peek_token (self), GTK_CSS_TOKEN_EOF))
    gtk_css_token_clear (&self->token);
}

void
gtk_css_parser_start_block (GtkCssParser *self)
{
  GtkCssParserBlock block;

  gtk_css_parser_ensure_token (self);

  if (gtk_css_token_is_preserved (&self->token, &block.end_token))
    {
      g_critical ("gtk_css_parser_start_block() may only be called for non-preserved tokens");
      return;
    }

  block.inherited_end_token = GTK_CSS_TOKEN_EOF;
  block.alternative_token = GTK_CSS_TOKEN_EOF;
  block.start_location = self->location;
  g_array_append_val (self->blocks, block);

  gtk_css_token_clear (&self->token);
}

void
gtk_css_parser_start_semicolon_block (GtkCssParser    *self,
                                      GtkCssTokenType  alternative_token)
{
  GtkCssParserBlock block;

  block.end_token = GTK_CSS_TOKEN_SEMICOLON;
  if (self->blocks->len)
    block.inherited_end_token = g_array_index (self->blocks, GtkCssParserBlock, self->blocks->len - 1).end_token;
  else
    block.inherited_end_token = GTK_CSS_TOKEN_EOF;
  block.alternative_token = alternative_token;
  block.start_location = self->location;
  g_array_append_val (self->blocks, block);
}

void
gtk_css_parser_end_block_prelude (GtkCssParser *self)
{
  GtkCssParserBlock *block;

  g_return_if_fail (self->blocks->len > 0);

  block = &g_array_index (self->blocks, GtkCssParserBlock, self->blocks->len - 1);

  if (block->alternative_token == GTK_CSS_TOKEN_EOF)
    return;

  gtk_css_parser_skip_until (self, GTK_CSS_TOKEN_EOF);

  if (gtk_css_token_is (&self->token, block->alternative_token))
    {
      if (gtk_css_token_is_preserved (&self->token, &block->end_token))
        {
          g_critical ("alternative token is not preserved");
          return;
        }
      block->alternative_token = GTK_CSS_TOKEN_EOF;
      block->inherited_end_token = GTK_CSS_TOKEN_EOF;
      gtk_css_token_clear (&self->token);
    }
}

void
gtk_css_parser_end_block (GtkCssParser *self)
{
  GtkCssParserBlock *block;

  g_return_if_fail (self->blocks->len > 0);

  gtk_css_parser_skip_until (self, GTK_CSS_TOKEN_EOF);

  block = &g_array_index (self->blocks, GtkCssParserBlock, self->blocks->len - 1);

  if (gtk_css_token_is (&self->token, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_warn (self,
                           GTK_CSS_PARSER_WARNING_SYNTAX,
                           gtk_css_parser_get_block_location (self),
                           gtk_css_parser_get_start_location (self),
                           "Unterminated block at end of document");
      g_array_set_size (self->blocks, self->blocks->len - 1);
    }
  else if (gtk_css_token_is (&self->token, block->inherited_end_token))
    {
      g_assert (block->end_token == GTK_CSS_TOKEN_SEMICOLON);
      gtk_css_parser_warn (self,
                           GTK_CSS_PARSER_WARNING_SYNTAX,
                           gtk_css_parser_get_block_location (self),
                           gtk_css_parser_get_start_location (self),
                           "Expected ';' at end of block");
      g_array_set_size (self->blocks, self->blocks->len - 1);
    }
  else
    {
      g_array_set_size (self->blocks, self->blocks->len - 1);
      if (gtk_css_token_is_preserved (&self->token, NULL))
        {
          gtk_css_token_clear (&self->token);
        }
      else
        {
          gtk_css_parser_start_block (self);
          gtk_css_parser_end_block (self);
        }
    }
}

/*
 * gtk_css_parser_skip:
 * @self: a `GtkCssParser`
 *
 * Skips a component value.
 *
 * This means that if the token is a preserved token, only
 * this token will be skipped. If the token starts a block,
 * the whole block will be skipped.
 **/
void
gtk_css_parser_skip (GtkCssParser *self)
{
  const GtkCssToken *token;
  
  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is_preserved (token, NULL))
    {
      gtk_css_parser_consume_token (self);
    }
  else
    {
      gtk_css_parser_start_block (self);
      gtk_css_parser_end_block (self);
    }
}

/*
 * gtk_css_parser_skip_until:
 * @self: a `GtkCssParser`
 * @token_type: type of token to skip to
 *
 * Repeatedly skips a token until a certain type is reached.
 * After this called, gtk_css_parser_get_token() will either
 * return a token of this type or the eof token.
 *
 * This function is useful for resyncing a parser after encountering
 * an error.
 *
 * If you want to skip until the end, use %GSK_TOKEN_TYPE_EOF
 * as the token type.
 **/
void
gtk_css_parser_skip_until (GtkCssParser    *self,
                           GtkCssTokenType  token_type)
{
  const GtkCssToken *token;
  
  for (token = gtk_css_parser_get_token (self);
       !gtk_css_token_is (token, token_type) &&
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (self))
    {
      gtk_css_parser_skip (self);
    }
}

void
gtk_css_parser_emit_error (GtkCssParser         *self,
                           const GtkCssLocation *start,
                           const GtkCssLocation *end,
                           const GError         *error)
{
  if (self->error_func)
    self->error_func (self, start, end, error, self->user_data);
}

void
gtk_css_parser_error (GtkCssParser         *self,
                      GtkCssParserError     code,
                      const GtkCssLocation *start,
                      const GtkCssLocation *end,
                      const char           *format,
                      ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              code,
                              format, args);
  gtk_css_parser_emit_error (self, start, end, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_error_syntax (GtkCssParser *self,
                             const char   *format,
                             ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_SYNTAX,
                              format, args);
  gtk_css_parser_emit_error (self,
                             gtk_css_parser_get_start_location (self),
                             gtk_css_parser_get_end_location (self),
                             error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_error_value (GtkCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                              format, args);
  gtk_css_parser_emit_error (self,
                             gtk_css_parser_get_start_location (self),
                             gtk_css_parser_get_end_location (self),
                             error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_error_import (GtkCssParser *self,
                             const char   *format,
                             ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_IMPORT,
                              format, args);
  gtk_css_parser_emit_error (self,
                             gtk_css_parser_get_start_location (self),
                             gtk_css_parser_get_end_location (self),
                             error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_warn (GtkCssParser         *self,
                     GtkCssParserWarning   code,
                     const GtkCssLocation *start,
                     const GtkCssLocation *end,
                     const char           *format,
                     ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_WARNING,
                              code,
                              format, args);
  gtk_css_parser_emit_error (self, start, end, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_warn_syntax (GtkCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_WARNING,
                              GTK_CSS_PARSER_WARNING_SYNTAX,
                              format, args);
  gtk_css_parser_emit_error (self,
                             gtk_css_parser_get_start_location (self),
                             gtk_css_parser_get_end_location (self),
                             error);
  g_error_free (error);
  va_end (args);
}

gboolean
gtk_css_parser_consume_function (GtkCssParser *self,
                                 guint         min_args,
                                 guint         max_args,
                                 guint (* parse_func) (GtkCssParser *, guint, gpointer),
                                 gpointer      data)
{
  const GtkCssToken *token;
  gboolean result = FALSE;
  char function_name[64];
  guint arg;

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  g_strlcpy (function_name, gtk_css_token_get_string (token), 64);
  gtk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      guint parse_args = parse_func (self, arg, data);
      if (parse_args == 0)
        break;
      arg += parse_args;
      token = gtk_css_parser_get_token (self);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gtk_css_parser_error_syntax (self, "%s() requires at least %u arguments", function_name, min_args);
              break;
            }
          else
            {
              result = TRUE;
              break;
            }
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        {
          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gtk_css_parser_consume_token (self);
          continue;
        }
      else
        {
          gtk_css_parser_error_syntax (self, "Unexpected data at end of %s() argument", function_name);
          break;
        }
    }

  gtk_css_parser_end_block (self);

  return result;
}

/**
 * gtk_css_parser_has_token:
 * @self: a `GtkCssParser`
 * @token_type: type of the token to check
 *
 * Checks if the next token is of @token_type.
 *
 * Returns: %TRUE if the next token is of @token_type
 **/
gboolean
gtk_css_parser_has_token (GtkCssParser    *self,
                          GtkCssTokenType  token_type)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  return gtk_css_token_is (token, token_type);
}

/**
 * gtk_css_parser_has_ident:
 * @self: a `GtkCssParser`
 * @ident: name of identifier
 *
 * Checks if the next token is an identifier with the given @name.
 *
 * Returns: %TRUE if the next token is an identifier with the given @name
 **/
gboolean
gtk_css_parser_has_ident (GtkCssParser *self,
                          const char   *ident)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  return gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) &&
         g_ascii_strcasecmp (gtk_css_token_get_string (token), ident) == 0;
}

gboolean
gtk_css_parser_has_integer (GtkCssParser *self)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  return gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
         gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER);
}

/**
 * gtk_css_parser_has_function:
 * @self: a `GtkCssParser`
 * @name: name of function
 *
 * Checks if the next token is a function with the given @name.
 *
 * Returns: %TRUE if the next token is a function with the given @name
 */
gboolean
gtk_css_parser_has_function (GtkCssParser *self,
                             const char   *name)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  return gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION) &&
         g_ascii_strcasecmp (gtk_css_token_get_string (token), name) == 0;
}

/**
 * gtk_css_parser_try_delim:
 * @self: a `GtkCssParser`
 * @codepoint: unicode character codepoint to check
 *
 * Checks if the current token is a delimiter matching the given
 * @codepoint. If that is the case, the token is consumed and
 * %TRUE is returned.
 *
 * Keep in mind that not every unicode codepoint can be a delim
 * token.
 *
 * Returns: %TRUE if the token matched and was consumed.
 **/
gboolean
gtk_css_parser_try_delim (GtkCssParser *self,
                          gunichar      codepoint)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_DELIM) ||
      codepoint != token->delim.delim)
    return FALSE;

  gtk_css_parser_consume_token (self);
  return TRUE;
}

/**
 * gtk_css_parser_try_ident:
 * @self: a `GtkCssParser`
 * @ident: identifier to check for
 *
 * Checks if the current token is an identifier matching the given
 * @ident string. If that is the case, the token is consumed
 * and %TRUE is returned.
 *
 * Returns: %TRUE if the token matched and was consumed.
 **/
gboolean
gtk_css_parser_try_ident (GtkCssParser *self,
                          const char   *ident)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) ||
      g_ascii_strcasecmp (gtk_css_token_get_string (token), ident) != 0)
    return FALSE;

  gtk_css_parser_consume_token (self);
  return TRUE;
}

/**
 * gtk_css_parser_try_at_keyword:
 * @self: a `GtkCssParser`
 * @keyword: name of keyword to check for
 *
 * Checks if the current token is an at-keyword token with the
 * given @keyword. If that is the case, the token is consumed
 * and %TRUE is returned.
 *
 * Returns: %TRUE if the token matched and was consumed.
 **/
gboolean
gtk_css_parser_try_at_keyword (GtkCssParser *self,
                               const char   *keyword)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_AT_KEYWORD) ||
      g_ascii_strcasecmp (gtk_css_token_get_string (token), keyword) != 0)
    return FALSE;

  gtk_css_parser_consume_token (self);
  return TRUE;
}

/**
 * gtk_css_parser_try_token:
 * @self: a `GtkCssParser`
 * @token_type: type of token to try
 *
 * Consumes the next token if it matches the given @token_type.
 *
 * This function can be used in loops like this:
 * do {
 *   ... parse one element ...
 * } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA);
 *
 * Returns: %TRUE if a token was consumed
 **/
gboolean
gtk_css_parser_try_token (GtkCssParser    *self,
                          GtkCssTokenType  token_type)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, token_type))
    return FALSE;

  gtk_css_parser_consume_token (self);
  return TRUE;
}

/**
 * gtk_css_parser_consume_ident:
 * @self: a `GtkCssParser`
 *
 * If the current token is an identifier, consumes it and returns
 * its name.
 *
 * If the current token is not an identifier, an error is emitted
 * and %NULL is returned.
 *
 * Returns: (transfer full): the name of the consumed identifier
 */
char *
gtk_css_parser_consume_ident (GtkCssParser *self)
{
  const GtkCssToken *token;
  char *ident;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_parser_error_syntax (self, "Expected an identifier");
      return NULL;
    }

  ident = g_strdup (gtk_css_token_get_string (token));
  gtk_css_parser_consume_token (self);

  return ident;
}

/**
 * gtk_css_parser_consume_string:
 * @self: a `GtkCssParser`
 *
 * If the current token is a string, consumes it and return the string.
 *
 * If the current token is not a string, an error is emitted
 * and %NULL is returned.
 *
 * Returns: (transfer full): the name of the consumed string
 **/
char *
gtk_css_parser_consume_string (GtkCssParser *self)
{
  const GtkCssToken *token;
  char *ident;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    {
      gtk_css_parser_error_syntax (self, "Expected a string");
      return NULL;
    }

  ident = g_strdup (gtk_css_token_get_string (token));
  gtk_css_parser_consume_token (self);

  return ident;
}

static guint
gtk_css_parser_parse_url_arg (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data)
{
  char **out_url = data;

  *out_url = gtk_css_parser_consume_string (parser);
  if (*out_url == NULL)
    return 0;
  
  return 1;
}

gboolean
gtk_css_parser_has_url (GtkCssParser *self)
{
  return gtk_css_parser_has_token (self, GTK_CSS_TOKEN_URL)
      || gtk_css_parser_has_token (self, GTK_CSS_TOKEN_BAD_URL)
      || gtk_css_parser_has_function (self, "url");
}

/**
 * gtk_css_parser_consume_url:
 * @self: a `GtkCssParser`
 *
 * If the parser matches the `<url>` token from the [CSS
 * specification](https://drafts.csswg.org/css-values-4/#url-value),
 * consumes it, resolves the URL and returns the resulting `GFile`.
 * On failure, an error is emitted and %NULL is returned.
 *
 * Returns: (nullable) (transfer full): the resulting URL
 **/
char *
gtk_css_parser_consume_url (GtkCssParser *self)
{
  const GtkCssToken *token;
  char *url;

  token = gtk_css_parser_get_token (self);

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_URL))
    {
      url = g_strdup (gtk_css_token_get_string (token));
      gtk_css_parser_consume_token (self);
    }
  else if (gtk_css_token_is_function (token, "url"))
    {
      if (!gtk_css_parser_consume_function (self, 1, 1, gtk_css_parser_parse_url_arg, &url))
        return NULL;
    }
  else
    {
      gtk_css_parser_error_syntax (self, "Expected a URL");
      return NULL;
    }
  
  return url;
}

gboolean
gtk_css_parser_has_number (GtkCssParser *self)
{
  return gtk_css_parser_has_token (self, GTK_CSS_TOKEN_SIGNED_NUMBER)
      || gtk_css_parser_has_token (self, GTK_CSS_TOKEN_SIGNLESS_NUMBER)
      || gtk_css_parser_has_token (self, GTK_CSS_TOKEN_SIGNED_INTEGER)
      || gtk_css_parser_has_token (self, GTK_CSS_TOKEN_SIGNLESS_INTEGER);
}

gboolean
gtk_css_parser_consume_number (GtkCssParser *self,
                               double       *number)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      *number = token->number.number;
      gtk_css_parser_consume_token (self);
      return TRUE;
    }

  gtk_css_parser_error_syntax (self, "Expected a number");
  /* FIXME: Implement calc() */
  return FALSE;
}

gboolean
gtk_css_parser_consume_integer (GtkCssParser *self,
                                int          *number)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      *number = token->number.number;
      gtk_css_parser_consume_token (self);
      return TRUE;
    }

  gtk_css_parser_error_syntax (self, "Expected an integer");
  /* FIXME: Implement calc() */
  return FALSE;
}

gboolean
gtk_css_parser_consume_percentage (GtkCssParser *self,
                                   double       *number)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      *number = token->number.number;
      gtk_css_parser_consume_token (self);
      return TRUE;
    }

  gtk_css_parser_error_syntax (self, "Expected a percentage");
  /* FIXME: Implement calc() */
  return FALSE;
}

gsize
gtk_css_parser_consume_any (GtkCssParser            *parser,
                            const GtkCssParseOption *options,
                            gsize                    n_options,
                            gpointer                 user_data)
{
  gsize result;
  gsize i;

  g_return_val_if_fail (parser != NULL, 0);
  g_return_val_if_fail (options != NULL, 0);
  g_return_val_if_fail (n_options < sizeof (gsize) * 8 - 1, 0);

  result = 0;
  while (result != (1u << n_options) - 1u)
    {
      for (i = 0; i < n_options; i++)
        {
          if (result & (1 << i))
            continue;
          if (options[i].can_parse && !options[i].can_parse (parser, options[i].data, user_data))
            continue;
          if (!options[i].parse (parser, options[i].data, user_data))
            return 0;
          result |= 1 << i;
          break;
        }
      if (i == n_options)
        break;
    }

  if (result == 0)
    {
      gtk_css_parser_error_syntax (parser, "No valid value given");
      return result;
    }

  return result;
}
