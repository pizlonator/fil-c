/* GStreamer SAMI subtitle parser
 * Copyright (c) 2006, 2013 Young-Ho Cha <ganadist at gmail com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "samiparse.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#define ITALIC_TAG 'i'
#define SPAN_TAG   's'
#define RUBY_TAG   'r'
#define RT_TAG     't'
#define CLEAR_TAG  '0'

typedef struct _HtmlParser HtmlParser;
typedef struct _HtmlContext HtmlContext;
typedef struct _GstSamiContext GstSamiContext;

struct _GstSamiContext
{
  GString *buf;                 /* buffer to collect content */
  GString *rubybuf;             /* buffer to collect ruby content */
  GString *resultbuf;           /* when opening the next 'sync' tag, move
                                 * from 'buf' to avoid to append following
                                 * content */
  GString *state;               /* in many sami files there are tags that
                                 * are not closed, so for each open tag the
                                 * parser will append a tag flag here so
                                 * that tags can be closed properly on
                                 * 'sync' tags. See _context_push_state()
                                 * and _context_pop_state(). */
  HtmlContext *htmlctxt;        /* html parser context */
  gboolean has_result;          /* set when ready to push out result */
  gboolean in_sync;             /* flag to avoid appending anything except the
                                 * content of the sync elements to buf */
  guint64 time1;                /* previous start attribute in sync tag */
  guint64 time2;                /* current start attribute in sync tag  */
};

struct _HtmlParser
{
  void (*start_element) (HtmlContext * ctx,
      const gchar * name, const gchar ** attr, gpointer user_data);
  void (*end_element) (HtmlContext * ctx,
      const gchar * name, gpointer user_data);
  void (*text) (HtmlContext * ctx,
      const gchar * text, gsize text_len, gpointer user_data);
};

struct _HtmlContext
{
  const HtmlParser *parser;
  gpointer user_data;
  GString *buf;
};

static HtmlContext *
html_context_new (HtmlParser * parser, gpointer user_data)
{
  HtmlContext *ctxt = (HtmlContext *) g_new0 (HtmlContext, 1);
  ctxt->parser = parser;
  ctxt->user_data = user_data;
  ctxt->buf = g_string_new (NULL);
  return ctxt;
}

static void
html_context_free (HtmlContext * ctxt)
{
  g_string_free (ctxt->buf, TRUE);
  g_free (ctxt);
}

typedef struct
{
  gunichar unescaped:24;
  guint8 escaped_len;
  gchar escaped[8];
} EntityMap;

#define ENTITY(unicode,ent) unicode,sizeof(ent)-1,ent

static const EntityMap XmlEntities[] = {
  {ENTITY (34, "quot")},
  {ENTITY (38, "amp")},
  {ENTITY (39, "apos")},
  {ENTITY (60, "lt")},
  {ENTITY (62, "gt")},
};

