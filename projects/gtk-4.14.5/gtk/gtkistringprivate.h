#pragma once

#include <glib.h>
#include <string.h>

typedef struct
{
  guint n_bytes;
  guint n_chars;
  union {
    char  buf[24];
    char *str;
  } u;
} IString;

static inline gboolean
istring_is_inline (const IString *str)
{
  return str->n_bytes <= (sizeof str->u.buf - 1);
}

static inline char *
istring_str (IString *str)
{
  if (istring_is_inline (str))
    return str->u.buf;
  else
    return str->u.str;
}

static inline void
istring_clear (IString *str)
{
  if (istring_is_inline (str))
    str->u.buf[0] = 0;
  else
    g_clear_pointer (&str->u.str, g_free);

  str->n_bytes = 0;
  str->n_chars = 0;
}

static inline void
istring_set (IString    *str,
             const char *text,
             guint       n_bytes,
             guint       n_chars)
{
  if G_LIKELY (n_bytes <= (sizeof str->u.buf - 1))
    {
      memcpy (str->u.buf, text, n_bytes);
      str->u.buf[n_bytes] = 0;
    }
  else
    {
      str->u.str = g_strndup (text, n_bytes);
    }

  str->n_bytes = n_bytes;
  str->n_chars = n_chars;
}

static inline gboolean
istring_empty (IString *str)
{
  return str->n_bytes == 0;
}

static inline gboolean
istring_ends_with_space (IString *str)
{
  return g_ascii_isspace (istring_str (str)[str->n_bytes - 1]);
}

static inline gboolean
istring_starts_with_space (IString *str)
{
  return g_unichar_isspace (g_utf8_get_char (istring_str (str)));
}

static inline gboolean
istring_contains_unichar (IString  *str,
                          gunichar  ch)
{
  return g_utf8_strchr (istring_str (str), str->n_bytes, ch) != NULL;
}

static inline gboolean
istring_only_contains_space (IString *str)
{
  const char *iter;

  for (iter = istring_str (str); *iter; iter = g_utf8_next_char (iter))
    {
      if (!g_unichar_isspace (g_utf8_get_char (iter)))
        return FALSE;
    }

  return TRUE;
}

static inline gboolean
istring_contains_space (IString *str)
{
  const char *iter;

  for (iter = istring_str (str); *iter; iter = g_utf8_next_char (iter))
    {
      if (g_unichar_isspace (g_utf8_get_char (iter)))
        return TRUE;
    }

  return FALSE;
}

static inline void
istring_prepend (IString *str,
                 IString *other)
{
  if G_LIKELY (str->n_bytes + other->n_bytes <= (sizeof str->u.buf - 1))
    {
      memmove (str->u.buf + other->n_bytes, str->u.buf, str->n_bytes);
      memcpy (str->u.buf, other->u.buf, other->n_bytes);
      str->n_bytes += other->n_bytes;
      str->n_chars += other->n_chars;
      str->u.buf[str->n_bytes] = 0;
    }
  else
    {
      char *old = NULL;

      if (!istring_is_inline (str))
        old = str->u.str;

      str->u.str = g_strconcat (istring_str (other), istring_str (str), NULL);
      str->n_bytes += other->n_bytes;
      str->n_chars += other->n_chars;

      g_free (old);
    }
}

static inline void
istring_append (IString *str,
                IString *other)
{
  const char *text = istring_str (other);
  guint n_bytes = other->n_bytes;
  guint n_chars = other->n_chars;

  if G_LIKELY (istring_is_inline (str))
    {
      if G_LIKELY (str->n_bytes + n_bytes <= (sizeof str->u.buf - 1))
        memcpy (str->u.buf + str->n_bytes, text, n_bytes);
      else
        str->u.str = g_strconcat (str->u.buf, text, NULL);
    }
  else
    {
      str->u.str = g_realloc (str->u.str, str->n_bytes + n_bytes + 1);
      memcpy (str->u.str + str->n_bytes, text, n_bytes);
    }

  str->n_bytes += n_bytes;
  str->n_chars += n_chars;

  istring_str (str)[str->n_bytes] = 0;
}