static const EntityMap HtmlEntities[] = {
/* nbsp we'll handle manually
{ 160,	"nbsp;" }, */
  {ENTITY (161, "iexcl")},
  {ENTITY (162, "cent")},
  {ENTITY (163, "pound")},
  {ENTITY (164, "curren")},
  {ENTITY (165, "yen")},
  {ENTITY (166, "brvbar")},
  {ENTITY (167, "sect")},
  {ENTITY (168, "uml")},
  {ENTITY (169, "copy")},
  {ENTITY (170, "ordf")},
  {ENTITY (171, "laquo")},
  {ENTITY (172, "not")},
  {ENTITY (173, "shy")},
  {ENTITY (174, "reg")},
  {ENTITY (175, "macr")},
  {ENTITY (176, "deg")},
  {ENTITY (177, "plusmn")},
  {ENTITY (178, "sup2")},
  {ENTITY (179, "sup3")},
  {ENTITY (180, "acute")},
  {ENTITY (181, "micro")},
  {ENTITY (182, "para")},
  {ENTITY (183, "middot")},
  {ENTITY (184, "cedil")},
  {ENTITY (185, "sup1")},
  {ENTITY (186, "ordm")},
  {ENTITY (187, "raquo")},
  {ENTITY (188, "frac14")},
  {ENTITY (189, "frac12")},
  {ENTITY (190, "frac34")},
  {ENTITY (191, "iquest")},
  {ENTITY (192, "Agrave")},
  {ENTITY (193, "Aacute")},
  {ENTITY (194, "Acirc")},
  {ENTITY (195, "Atilde")},
  {ENTITY (196, "Auml")},
  {ENTITY (197, "Aring")},
  {ENTITY (198, "AElig")},
  {ENTITY (199, "Ccedil")},
  {ENTITY (200, "Egrave")},
  {ENTITY (201, "Eacute")},
  {ENTITY (202, "Ecirc")},
  {ENTITY (203, "Euml")},
  {ENTITY (204, "Igrave")},
  {ENTITY (205, "Iacute")},
  {ENTITY (206, "Icirc")},
  {ENTITY (207, "Iuml")},
  {ENTITY (208, "ETH")},
  {ENTITY (209, "Ntilde")},
  {ENTITY (210, "Ograve")},
  {ENTITY (211, "Oacute")},
  {ENTITY (212, "Ocirc")},
  {ENTITY (213, "Otilde")},
  {ENTITY (214, "Ouml")},
  {ENTITY (215, "times")},
  {ENTITY (216, "Oslash")},
  {ENTITY (217, "Ugrave")},
  {ENTITY (218, "Uacute")},
  {ENTITY (219, "Ucirc")},
  {ENTITY (220, "Uuml")},
  {ENTITY (221, "Yacute")},
  {ENTITY (222, "THORN")},
  {ENTITY (223, "szlig")},
  {ENTITY (224, "agrave")},
  {ENTITY (225, "aacute")},
  {ENTITY (226, "acirc")},
  {ENTITY (227, "atilde")},
  {ENTITY (228, "auml")},
  {ENTITY (229, "aring")},
  {ENTITY (230, "aelig")},
  {ENTITY (231, "ccedil")},
  {ENTITY (232, "egrave")},
  {ENTITY (233, "eacute")},
  {ENTITY (234, "ecirc")},
  {ENTITY (235, "euml")},
  {ENTITY (236, "igrave")},
  {ENTITY (237, "iacute")},
  {ENTITY (238, "icirc")},
  {ENTITY (239, "iuml")},
  {ENTITY (240, "eth")},
  {ENTITY (241, "ntilde")},
  {ENTITY (242, "ograve")},
  {ENTITY (243, "oacute")},
  {ENTITY (244, "ocirc")},
  {ENTITY (245, "otilde")},
  {ENTITY (246, "ouml")},
  {ENTITY (247, "divide")},
  {ENTITY (248, "oslash")},
  {ENTITY (249, "ugrave")},
  {ENTITY (250, "uacute")},
  {ENTITY (251, "ucirc")},
  {ENTITY (252, "uuml")},
  {ENTITY (253, "yacute")},
  {ENTITY (254, "thorn")},
  {ENTITY (255, "yuml")},
  {ENTITY (338, "OElig")},
  {ENTITY (339, "oelig")},
  {ENTITY (352, "Scaron")},
  {ENTITY (353, "scaron")},
  {ENTITY (376, "Yuml")},
  {ENTITY (402, "fnof")},
  {ENTITY (710, "circ")},
  {ENTITY (732, "tilde")},
  {ENTITY (913, "Alpha")},
  {ENTITY (914, "Beta")},
  {ENTITY (915, "Gamma")},
  {ENTITY (916, "Delta")},
  {ENTITY (917, "Epsilon")},
  {ENTITY (918, "Zeta")},
  {ENTITY (919, "Eta")},
  {ENTITY (920, "Theta")},
  {ENTITY (921, "Iota")},
  {ENTITY (922, "Kappa")},
  {ENTITY (923, "Lambda")},
  {ENTITY (924, "Mu")},
  {ENTITY (925, "Nu")},
  {ENTITY (926, "Xi")},
  {ENTITY (927, "Omicron")},
  {ENTITY (928, "Pi")},
  {ENTITY (929, "Rho")},
  {ENTITY (931, "Sigma")},
  {ENTITY (932, "Tau")},
  {ENTITY (933, "Upsilon")},
  {ENTITY (934, "Phi")},
  {ENTITY (935, "Chi")},
  {ENTITY (936, "Psi")},
  {ENTITY (937, "Omega")},
  {ENTITY (945, "alpha")},
  {ENTITY (946, "beta")},
  {ENTITY (947, "gamma")},
  {ENTITY (948, "delta")},
  {ENTITY (949, "epsilon")},
  {ENTITY (950, "zeta")},
  {ENTITY (951, "eta")},
  {ENTITY (952, "theta")},
  {ENTITY (953, "iota")},
  {ENTITY (954, "kappa")},
  {ENTITY (955, "lambda")},
  {ENTITY (956, "mu")},
  {ENTITY (957, "nu")},
  {ENTITY (958, "xi")},
  {ENTITY (959, "omicron")},
  {ENTITY (960, "pi")},
  {ENTITY (961, "rho")},
  {ENTITY (962, "sigmaf")},
  {ENTITY (963, "sigma")},
  {ENTITY (964, "tau")},
  {ENTITY (965, "upsilon")},
  {ENTITY (966, "phi")},
  {ENTITY (967, "chi")},
  {ENTITY (968, "psi")},
  {ENTITY (969, "omega")},
  {ENTITY (977, "thetasym")},
  {ENTITY (978, "upsih")},
  {ENTITY (982, "piv")},
  {ENTITY (8194, "ensp")},
  {ENTITY (8195, "emsp")},
  {ENTITY (8201, "thinsp")},
  {ENTITY (8204, "zwnj")},
  {ENTITY (8205, "zwj")},
  {ENTITY (8206, "lrm")},
  {ENTITY (8207, "rlm")},
  {ENTITY (8211, "ndash")},
  {ENTITY (8212, "mdash")},
  {ENTITY (8216, "lsquo")},
  {ENTITY (8217, "rsquo")},
  {ENTITY (8218, "sbquo")},
  {ENTITY (8220, "ldquo")},
  {ENTITY (8221, "rdquo")},
  {ENTITY (8222, "bdquo")},
  {ENTITY (8224, "dagger")},
  {ENTITY (8225, "Dagger")},
  {ENTITY (8226, "bull")},
  {ENTITY (8230, "hellip")},
  {ENTITY (8240, "permil")},
  {ENTITY (8242, "prime")},
  {ENTITY (8243, "Prime")},
  {ENTITY (8249, "lsaquo")},
  {ENTITY (8250, "rsaquo")},
  {ENTITY (8254, "oline")},
  {ENTITY (8260, "frasl")},
  {ENTITY (8364, "euro")},
  {ENTITY (8465, "image")},
  {ENTITY (8472, "weierp")},
  {ENTITY (8476, "real")},
  {ENTITY (8482, "trade")},
  {ENTITY (8501, "alefsym")},
  {ENTITY (8592, "larr")},
  {ENTITY (8593, "uarr")},
  {ENTITY (8594, "rarr")},
  {ENTITY (8595, "darr")},
  {ENTITY (8596, "harr")},
  {ENTITY (8629, "crarr")},
  {ENTITY (8656, "lArr")},
  {ENTITY (8657, "uArr")},
  {ENTITY (8658, "rArr")},
  {ENTITY (8659, "dArr")},
  {ENTITY (8660, "hArr")},
  {ENTITY (8704, "forall")},
  {ENTITY (8706, "part")},
  {ENTITY (8707, "exist")},
  {ENTITY (8709, "empty")},
  {ENTITY (8711, "nabla")},
  {ENTITY (8712, "isin")},
  {ENTITY (8713, "notin")},
  {ENTITY (8715, "ni")},
  {ENTITY (8719, "prod")},
  {ENTITY (8721, "sum")},
  {ENTITY (8722, "minus")},
  {ENTITY (8727, "lowast")},
  {ENTITY (8730, "radic")},
  {ENTITY (8733, "prop")},
  {ENTITY (8734, "infin")},
  {ENTITY (8736, "ang")},
  {ENTITY (8743, "and")},
  {ENTITY (8744, "or")},
  {ENTITY (8745, "cap")},
  {ENTITY (8746, "cup")},
  {ENTITY (8747, "int")},
  {ENTITY (8756, "there4")},
  {ENTITY (8764, "sim")},
  {ENTITY (8773, "cong")},
  {ENTITY (8776, "asymp")},
  {ENTITY (8800, "ne")},
  {ENTITY (8801, "equiv")},
  {ENTITY (8804, "le")},
  {ENTITY (8805, "ge")},
  {ENTITY (8834, "sub")},
  {ENTITY (8835, "sup")},
  {ENTITY (8836, "nsub")},
  {ENTITY (8838, "sube")},
  {ENTITY (8839, "supe")},
  {ENTITY (8853, "oplus")},
  {ENTITY (8855, "otimes")},
  {ENTITY (8869, "perp")},
  {ENTITY (8901, "sdot")},
  {ENTITY (8968, "lceil")},
  {ENTITY (8969, "rceil")},
  {ENTITY (8970, "lfloor")},
  {ENTITY (8971, "rfloor")},
  {ENTITY (9001, "lang")},
  {ENTITY (9002, "rang")},
  {ENTITY (9674, "loz")},
  {ENTITY (9824, "spades")},
  {ENTITY (9827, "clubs")},
  {ENTITY (9829, "hearts")},
  {ENTITY (9830, "diams")},
};

static gchar *
unescape_string (const gchar * text)
{
  gint i;
  GString *unescaped = g_string_new (NULL);

  while (*text) {
    if (*text == '&') {
      text++;

      /* unescape &nbsp and &nbsp; */
      if (!g_ascii_strncasecmp (text, "nbsp", 4)) {
        g_string_append_unichar (unescaped, 160);
        text += 4;
        if (*text == ';') {
          text++;
        }
        goto next;
      }

      /* pass xml entities. these will be processed as pango markup */
      for (i = 0; i < G_N_ELEMENTS (XmlEntities); i++) {
        const EntityMap *entity = &XmlEntities[i];
        guint8 escaped_len = entity->escaped_len;

        if (!g_ascii_strncasecmp (text, entity->escaped, escaped_len)
            && text[escaped_len] == ';') {
          g_string_append_c (unescaped, '&');
          g_string_append_len (unescaped, entity->escaped, escaped_len);
          g_string_append_c (unescaped, ';');
          text += escaped_len + 1;
          goto next;
        }
      }

      /* convert html entities */
      for (i = 0; i < G_N_ELEMENTS (HtmlEntities); i++) {
        const EntityMap *entity = &HtmlEntities[i];
        guint8 escaped_len = entity->escaped_len;

        if (!strncmp (text, entity->escaped, escaped_len)
            && text[escaped_len] == ';') {
          g_string_append_unichar (unescaped, entity->unescaped);
          text += escaped_len + 1;
          goto next;
        }
      }

      if (*text == '#') {
        gboolean is_hex = FALSE;
        gunichar l;
        gchar *end = NULL;

        text++;
        if (*text == 'x') {
          is_hex = TRUE;
          text++;
        }
        errno = 0;
        if (is_hex) {
          l = strtoul (text, &end, 16);
        } else {
          l = strtoul (text, &end, 10);
        }

        if (text == end || errno != 0) {
          /* error occurred. pass it */
          goto next;
        }
        g_string_append_unichar (unescaped, l);
        text = end;

        if (*text == ';') {
          text++;
        }
        goto next;
      }

      /* escape & */
      g_string_append (unescaped, "&amp;");

    next:
      continue;

    } else if (g_ascii_isspace (*text)) {
      g_string_append_c (unescaped, ' ');
      /* strip whitespace */
      do {
        text++;
      } while ((*text) && g_ascii_isspace (*text));
    } else {
      g_string_append_c (unescaped, *text);
      text++;
    }
  }

  return g_string_free (unescaped, FALSE);
}

static const gchar *
string_token (const gchar * string, const gchar * delimiter, gchar ** first)
{
  gchar *next = strstr (string, delimiter);
  if (next) {
    *first = g_strndup (string, next - string);
  } else {
    *first = g_strdup (string);
  }
  return next;
}

static void
html_context_handle_element (HtmlContext * ctxt,
    const gchar * string, gboolean must_close)
{
  gchar *name = NULL;
  gint count = 0, i;
  gchar **attrs;
  const gchar *found, *next;

  /* split element name and attributes */
  next = string_token (string, " ", &name);

  if (next) {
    /* count attributes */
    found = next + 1;
    while (TRUE) {
      found = strchr (found, '=');
      if (!found)
        break;
      found++;
      count++;
    }
  } else {
    count = 0;
  }

  attrs = g_new0 (gchar *, (count + 1) * 2);

  for (i = 0; i < count && next != NULL; i += 2) {
    gchar *attr_name = NULL, *attr_value = NULL;
    gsize length;
    next = string_token (next + 1, "=", &attr_name);
    if (!next) {
      g_free (attr_name);
      break;
    }
    next = string_token (next + 1, " ", &attr_value);

    /* strip " or ' from attribute value */
    if (attr_value[0] == '"' || attr_value[0] == '\'') {
      gchar *tmp = g_strdup (attr_value + 1);
      g_free (attr_value);
      attr_value = tmp;
    }

    length = strlen (attr_value);
    if (length > 0 && (attr_value[length - 1] == '"'
            || attr_value[length - 1] == '\'')) {
      attr_value[length - 1] = '\0';
    }

    attrs[i] = attr_name;
    attrs[i + 1] = attr_value;
  }

  ctxt->parser->start_element (ctxt, name,
      (const gchar **) attrs, ctxt->user_data);
  if (must_close) {
    ctxt->parser->end_element (ctxt, name, ctxt->user_data);
  }
  g_strfreev (attrs);
  g_free (name);
}

static void
html_context_parse (HtmlContext * ctxt, gchar * text, gsize text_len)
{
  const gchar *next = NULL;

  g_string_append_len (ctxt->buf, text, text_len);
  next = ctxt->buf->str;
  while (TRUE) {
    if (next[0] == '<') {
      gchar *element = NULL;
      /* find <blahblah> */
      if (!strchr (next, '>')) {
        /* no tag end point. buffer will be process in next time */
        return;
      }

      next = string_token (next, ">", &element);
      next++;
      if (g_str_has_suffix (element, "/")) {
        /* handle <blah/> */
        element[strlen (element) - 1] = '\0';
        html_context_handle_element (ctxt, element + 1, TRUE);
      } else if (element[1] == '/') {
        /* handle </blah> */
        ctxt->parser->end_element (ctxt, element + 2, ctxt->user_data);
      } else {
        /* handle <blah> */
        html_context_handle_element (ctxt, element + 1, FALSE);
      }
      g_free (element);
    } else if (strchr (next, '<')) {
      gchar *text = NULL;
      gsize length;
      next = string_token (next, "<", &text);
      text = g_strstrip (text);
      length = strlen (text);
      ctxt->parser->text (ctxt, text, length, ctxt->user_data);
      g_free (text);

    } else {
      gchar *text = (gchar *) next;
      gsize length;
      text = g_strstrip (text);
      length = strlen (text);
      ctxt->parser->text (ctxt, text, length, ctxt->user_data);
      ctxt->buf = g_string_assign (ctxt->buf, "");
      return;
    }
  }

  ctxt->buf = g_string_assign (ctxt->buf, next);
}

static gchar *
has_tag (GString * str, const gchar tag)
{
  return strrchr (str->str, tag);
}

static void
sami_context_push_state (GstSamiContext * sctx, char state)
{
  GST_LOG ("state %c", state);
  g_string_append_c (sctx->state, state);
}

static void
sami_context_pop_state (GstSamiContext * sctx, char state)
{
  GString *str = g_string_new ("");
  GString *context_state = sctx->state;
  int i;

  GST_LOG ("state %c", state);
  for (i = context_state->len - 1; i >= 0; i--) {
    switch (context_state->str[i]) {
      case ITALIC_TAG:         /* <i> */
      {
        g_string_append (str, "</i>");
        break;
      }
      case SPAN_TAG:           /* <span foreground= > */
      {
        g_string_append (str, "</span>");
        break;
      }
      case RUBY_TAG:           /* <span size= >  -- ruby */
      {
        break;
      }
      case RT_TAG:             /*  ruby */
      {
        /* FIXME: support for furigana/ruby once implemented in pango */
        g_string_append (sctx->rubybuf, "</span>");
        if (has_tag (context_state, ITALIC_TAG)) {
          g_string_append (sctx->rubybuf, "</i>");
        }

        break;
      }
      default:
        break;
    }
    if (context_state->str[i] == state) {
      g_string_append (sctx->buf, str->str);
      g_string_free (str, TRUE);
      g_string_truncate (context_state, i);
      return;
    }
  }
  if (state == CLEAR_TAG) {
    g_string_append (sctx->buf, str->str);
    g_string_truncate (context_state, 0);
  }
  g_string_free (str, TRUE);
}

static void
handle_start_sync (GstSamiContext * sctx, const gchar ** atts)
{
  int i;

  sami_context_pop_state (sctx, CLEAR_TAG);
  if (atts != NULL) {
    for (i = 0; (atts[i] != NULL); i += 2) {
      const gchar *key, *value;

      key = atts[i];
      value = atts[i + 1];

      if (!value)
        continue;
      if (!g_ascii_strcasecmp ("start", key)) {
        /* Only set a new start time if we don't have text pending */
        if (sctx->resultbuf->len == 0)
          sctx->time1 = sctx->time2;

        sctx->time2 = atoi ((const char *) value) * GST_MSECOND;
        sctx->time2 = MAX (sctx->time2, sctx->time1);
        g_string_append (sctx->resultbuf, sctx->buf->str);
        sctx->has_result = (sctx->resultbuf->len != 0) ? TRUE : FALSE;
        g_string_truncate (sctx->buf, 0);
      }
    }
  }
}

static void
handle_start_font (GstSamiContext * sctx, const gchar ** atts)
{
  int i;

  sami_context_pop_state (sctx, SPAN_TAG);
  if (atts != NULL) {
    g_string_append (sctx->buf, "<span");
    for (i = 0; (atts[i] != NULL); i += 2) {
      const gchar *key, *value;

      key = atts[i];
      value = atts[i + 1];

      if (!value)
        continue;
      if (!g_ascii_strcasecmp ("color", key)) {
        /*
         * There are invalid color value in many
         * sami files.
         * It will fix hex color value that start without '#'
         */
        const gchar *sharp = "";
        int len = strlen (value);

        if (!(*value == '#' && len == 7)) {
          gchar *r;

          /* check if it looks like hex */
          if (strtol ((const char *) value, &r, 16) >= 0 &&
              ((gchar *) r == (value + 6) && len == 6)) {
            sharp = "#";
          }
        }
        /* some colours can be found in many sami files, but X RGB database
         * doesn't contain a colour by this name, so map explicitly */
        if (!g_ascii_strcasecmp ("aqua", value)) {
          value = "#00ffff";
        } else if (!g_ascii_strcasecmp ("crimson", value)) {
          value = "#dc143c";
        } else if (!g_ascii_strcasecmp ("fuchsia", value)) {
          value = "#ff00ff";
        } else if (!g_ascii_strcasecmp ("indigo", value)) {
          value = "#4b0082";
        } else if (!g_ascii_strcasecmp ("lime", value)) {
          value = "#00ff00";
        } else if (!g_ascii_strcasecmp ("olive", value)) {
          value = "#808000";
        } else if (!g_ascii_strcasecmp ("silver", value)) {
          value = "#c0c0c0";
        } else if (!g_ascii_strcasecmp ("teal", value)) {
          value = "#008080";
        }
        g_string_append_printf (sctx->buf, " foreground=\"%s%s\"", sharp,
            value);
      } else if (!g_ascii_strcasecmp ("face", key)) {
        g_string_append_printf (sctx->buf, " font_family=\"%s\"", value);
      }
    }
    g_string_append_c (sctx->buf, '>');
    sami_context_push_state (sctx, SPAN_TAG);
  }
}

static void
handle_start_element (HtmlContext * ctx, const gchar * name,
    const char **atts, gpointer user_data)
{
  GstSamiContext *sctx = (GstSamiContext *) user_data;

  GST_LOG ("name:%s", name);

  if (!g_ascii_strcasecmp ("sync", name)) {
    handle_start_sync (sctx, atts);
    sctx->in_sync = TRUE;
  } else if (!g_ascii_strcasecmp ("font", name)) {
    handle_start_font (sctx, atts);
  } else if (!g_ascii_strcasecmp ("ruby", name)) {
    sami_context_push_state (sctx, RUBY_TAG);
  } else if (!g_ascii_strcasecmp ("br", name)) {
    g_string_append_c (sctx->buf, '\n');
    /* FIXME: support for furigana/ruby once implemented in pango */
  } else if (!g_ascii_strcasecmp ("rt", name)) {
    if (has_tag (sctx->state, ITALIC_TAG)) {
      g_string_append (sctx->rubybuf, "<i>");
    }
    g_string_append (sctx->rubybuf, "<span size='xx-small' rise='-100'>");
    sami_context_push_state (sctx, RT_TAG);
  } else if (!g_ascii_strcasecmp ("i", name)) {
    g_string_append (sctx->buf, "<i>");
    sami_context_push_state (sctx, ITALIC_TAG);
  } else if (!g_ascii_strcasecmp ("p", name)) {
  }
}

static void
handle_end_element (HtmlContext * ctx, const char *name, gpointer user_data)
{
  GstSamiContext *sctx = (GstSamiContext *) user_data;

  GST_LOG ("name:%s", name);

  if (!g_ascii_strcasecmp ("sync", name)) {
    sctx->in_sync = FALSE;
  } else if ((!g_ascii_strcasecmp ("body", name)) ||
      (!g_ascii_strcasecmp ("sami", name))) {
    /* We will usually have one buffer left when the body is closed
     * as we need the next sync to actually send it */
    if (sctx->buf->len != 0) {
      /* Only set a new start time if we don't have text pending */
      if (sctx->resultbuf->len == 0)
        sctx->time1 = sctx->time2;

      sctx->time2 = GST_CLOCK_TIME_NONE;
      g_string_append (sctx->resultbuf, sctx->buf->str);
      sctx->has_result = (sctx->resultbuf->len != 0) ? TRUE : FALSE;
      g_string_truncate (sctx->buf, 0);
    }
  } else if (!g_ascii_strcasecmp ("font", name)) {
    sami_context_pop_state (sctx, SPAN_TAG);
  } else if (!g_ascii_strcasecmp ("ruby", name)) {
    sami_context_pop_state (sctx, RUBY_TAG);
  } else if (!g_ascii_strcasecmp ("i", name)) {
    sami_context_pop_state (sctx, ITALIC_TAG);
  }
}

static void
handle_text (HtmlContext * ctx, const gchar * text, gsize text_len,
    gpointer user_data)
{
  GstSamiContext *sctx = (GstSamiContext *) user_data;

  /* Skip everything except content of the sync elements */
  if (!sctx->in_sync)
    return;

  if (has_tag (sctx->state, RT_TAG)) {
    g_string_append_c (sctx->rubybuf, ' ');
    g_string_append (sctx->rubybuf, text);
    g_string_append_c (sctx->rubybuf, ' ');
  } else {
    g_string_append (sctx->buf, text);
  }
}

static HtmlParser samiParser = {
  handle_start_element,         /* start_element */
  handle_end_element,           /* end_element */
  handle_text,                  /* text */
};

void
sami_context_init (ParserState * state)
{
  GstSamiContext *context;

  g_assert (state->user_data == NULL);

  context = g_new0 (GstSamiContext, 1);

  context->htmlctxt = html_context_new (&samiParser, context);
  context->buf = g_string_new ("");
  context->rubybuf = g_string_new ("");
  context->resultbuf = g_string_new ("");
  context->state = g_string_new ("");

  state->user_data = context;
}

void
sami_context_deinit (ParserState * state)
{
  GstSamiContext *context = (GstSamiContext *) state->user_data;

  if (context) {
    html_context_free (context->htmlctxt);
    context->htmlctxt = NULL;
    g_string_free (context->buf, TRUE);
    g_string_free (context->rubybuf, TRUE);
    g_string_free (context->resultbuf, TRUE);
    g_string_free (context->state, TRUE);
    g_free (context);
    state->user_data = NULL;
  }
}

void
sami_context_reset (ParserState * state)
{
  GstSamiContext *context = (GstSamiContext *) state->user_data;

  if (context) {
    g_string_truncate (context->buf, 0);
    g_string_truncate (context->rubybuf, 0);
    g_string_truncate (context->resultbuf, 0);
    g_string_truncate (context->state, 0);
    context->has_result = FALSE;
    context->in_sync = FALSE;
    context->time1 = 0;
    context->time2 = 0;
  }
}

gchar *
parse_sami (ParserState * state, const gchar * line)
{
  gchar *ret = NULL;
  GstSamiContext *context = (GstSamiContext *) state->user_data;

  gchar *unescaped = unescape_string (line);
  html_context_parse (context->htmlctxt, (gchar *) unescaped,
      strlen (unescaped));
  g_free (unescaped);

  if (context->has_result) {
    if (context->rubybuf->len) {
      g_string_append_c (context->rubybuf, '\n');
      g_string_prepend (context->resultbuf, context->rubybuf->str);
      g_string_truncate (context->rubybuf, 0);
    }

    ret = g_string_free (context->resultbuf, FALSE);
    context->resultbuf = g_string_new ("");
    state->start_time = context->time1;
    state->duration = context->time2 - context->time1;
    context->has_result = FALSE;
  }
  return ret;
}
