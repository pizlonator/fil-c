/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
 *
 * m3u8.c:
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

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <glib.h>
#include <string.h>

#include "m3u8.h"
#include "gsthlselements.h"

#define GST_CAT_DEFAULT hls_debug

static GstM3U8MediaFile *gst_m3u8_media_file_new (gchar * uri,
    gchar * title, GstClockTime duration, guint sequence,
    GstDateTime * program_dt);
static void gst_m3u8_init_file_unref (GstM3U8InitFile * self);
static gchar *uri_join (const gchar * uri, const gchar * path);

GstM3U8 *
gst_m3u8_new (void)
{
  GstM3U8 *m3u8;

  m3u8 = g_new0 (GstM3U8, 1);

  m3u8->current_file = NULL;
  m3u8->current_file_duration = GST_CLOCK_TIME_NONE;
  m3u8->sequence = -1;
  m3u8->sequence_position = 0;
  m3u8->highest_sequence_number = -1;
  m3u8->duration = GST_CLOCK_TIME_NONE;

  g_mutex_init (&m3u8->lock);
  m3u8->ref_count = 1;

  return m3u8;
}

/* call with M3U8_LOCK held */
static void
gst_m3u8_take_uri (GstM3U8 * self, gchar * uri, gchar * base_uri, gchar * name)
{
  g_return_if_fail (self != NULL);

  if (self->uri != uri) {
    g_free (self->uri);
    self->uri = uri;
  }
  if (self->base_uri != base_uri) {
    g_free (self->base_uri);
    self->base_uri = base_uri;
  }
  if (self->name != name) {
    g_free (self->name);
    self->name = name;
  }
}

void
gst_m3u8_set_uri (GstM3U8 * m3u8, const gchar * uri, const gchar * base_uri,
    const gchar * name)
{
  GST_M3U8_LOCK (m3u8);
  gst_m3u8_take_uri (m3u8, g_strdup (uri), g_strdup (base_uri),
      g_strdup (name));
  GST_M3U8_UNLOCK (m3u8);
}

GstM3U8 *
gst_m3u8_ref (GstM3U8 * m3u8)
{
  g_assert (m3u8 != NULL && m3u8->ref_count > 0);

  g_atomic_int_add (&m3u8->ref_count, 1);
  return m3u8;
}

void
gst_m3u8_unref (GstM3U8 * self)
{
  g_return_if_fail (self != NULL && self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count)) {
    g_free (self->uri);
    g_free (self->base_uri);
    g_free (self->name);

    g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_unref, NULL);
    g_list_free (self->files);

    g_free (self->last_data);
    g_mutex_clear (&self->lock);
    g_free (self);
  }
}

static GstM3U8MediaFile *
gst_m3u8_media_file_new (gchar * uri, gchar * title, GstClockTime duration,
    guint sequence, GstDateTime * program_dt)
{
  GstM3U8MediaFile *file;

  file = g_new0 (GstM3U8MediaFile, 1);
  file->uri = uri;
  file->title = title;
  file->duration = duration;
  file->sequence = sequence;
  file->ref_count = 1;
  file->program_dt = program_dt;

  return file;
}

GstM3U8MediaFile *
gst_m3u8_media_file_ref (GstM3U8MediaFile * mfile)
{
  g_assert (mfile != NULL && mfile->ref_count > 0);

  g_atomic_int_add (&mfile->ref_count, 1);
  return mfile;
}

void
gst_m3u8_media_file_unref (GstM3U8MediaFile * self)
{
  g_return_if_fail (self != NULL && self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count)) {
    if (self->init_file)
      gst_m3u8_init_file_unref (self->init_file);
    g_free (self->title);
    g_free (self->uri);
    g_free (self->key);
    if (self->program_dt)
      gst_date_time_unref (self->program_dt);
    g_free (self);
  }
}

static GstM3U8InitFile *
gst_m3u8_init_file_new (gchar * uri)
{
  GstM3U8InitFile *file;

  file = g_new0 (GstM3U8InitFile, 1);
  file->uri = uri;
  file->ref_count = 1;

  return file;
}

static GstM3U8InitFile *
gst_m3u8_init_file_ref (GstM3U8InitFile * ifile)
{
  g_assert (ifile != NULL && ifile->ref_count > 0);

  g_atomic_int_add (&ifile->ref_count, 1);
  return ifile;
}

static void
gst_m3u8_init_file_unref (GstM3U8InitFile * self)
{
  g_return_if_fail (self != NULL && self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count)) {
    g_free (self->uri);
    g_free (self);
  }
}

static gboolean
int_from_string (gchar * ptr, gchar ** endptr, gint * val)
{
  gchar *end;
  gint64 ret;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  ret = g_ascii_strtoll (ptr, &end, 10);
  if ((errno == ERANGE && (ret == G_MAXINT64 || ret == G_MININT64))
      || (errno != 0 && ret == 0)) {
    GST_WARNING ("%s", g_strerror (errno));
    return FALSE;
  }

  if (ret > G_MAXINT || ret < G_MININT) {
    GST_WARNING ("%s", g_strerror (ERANGE));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  *val = (gint) ret;

  return end != ptr;
}

static gboolean
int64_from_string (gchar * ptr, gchar ** endptr, gint64 * val)
{
  gchar *end;
  gint64 ret;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  ret = g_ascii_strtoll (ptr, &end, 10);
  if ((errno == ERANGE && (ret == G_MAXINT64 || ret == G_MININT64))
      || (errno != 0 && ret == 0)) {
    GST_WARNING ("%s", g_strerror (errno));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  *val = ret;

  return end != ptr;
}

static gboolean
double_from_string (gchar * ptr, gchar ** endptr, gdouble * val)
{
  gchar *end;
  gdouble ret;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  ret = g_ascii_strtod (ptr, &end);
  if ((errno == ERANGE && (ret == HUGE_VAL || ret == -HUGE_VAL))
      || (errno != 0 && ret == 0)) {
    GST_WARNING ("%s", g_strerror (errno));
    return FALSE;
  }

  if (!isfinite (ret)) {
    GST_WARNING ("%s", g_strerror (ERANGE));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  *val = (gdouble) ret;

  return end != ptr;
}

static gboolean
parse_attributes (gchar ** ptr, gchar ** a, gchar ** v)
{
  gchar *end = NULL, *p, *ve;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (*ptr != NULL, FALSE);
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (v != NULL, FALSE);

  /* [attribute=value,]* */

  *a = *ptr;
  end = p = g_utf8_strchr (*ptr, -1, ',');
  if (end) {
    gchar *q = g_utf8_strchr (*ptr, -1, '"');
    if (q && q < end) {
      /* special case, such as CODECS="avc1.77.30, mp4a.40.2" */
      q = g_utf8_next_char (q);
      if (q) {
        q = g_utf8_strchr (q, -1, '"');
      }
      if (q) {
        end = p = g_utf8_strchr (q, -1, ',');
      }
    }
  }
  if (end) {
    do {
      end = g_utf8_next_char (end);
    } while (end && *end == ' ');
    *p = '\0';
  }

  *v = p = g_utf8_strchr (*ptr, -1, '=');
  if (*v) {
    *p = '\0';
    *v = g_utf8_next_char (*v);
    if (**v == '"') {
      ve = g_utf8_next_char (*v);
      if (ve) {
        ve = g_utf8_strchr (ve, -1, '"');
      }
      if (ve) {
        *v = g_utf8_next_char (*v);
        *ve = '\0';
      } else {
        GST_WARNING ("Cannot remove quotation marks from %s", *a);
      }
    }
  } else {
    GST_WARNING ("missing = after attribute");
    return FALSE;
  }

  *ptr = end;
  return TRUE;
}

static gint
gst_hls_variant_stream_compare_by_bitrate (gconstpointer a, gconstpointer b)
{
  const GstHLSVariantStream *vs_a = (const GstHLSVariantStream *) a;
  const GstHLSVariantStream *vs_b = (const GstHLSVariantStream *) b;

  if (vs_a->bandwidth == vs_b->bandwidth)
    return g_strcmp0 (vs_a->name, vs_b->name);

  return vs_a->bandwidth - vs_b->bandwidth;
}

/* If we have MEDIA-SEQUENCE, ensure that it's consistent. If it is not,
 * the client SHOULD halt playback (6.3.4), which is what we do then. */
static gboolean
check_media_seqnums (GstM3U8 * self, GList * previous_files)
{
  GList *l, *m;
  GstM3U8MediaFile *f1 = NULL, *f2 = NULL;

  g_return_val_if_fail (previous_files, FALSE);

  if (!self->files) {
    /* Empty playlists are trivially consistent */
    return TRUE;
  }

  /* Find first case of higher/equal sequence number in new playlist.
   * From there on we can linearly step ahead */
  for (l = self->files; l; l = l->next) {
    gboolean match = FALSE;

    f1 = l->data;
    for (m = previous_files; m; m = m->next) {
      f2 = m->data;

      if (f1->sequence >= f2->sequence) {
        match = TRUE;
        break;
      }
    }
    if (match)
      break;
  }

  /* We must have seen at least one entry on each list */
  g_assert (f1 != NULL);
  g_assert (f2 != NULL);

  if (!l) {
    /* No match, no sequence in the new playlist was higher than
     * any in the old. This is bad! */
    GST_ERROR ("Media sequence doesn't continue: last new %" G_GINT64_FORMAT
        " < last old %" G_GINT64_FORMAT, f1->sequence, f2->sequence);
    return FALSE;
  }

  for (; l && m; l = l->next, m = m->next) {
    f1 = l->data;
    f2 = m->data;

    if (f1->sequence == f2->sequence && !g_str_equal (f1->uri, f2->uri)) {
      /* Same sequence, different URI. This is bad! */
      GST_ERROR ("Media URIs inconsistent (sequence %" G_GINT64_FORMAT
          "): had '%s', got '%s'", f1->sequence, f2->uri, f1->uri);
      return FALSE;
    } else if (f1->sequence < f2->sequence) {
      /* Not same sequence but by construction sequence must be higher in the
       * new one. All good in that case, if it isn't then this means that
       * sequence numbers are decreasing or files were inserted */
      GST_ERROR ("Media sequences inconsistent: %" G_GINT64_FORMAT " < %"
          G_GINT64_FORMAT ": URIs new '%s' old '%s'", f1->sequence,
          f2->sequence, f1->uri, f2->uri);
      return FALSE;
    }
  }

  /* All good if we're getting here */
  return TRUE;
}

/* If we don't have MEDIA-SEQUENCE, we check URIs in the previous and
 * current playlist to calculate the/a correct MEDIA-SEQUENCE for the new
 * playlist in relation to the old. That is, same URIs get the same number
 * and later URIs get higher numbers */
static void
generate_media_seqnums (GstM3U8 * self, GList * previous_files)
{
  GList *l, *m;
  GstM3U8MediaFile *f1 = NULL, *f2 = NULL;
  gint64 mediasequence;

  g_return_if_fail (previous_files);

  /* Find first case of same URI in new playlist.
   * From there on we can linearly step ahead */
  for (l = self->files; l; l = l->next) {
    gboolean match = FALSE;

    f1 = l->data;
    for (m = previous_files; m; m = m->next) {
      f2 = m->data;

      if (g_str_equal (f1->uri, f2->uri)) {
        match = TRUE;
        break;
      }
    }

    if (match)
      break;
  }

  if (l) {
    /* Match, check that all following ones are matching too and continue
     * sequence numbers from there on */

    mediasequence = f2->sequence;

    for (; l && m; l = l->next, m = m->next) {
      f1 = l->data;
      f2 = m->data;

      f1->sequence = mediasequence;
      mediasequence++;

      if (!g_str_equal (f1->uri, f2->uri)) {
        GST_WARNING ("Inconsistent URIs after playlist update: '%s' != '%s'",
            f1->uri, f2->uri);
      }
    }
  } else {
    /* No match, this means f2 is the last item in the previous playlist
     * and we have to start our new playlist at that sequence */
    mediasequence = f2->sequence + 1;
    l = self->files;
  }

  for (; l; l = l->next) {
    f1 = l->data;

    f1->sequence = mediasequence;
    mediasequence++;
  }
}

/*
 * @data: a m3u8 playlist text data, taking ownership
 */
gboolean
gst_m3u8_update (GstM3U8 * self, gchar * data)
{
  gint val;
  GstDateTime *program_dt = NULL;
  GstClockTime duration;
  gchar *title, *end;
  gboolean discontinuity = FALSE;
  gchar *current_key = NULL;
  gboolean have_iv = FALSE;
  guint8 iv[16] = { 0, };
  gint64 size = -1, offset = -1;
  gint64 mediasequence;
  GList *previous_files = NULL;
  gboolean have_mediasequence = FALSE;
  GstM3U8InitFile *last_init_file = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  GST_M3U8_LOCK (self);

  /* check if the data changed since last update */
  if (self->last_data && g_str_equal (self->last_data, data)) {
    GST_DEBUG ("Playlist is the same as previous one");
    g_free (data);
    GST_M3U8_UNLOCK (self);
    return TRUE;
  }

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    GST_WARNING ("Data doesn't start with #EXTM3U");
    g_free (data);
    GST_M3U8_UNLOCK (self);
    return FALSE;
  }

  if (g_strrstr (data, "\n#EXT-X-STREAM-INF:") != NULL) {
    GST_WARNING ("Not a media playlist, but a master playlist!");
    GST_M3U8_UNLOCK (self);
    return FALSE;
  }

  GST_TRACE ("data:\n%s", data);

  g_free (self->last_data);
  self->last_data = data;

  self->current_file = NULL;
  previous_files = self->files;
  self->files = NULL;
  self->duration = GST_CLOCK_TIME_NONE;
  mediasequence = 0;

  /* By default, allow caching */
  self->allowcache = TRUE;

  duration = 0;
  title = NULL;
  data += 7;
  while (TRUE) {
    gchar *r;

    end = g_utf8_strchr (data, -1, '\n');
    if (end)
      *end = '\0';

    r = g_utf8_strchr (data, -1, '\r');
    if (r)
      *r = '\0';

    if (data[0] != '#' && data[0] != '\0') {
      if (duration <= 0) {
        GST_LOG ("%s: got line without EXTINF, dropping", data);
        goto next_line;
      }

      data = uri_join (self->base_uri ? self->base_uri : self->uri, data);
      if (data != NULL) {
        GstM3U8MediaFile *file;
        file =
            gst_m3u8_media_file_new (data, title, duration,
            mediasequence++, program_dt);
        program_dt = NULL;

        /* set encryption params */
        file->key = current_key ? g_strdup (current_key) : NULL;
        if (file->key) {
          if (have_iv) {
            memcpy (file->iv, iv, sizeof (iv));
          } else {
            guint8 *iv = file->iv + 12;
            GST_WRITE_UINT32_BE (iv, file->sequence);
          }
        }

        if (size != -1) {
          file->size = size;
          if (offset != -1) {
            file->offset = offset;
          } else {
            GstM3U8MediaFile *prev = self->files ? self->files->data : NULL;

            if (!prev) {
              offset = 0;
            } else {
              offset = prev->offset + prev->size;
            }
            file->offset = offset;
          }
        } else {
          file->size = -1;
          file->offset = 0;
        }

        file->discont = discontinuity;
        if (last_init_file)
          file->init_file = gst_m3u8_init_file_ref (last_init_file);

        duration = 0;
        title = NULL;
        discontinuity = FALSE;
        size = offset = -1;
        self->files = g_list_prepend (self->files, file);
      }

    } else if (g_str_has_prefix (data, "#EXTINF:")) {
      gdouble fval;
      if (!double_from_string (data + 8, &data, &fval)) {
        GST_WARNING ("Can't read EXTINF duration");
        goto next_line;
      }
      duration = fval * (gdouble) GST_SECOND;
      if (self->targetduration > 0 && duration > self->targetduration) {
        GST_WARNING ("EXTINF duration (%" GST_TIME_FORMAT
            ") > TARGETDURATION (%" GST_TIME_FORMAT ")",
            GST_TIME_ARGS (duration), GST_TIME_ARGS (self->targetduration));
      }
      if (!data || *data != ',')
        goto next_line;
      data = g_utf8_next_char (data);
      if (data != end) {
        g_free (title);
        title = g_strdup (data);
      }
    } else if (g_str_has_prefix (data, "#EXT-X-")) {
      gchar *data_ext_x = data + 7;

      /* All these entries start with #EXT-X- */
      if (g_str_has_prefix (data_ext_x, "ENDLIST")) {
        self->endlist = TRUE;
      } else if (g_str_has_prefix (data_ext_x, "VERSION:")) {
        if (int_from_string (data + 15, &data, &val))
          self->version = val;
      } else if (g_str_has_prefix (data_ext_x, "TARGETDURATION:")) {
        if (int_from_string (data + 22, &data, &val))
          self->targetduration = val * GST_SECOND;
      } else if (g_str_has_prefix (data_ext_x, "MEDIA-SEQUENCE:")) {
        if (int_from_string (data + 22, &data, &val)) {
          mediasequence = val;
          have_mediasequence = TRUE;
        }
      } else if (g_str_has_prefix (data_ext_x, "DISCONTINUITY-SEQUENCE:")) {
        if (int_from_string (data + 30, &data, &val)
            && val != self->discont_sequence) {
          self->discont_sequence = val;
          discontinuity = TRUE;
        }
      } else if (g_str_has_prefix (data_ext_x, "DISCONTINUITY")) {
        self->discont_sequence++;
        discontinuity = TRUE;
      } else if (g_str_has_prefix (data_ext_x, "PROGRAM-DATE-TIME:")) {
        if (program_dt)
          gst_date_time_unref (program_dt);
        program_dt = gst_date_time_new_from_iso8601_string (data + 25);
        if (!program_dt) {
          GST_WARNING ("Could not parse program date/time");
        }
      } else if (g_str_has_prefix (data_ext_x, "ALLOW-CACHE:")) {
        self->allowcache = g_ascii_strcasecmp (data + 19, "YES") == 0;
      } else if (g_str_has_prefix (data_ext_x, "KEY:")) {
        gchar *v, *a;

        data = data + 11;

        /* IV and KEY are only valid until the next #EXT-X-KEY */
        have_iv = FALSE;
        g_free (current_key);
        current_key = NULL;
        while (data && parse_attributes (&data, &a, &v)) {
          if (g_str_equal (a, "URI")) {
            current_key =
                uri_join (self->base_uri ? self->base_uri : self->uri, v);
          } else if (g_str_equal (a, "IV")) {
            gchar *ivp = v;
            gint i;

            if (strlen (ivp) < 32 + 2 || (!g_str_has_prefix (ivp, "0x")
                    && !g_str_has_prefix (ivp, "0X"))) {
              GST_WARNING ("Can't read IV");
              continue;
            }

            ivp += 2;
            for (i = 0; i < 16; i++) {
              gint h, l;

              h = g_ascii_xdigit_value (*ivp);
              ivp++;
              l = g_ascii_xdigit_value (*ivp);
              ivp++;
              if (h == -1 || l == -1) {
                i = -1;
                break;
              }
              iv[i] = (h << 4) | l;
            }

            if (i == -1) {
              GST_WARNING ("Can't read IV");
              continue;
            }
            have_iv = TRUE;
          } else if (g_str_equal (a, "METHOD")) {
            if (!g_str_equal (v, "AES-128")) {
              GST_WARNING ("Encryption method %s not supported", v);
              continue;
            }
          }
        }
      } else if (g_str_has_prefix (data_ext_x, "BYTERANGE:")) {
        gchar *v = data + 17;

        if (int64_from_string (v, &v, &size)) {
          if (*v == '@' && !int64_from_string (v + 1, &v, &offset))
            goto next_line;
        } else {
          goto next_line;
        }
      } else if (g_str_has_prefix (data_ext_x, "MAP:")) {
        gchar *v, *a, *header_uri = NULL;

        data = data + 11;

        while (data != NULL && parse_attributes (&data, &a, &v)) {
          if (strcmp (a, "URI") == 0) {
            header_uri =
                uri_join (self->base_uri ? self->base_uri : self->uri, v);
          } else if (strcmp (a, "BYTERANGE") == 0) {
            if (int64_from_string (v, &v, &size)) {
              if (*v == '@' && !int64_from_string (v + 1, &v, &offset)) {
                g_free (header_uri);
                goto next_line;
              }
            } else {
              g_free (header_uri);
              goto next_line;
            }
          }
        }

        if (header_uri) {
          GstM3U8InitFile *init_file;
          init_file = gst_m3u8_init_file_new (header_uri);

          if (size != -1) {
            init_file->size = size;
            if (offset != -1)
              init_file->offset = offset;
            else
              init_file->offset = 0;
          } else {
            init_file->size = -1;
            init_file->offset = 0;
          }
          if (last_init_file)
            gst_m3u8_init_file_unref (last_init_file);

          last_init_file = init_file;
        }
      } else {
        GST_LOG ("Ignored line: %s", data);
      }
    } else {
      GST_LOG ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  g_free (current_key);
  current_key = NULL;

  if (program_dt) {
    gst_date_time_unref (program_dt);
    program_dt = NULL;
  }

  self->files = g_list_reverse (self->files);

  if (last_init_file)
    gst_m3u8_init_file_unref (last_init_file);

  if (previous_files) {
    gboolean consistent = TRUE;

    if (have_mediasequence) {
      consistent = check_media_seqnums (self, previous_files);
    } else {
      generate_media_seqnums (self, previous_files);
    }

    g_list_foreach (previous_files, (GFunc) gst_m3u8_media_file_unref, NULL);
    g_list_free (previous_files);
    previous_files = NULL;

    /* error was reported above already */
    if (!consistent) {
      GST_M3U8_UNLOCK (self);
      return FALSE;
    }
  }

  if (self->files == NULL) {
    GST_ERROR ("Invalid media playlist, it does not contain any media files");
    GST_M3U8_UNLOCK (self);
    return FALSE;
  }

  /* calculate the start and end times of this media playlist. */
  {
    GList *walk;
    GstM3U8MediaFile *file;
    GstClockTime duration = 0;

    mediasequence = -1;

    for (walk = self->files; walk; walk = walk->next) {
      file = walk->data;

      if (mediasequence == -1) {
        mediasequence = file->sequence;
      } else if (mediasequence >= file->sequence) {
        GST_ERROR ("Non-increasing media sequence");
        GST_M3U8_UNLOCK (self);
        return FALSE;
      } else {
        mediasequence = file->sequence;
      }

      duration += file->duration;
      if (file->sequence > self->highest_sequence_number) {
        if (self->highest_sequence_number >= 0) {
          /* if an update of the media playlist has been missed, there
             will be a gap between self->highest_sequence_number and the
             first sequence number in this media playlist. In this situation
             assume that the missing fragments had a duration of
             targetduration each */
          self->last_file_end +=
              (file->sequence - self->highest_sequence_number -
              1) * self->targetduration;
        }
        self->last_file_end += file->duration;
        self->highest_sequence_number = file->sequence;
      }
    }
    if (GST_M3U8_IS_LIVE (self)) {
      self->first_file_start = self->last_file_end - duration;
      GST_DEBUG ("Live playlist range %" GST_TIME_FORMAT " -> %"
          GST_TIME_FORMAT, GST_TIME_ARGS (self->first_file_start),
          GST_TIME_ARGS (self->last_file_end));
    }
    self->duration = duration;
  }

  /* first-time setup */
  if (self->files && self->sequence == -1) {
    GList *file;

    if (GST_M3U8_IS_LIVE (self)) {
      gint i;
      GstClockTime sequence_pos = 0;

      file = g_list_last (self->files);

      if (self->last_file_end >= GST_M3U8_MEDIA_FILE (file->data)->duration) {
        sequence_pos =
            self->last_file_end - GST_M3U8_MEDIA_FILE (file->data)->duration;
      }

      /* for live streams, start GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE from
       * the end of the playlist. See section 6.3.3 of HLS draft */
      for (i = 0; i < GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE && file->prev &&
          GST_M3U8_MEDIA_FILE (file->prev->data)->duration <= sequence_pos;
          ++i) {
        file = file->prev;
        sequence_pos -= GST_M3U8_MEDIA_FILE (file->data)->duration;
      }
      self->sequence_position = sequence_pos;
    } else {
      file = g_list_first (self->files);
      self->sequence_position = 0;
    }
    self->current_file = file;
    self->sequence = GST_M3U8_MEDIA_FILE (file->data)->sequence;
    GST_DEBUG ("first sequence: %u", (guint) self->sequence);
  }

  GST_LOG ("processed media playlist %s, %u fragments", self->name,
      g_list_length (self->files));

  GST_M3U8_UNLOCK (self);

  return TRUE;
}

/* call with M3U8_LOCK held */
static GList *
m3u8_find_next_fragment (GstM3U8 * m3u8, gboolean forward)
{
  GstM3U8MediaFile *file;
  GList *l = m3u8->files;

  if (forward) {
    while (l) {
      file = l->data;

      if (file->sequence >= m3u8->sequence)
        break;

      l = l->next;
    }
  } else {
    l = g_list_last (l);

    while (l) {
      file = l->data;

      if (file->sequence <= m3u8->sequence)
        break;

      l = l->prev;
    }
  }

  return l;
}

GstM3U8MediaFile *
gst_m3u8_get_next_fragment (GstM3U8 * m3u8, gboolean forward,
    GstClockTime * sequence_position, GstDateTime ** program_dt,
    gboolean * discont)
{
  GstM3U8MediaFile *file = NULL;

  g_return_val_if_fail (m3u8 != NULL, NULL);

  GST_M3U8_LOCK (m3u8);

  GST_DEBUG ("Looking for fragment %" G_GINT64_FORMAT, m3u8->sequence);

  if (m3u8->sequence < 0)       /* can't happen really */
    goto out;

  if (m3u8->current_file == NULL)
    m3u8->current_file = m3u8_find_next_fragment (m3u8, forward);

  if (m3u8->current_file == NULL)
    goto out;

  file = gst_m3u8_media_file_ref (m3u8->current_file->data);

  GST_DEBUG ("Got fragment with sequence %u (current sequence %u)",
      (guint) file->sequence, (guint) m3u8->sequence);

  if (sequence_position)
    *sequence_position = m3u8->sequence_position;

  if (program_dt)
    *program_dt =
        file->program_dt ? gst_date_time_ref (file->program_dt) : NULL;

  if (discont)
    *discont = file->discont || (m3u8->sequence != file->sequence);

  m3u8->current_file_duration = file->duration;
  m3u8->sequence = file->sequence;

out:

  GST_M3U8_UNLOCK (m3u8);

  return file;
}

gboolean
gst_m3u8_has_next_fragment (GstM3U8 * m3u8, gboolean forward)
{
  gboolean have_next;
  GList *cur;

  g_return_val_if_fail (m3u8 != NULL, FALSE);

  GST_M3U8_LOCK (m3u8);

  GST_DEBUG ("Checking next fragment %" G_GINT64_FORMAT,
      m3u8->sequence + (forward ? 1 : -1));

  if (m3u8->current_file) {
    cur = m3u8->current_file;
  } else {
    cur = m3u8_find_next_fragment (m3u8, forward);
  }

  have_next = cur && ((forward && cur->next) || (!forward && cur->prev));

  GST_M3U8_UNLOCK (m3u8);

  return have_next;
}

/* call with M3U8_LOCK held */
static void
m3u8_alternate_advance (GstM3U8 * m3u8, gboolean forward)
{
  gint targetnum = m3u8->sequence;
  GList *tmp;
  GstM3U8MediaFile *mf;

  /* figure out the target seqnum */
  if (forward)
    targetnum += 1;
  else
    targetnum -= 1;

  for (tmp = m3u8->files; tmp; tmp = tmp->next) {
    mf = (GstM3U8MediaFile *) tmp->data;
    if (mf->sequence == targetnum)
      break;
  }
  if (tmp == NULL) {
    GST_WARNING ("Can't find next fragment");
    return;
  }
  m3u8->current_file = tmp;
  m3u8->sequence = targetnum;
  m3u8->current_file_duration = GST_M3U8_MEDIA_FILE (tmp->data)->duration;
}

void
gst_m3u8_advance_fragment (GstM3U8 * m3u8, gboolean forward)
{
  GstM3U8MediaFile *file;

  g_return_if_fail (m3u8 != NULL);

  GST_M3U8_LOCK (m3u8);

  GST_DEBUG ("Sequence position was %" GST_TIME_FORMAT,
      GST_TIME_ARGS (m3u8->sequence_position));
  if (GST_CLOCK_TIME_IS_VALID (m3u8->current_file_duration)) {
    /* Advance our position based on the previous fragment we played */
    if (forward)
      m3u8->sequence_position += m3u8->current_file_duration;
    else if (m3u8->current_file_duration < m3u8->sequence_position)
      m3u8->sequence_position -= m3u8->current_file_duration;
    else
      m3u8->sequence_position = 0;
    m3u8->current_file_duration = GST_CLOCK_TIME_NONE;
    GST_DEBUG ("Sequence position now %" GST_TIME_FORMAT,
        GST_TIME_ARGS (m3u8->sequence_position));
  }
  if (!m3u8->current_file) {
    GList *l;

    GST_DEBUG ("Looking for fragment %" G_GINT64_FORMAT, m3u8->sequence);
    for (l = m3u8->files; l != NULL; l = l->next) {
      if (GST_M3U8_MEDIA_FILE (l->data)->sequence == m3u8->sequence) {
        m3u8->current_file = l;
        break;
      }
    }
    if (m3u8->current_file == NULL) {
      GST_DEBUG
          ("Could not find current fragment, trying next fragment directly");
      m3u8_alternate_advance (m3u8, forward);

      /* Resync sequence number if the above has failed for live streams */
      if (m3u8->current_file == NULL && GST_M3U8_IS_LIVE (m3u8)) {
        /* for live streams, start GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE from
           the end of the playlist. See section 6.3.3 of HLS draft */
        gint pos =
            g_list_length (m3u8->files) - GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE;
        m3u8->current_file = g_list_nth (m3u8->files, pos >= 0 ? pos : 0);
        m3u8->current_file_duration =
            GST_M3U8_MEDIA_FILE (m3u8->current_file->data)->duration;

        GST_WARNING ("Resyncing live playlist");
      }
      goto out;
    }
  }

  file = GST_M3U8_MEDIA_FILE (m3u8->current_file->data);
  GST_DEBUG ("Advancing from sequence %u", (guint) file->sequence);
  if (forward) {
    m3u8->current_file = m3u8->current_file->next;
    if (m3u8->current_file) {
      m3u8->sequence = GST_M3U8_MEDIA_FILE (m3u8->current_file->data)->sequence;
    } else {
      m3u8->sequence = file->sequence + 1;
    }
  } else {
    m3u8->current_file = m3u8->current_file->prev;
    if (m3u8->current_file) {
      m3u8->sequence = GST_M3U8_MEDIA_FILE (m3u8->current_file->data)->sequence;
    } else {
      m3u8->sequence = file->sequence - 1;
    }
  }
  if (m3u8->current_file) {
    /* Store duration of the fragment we're using to update the position 
     * the next time we advance */
    m3u8->current_file_duration =
        GST_M3U8_MEDIA_FILE (m3u8->current_file->data)->duration;
  }

out:

  GST_M3U8_UNLOCK (m3u8);
}

GstClockTime
gst_m3u8_get_duration (GstM3U8 * m3u8)
{
  GstClockTime duration = GST_CLOCK_TIME_NONE;

  g_return_val_if_fail (m3u8 != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_LOCK (m3u8);

  /* We can only get the duration for on-demand streams */
  if (!m3u8->endlist)
    goto out;

  if (!GST_CLOCK_TIME_IS_VALID (m3u8->duration) && m3u8->files != NULL) {
    GList *f;

    m3u8->duration = 0;
    for (f = m3u8->files; f != NULL; f = f->next)
      m3u8->duration += GST_M3U8_MEDIA_FILE (f)->duration;
  }
  duration = m3u8->duration;

out:

  GST_M3U8_UNLOCK (m3u8);

  return duration;
}

GstClockTime
gst_m3u8_get_target_duration (GstM3U8 * m3u8)
{
  GstClockTime target_duration;

  g_return_val_if_fail (m3u8 != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_LOCK (m3u8);
  target_duration = m3u8->targetduration;
  GST_M3U8_UNLOCK (m3u8);

  return target_duration;
}

gchar *
gst_m3u8_get_uri (GstM3U8 * m3u8)
{
  gchar *uri;

  GST_M3U8_LOCK (m3u8);
  uri = g_strdup (m3u8->uri);
  GST_M3U8_UNLOCK (m3u8);

  return uri;
}

gboolean
gst_m3u8_is_live (GstM3U8 * m3u8)
{
  gboolean is_live;

  g_return_val_if_fail (m3u8 != NULL, FALSE);

  GST_M3U8_LOCK (m3u8);
  is_live = GST_M3U8_IS_LIVE (m3u8);
  GST_M3U8_UNLOCK (m3u8);

  return is_live;
}

gchar *
uri_join (const gchar * uri1, const gchar * uri2)
{
  gchar *uri_copy, *tmp, *ret = NULL;

  if (gst_uri_is_valid (uri2))
    return g_strdup (uri2);

  uri_copy = g_strdup (uri1);
  if (uri2[0] != '/') {
    /* uri2 is a relative uri2 */
    /* look for query params */
    tmp = g_utf8_strchr (uri_copy, -1, '?');
    if (tmp) {
      /* find last / char, ignoring query params */
      tmp = g_utf8_strrchr (uri_copy, tmp - uri_copy, '/');
    } else {
      /* find last / char in URL */
      tmp = g_utf8_strrchr (uri_copy, -1, '/');
    }
    if (!tmp) {
      GST_WARNING ("Can't build a valid uri_copy");
      goto out;
    }

    *tmp = '\0';
    ret = g_strdup_printf ("%s/%s", uri_copy, uri2);
  } else {
    /* uri2 is an absolute uri2 */
    char *scheme, *hostname;

    scheme = uri_copy;
    /* find the : in <scheme>:// */
    tmp = g_utf8_strchr (uri_copy, -1, ':');
    if (!tmp) {
      GST_WARNING ("Can't build a valid uri_copy");
      goto out;
    }

    *tmp = '\0';

    /* skip :// */
    hostname = tmp + 3;

    tmp = g_utf8_strchr (hostname, -1, '/');
    if (tmp)
      *tmp = '\0';

    ret = g_strdup_printf ("%s://%s%s", scheme, hostname, uri2);
  }

out:
  g_free (uri_copy);
  return ret;
}

gboolean
gst_m3u8_get_seek_range (GstM3U8 * m3u8, gint64 * start, gint64 * stop)
{
  GstClockTime duration = 0;
  GList *walk;
  GstM3U8MediaFile *file;
  guint count;
  guint min_distance = 0;

  g_return_val_if_fail (m3u8 != NULL, FALSE);

  GST_M3U8_LOCK (m3u8);

  if (m3u8->files == NULL)
    goto out;

  if (GST_M3U8_IS_LIVE (m3u8)) {
    /* min_distance is used to make sure the seek range is never closer than
       GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE fragments from the end of a live
       playlist - see 6.3.3. "Playing the Playlist file" of the HLS draft */
    min_distance = GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE;
  }
  count = g_list_length (m3u8->files);

  for (walk = m3u8->files; walk && count > min_distance; walk = walk->next) {
    file = walk->data;
    --count;
    duration += file->duration;
  }

  if (duration <= 0)
    goto out;

  *start = m3u8->first_file_start;
  *stop = *start + duration;

out:

  GST_M3U8_UNLOCK (m3u8);
  return (duration > 0);
}

GstHLSMedia *
gst_hls_media_ref (GstHLSMedia * media)
{
  g_assert (media != NULL && media->ref_count > 0);
  g_atomic_int_add (&media->ref_count, 1);
  return media;
}

void
gst_hls_media_unref (GstHLSMedia * media)
{
  g_assert (media != NULL && media->ref_count > 0);
  if (g_atomic_int_dec_and_test (&media->ref_count)) {
    if (media->playlist)
      gst_m3u8_unref (media->playlist);
    g_free (media->group_id);
    g_free (media->name);
    g_free (media->uri);
    g_free (media->lang);
    g_free (media);
  }
}

static GstHLSMediaType
gst_m3u8_get_hls_media_type_from_string (const gchar * type_name)
{
  if (strcmp (type_name, "AUDIO") == 0)
    return GST_HLS_MEDIA_TYPE_AUDIO;
  if (strcmp (type_name, "VIDEO") == 0)
    return GST_HLS_MEDIA_TYPE_VIDEO;
  if (strcmp (type_name, "SUBTITLES") == 0)
    return GST_HLS_MEDIA_TYPE_SUBTITLES;
  if (strcmp (type_name, "CLOSED-CAPTIONS") == 0)
    return GST_HLS_MEDIA_TYPE_CLOSED_CAPTIONS;

  return GST_HLS_MEDIA_TYPE_INVALID;
}

#define GST_HLS_MEDIA_TYPE_NAME(mtype) gst_hls_media_type_get_name(mtype)
const gchar *
gst_hls_media_type_get_name (GstHLSMediaType mtype)
{
  static const gchar *nicks[GST_HLS_N_MEDIA_TYPES] = { "audio", "video",
    "subtitle", "closed-captions"
  };

  if (mtype < 0 || mtype >= GST_HLS_N_MEDIA_TYPES)
    return "invalid";

  return nicks[mtype];
}

/* returns unquoted copy of string */
static gchar *
gst_m3u8_unquote (const gchar * str)
{
  const gchar *start, *end;

  start = strchr (str, '"');
  if (start == NULL)
    return g_strdup (str);
  end = strchr (start + 1, '"');
  if (end == NULL) {
    GST_WARNING ("Broken quoted string [%s] - can't find end quote", str);
    return g_strdup (start + 1);
  }
  return g_strndup (start + 1, (gsize) (end - (start + 1)));
}

static GstHLSMedia *
gst_m3u8_parse_media (gchar * desc, const gchar * base_uri)
{
  GstHLSMedia *media;
  gchar *a, *v;

  media = g_new0 (GstHLSMedia, 1);
  media->ref_count = 1;
  media->playlist = gst_m3u8_new ();
  media->mtype = GST_HLS_MEDIA_TYPE_INVALID;

  GST_LOG ("parsing %s", desc);
  while (desc != NULL && parse_attributes (&desc, &a, &v)) {
    if (strcmp (a, "TYPE") == 0) {
      media->mtype = gst_m3u8_get_hls_media_type_from_string (v);
    } else if (strcmp (a, "GROUP-ID") == 0) {
      g_free (media->group_id);
      media->group_id = gst_m3u8_unquote (v);
    } else if (strcmp (a, "NAME") == 0) {
      g_free (media->name);
      media->name = gst_m3u8_unquote (v);
    } else if (strcmp (a, "URI") == 0) {
      gchar *uri;

      g_free (media->uri);
      uri = gst_m3u8_unquote (v);
      media->uri = uri_join (base_uri, uri);
      g_free (uri);
    } else if (strcmp (a, "LANGUAGE") == 0) {
      g_free (media->lang);
      media->lang = gst_m3u8_unquote (v);
    } else if (strcmp (a, "DEFAULT") == 0) {
      media->is_default = g_ascii_strcasecmp (v, "yes") == 0;
    } else if (strcmp (a, "FORCED") == 0) {
      media->forced = g_ascii_strcasecmp (v, "yes") == 0;
    } else if (strcmp (a, "AUTOSELECT") == 0) {
      media->autoselect = g_ascii_strcasecmp (v, "yes") == 0;
    } else {
      /* unhandled: ASSOC-LANGUAGE, INSTREAM-ID, CHARACTERISTICS */
      GST_FIXME ("EXT-X-MEDIA: unhandled attribute: %s = %s", a, v);
    }
  }

  if (media->mtype == GST_HLS_MEDIA_TYPE_INVALID)
    goto required_attributes_missing;

  if (media->uri == NULL)
    goto existing_stream;

  if (media->group_id == NULL || media->name == NULL)
    goto required_attributes_missing;

  if (media->mtype == GST_HLS_MEDIA_TYPE_CLOSED_CAPTIONS && media->uri != NULL)
    goto uri_with_cc;

  GST_DEBUG ("media: %s, group '%s', name '%s', uri '%s', %s %s %s, lang=%s",
      GST_HLS_MEDIA_TYPE_NAME (media->mtype), media->group_id, media->name,
      media->uri, media->is_default ? "default" : "-",
      media->autoselect ? "autoselect" : "-",
      media->forced ? "forced" : "-", media->lang ? media->lang : "??");

  return media;

uri_with_cc:
  {
    GST_WARNING ("closed captions EXT-X-MEDIA should not have URI specified");
    goto out_error;
  }
required_attributes_missing:
  {
    GST_WARNING ("EXT-X-MEDIA description is missing required attributes");
    goto out_error;
    /* fall through */
  }
existing_stream:
  {
    GST_DEBUG ("EXT-X-MEDIA without URI, describes embedded stream, skipping");
    /* fall through */
  }

out_error:
  {
    gst_hls_media_unref (media);
    return NULL;
  }
}

static GstHLSVariantStream *
gst_hls_variant_stream_new (void)
{
  GstHLSVariantStream *stream;

  stream = g_new0 (GstHLSVariantStream, 1);
  stream->m3u8 = gst_m3u8_new ();
  stream->refcount = 1;
  return stream;
}

GstHLSVariantStream *
gst_hls_variant_stream_ref (GstHLSVariantStream * stream)
{
  g_atomic_int_inc (&stream->refcount);
  return stream;
}

void
gst_hls_variant_stream_unref (GstHLSVariantStream * stream)
{
  if (g_atomic_int_dec_and_test (&stream->refcount)) {
    gint i;

    g_free (stream->name);
    g_free (stream->uri);
    g_free (stream->codecs);
    gst_m3u8_unref (stream->m3u8);
    for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
      g_free (stream->media_groups[i]);
      g_list_free_full (stream->media[i], (GDestroyNotify) gst_hls_media_unref);
    }
    g_free (stream);
  }
}

static GstHLSVariantStream *
find_variant_stream_by_name (GList * list, const gchar * name)
{
  for (; list != NULL; list = list->next) {
    GstHLSVariantStream *variant_stream = list->data;

    if (variant_stream->name != NULL && !strcmp (variant_stream->name, name))
      return variant_stream;
  }
  return NULL;
}

static GstHLSVariantStream *
find_variant_stream_by_uri (GList * list, const gchar * uri)
{
  for (; list != NULL; list = list->next) {
    GstHLSVariantStream *variant_stream = list->data;

    if (variant_stream->uri != NULL && !strcmp (variant_stream->uri, uri))
      return variant_stream;
  }
  return NULL;
}

static GstHLSMasterPlaylist *
gst_hls_master_playlist_new (void)
{
  GstHLSMasterPlaylist *playlist;

  playlist = g_new0 (GstHLSMasterPlaylist, 1);
  playlist->refcount = 1;
  playlist->is_simple = FALSE;

  return playlist;
}

void
gst_hls_master_playlist_unref (GstHLSMasterPlaylist * playlist)
{
  if (g_atomic_int_dec_and_test (&playlist->refcount)) {
    g_list_free_full (playlist->variants,
        (GDestroyNotify) gst_hls_variant_stream_unref);
    g_list_free_full (playlist->iframe_variants,
        (GDestroyNotify) gst_hls_variant_stream_unref);
    if (playlist->default_variant)
      gst_hls_variant_stream_unref (playlist->default_variant);
    g_free (playlist->last_data);
    g_free (playlist);
  }
}

static gint
hls_media_name_compare_func (gconstpointer media, gconstpointer name)
{
  return strcmp (((GstHLSMedia *) media)->name, (const gchar *) name);
}

/* Takes ownership of @data */
GstHLSMasterPlaylist *
gst_hls_master_playlist_new_from_data (gchar * data, const gchar * base_uri)
{
  GHashTable *media_groups[GST_HLS_N_MEDIA_TYPES] = { NULL, };
  GstHLSMasterPlaylist *playlist;
  GstHLSVariantStream *pending_stream;
  gchar *end, *free_data = data;
  gint val, i;
  GList *l;

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    GST_WARNING ("Data doesn't start with #EXTM3U");
    g_free (free_data);
    return NULL;
  }

  playlist = gst_hls_master_playlist_new ();

  /* store data before we modify it for parsing */
  playlist->last_data = g_strdup (data);

  GST_TRACE ("data:\n%s", data);

  if (strstr (data, "\n#EXTINF:") != NULL) {
    GST_INFO ("This is a simple media playlist, not a master playlist");

    pending_stream = gst_hls_variant_stream_new ();
    pending_stream->name = g_strdup (base_uri);
    pending_stream->uri = g_strdup (base_uri);
    gst_m3u8_set_uri (pending_stream->m3u8, base_uri, NULL, base_uri);
    playlist->variants = g_list_append (playlist->variants, pending_stream);
    playlist->default_variant = gst_hls_variant_stream_ref (pending_stream);
    playlist->is_simple = TRUE;

    if (!gst_m3u8_update (pending_stream->m3u8, data)) {
      GST_WARNING ("Failed to parse media playlist");
      gst_hls_master_playlist_unref (playlist);
      playlist = NULL;
    }
    return playlist;
  }

  pending_stream = NULL;
  data += 7;
  while (TRUE) {
    gchar *r;

    end = g_utf8_strchr (data, -1, '\n');
    if (end)
      *end = '\0';

    r = g_utf8_strchr (data, -1, '\r');
    if (r)
      *r = '\0';

    if (data[0] != '#' && data[0] != '\0') {
      gchar *name, *uri;

      if (pending_stream == NULL) {
        GST_LOG ("%s: got line without EXT-STREAM-INF, dropping", data);
        goto next_line;
      }

      name = data;
      uri = uri_join (base_uri, name);
      if (uri == NULL)
        goto next_line;

      pending_stream->name = g_strdup (name);
      pending_stream->uri = uri;

      if (find_variant_stream_by_name (playlist->variants, name)
          || find_variant_stream_by_uri (playlist->variants, uri)) {
        GST_DEBUG ("Already have a list with this name or URI: %s", name);
        gst_hls_variant_stream_unref (pending_stream);
      } else {
        GST_INFO ("stream %s @ %u: %s", name, pending_stream->bandwidth, uri);
        gst_m3u8_set_uri (pending_stream->m3u8, uri, NULL, name);
        playlist->variants = g_list_append (playlist->variants, pending_stream);
        /* use first stream in the playlist as default */
        if (playlist->default_variant == NULL) {
          playlist->default_variant =
              gst_hls_variant_stream_ref (pending_stream);
        }
      }
      pending_stream = NULL;
    } else if (g_str_has_prefix (data, "#EXT-X-VERSION:")) {
      if (int_from_string (data + 15, &data, &val))
        playlist->version = val;
    } else if (g_str_has_prefix (data, "#EXT-X-STREAM-INF:") ||
        g_str_has_prefix (data, "#EXT-X-I-FRAME-STREAM-INF:")) {
      GstHLSVariantStream *stream;
      gchar *v, *a;

      stream = gst_hls_variant_stream_new ();
      stream->iframe = g_str_has_prefix (data, "#EXT-X-I-FRAME-STREAM-INF:");
      data += stream->iframe ? 26 : 18;
      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "BANDWIDTH")) {
          if (!stream->bandwidth) {
            if (!int_from_string (v, NULL, &stream->bandwidth))
              GST_WARNING ("Error while reading BANDWIDTH");
          }
        } else if (g_str_equal (a, "AVERAGE-BANDWIDTH")) {
          GST_DEBUG
              ("AVERAGE-BANDWIDTH attribute available. Using it as stream bandwidth");
          if (!int_from_string (v, NULL, &stream->bandwidth))
            GST_WARNING ("Error while reading AVERAGE-BANDWIDTH");
        } else if (g_str_equal (a, "PROGRAM-ID")) {
          if (!int_from_string (v, NULL, &stream->program_id))
            GST_WARNING ("Error while reading PROGRAM-ID");
        } else if (g_str_equal (a, "CODECS")) {
          g_free (stream->codecs);
          stream->codecs = g_strdup (v);
        } else if (g_str_equal (a, "RESOLUTION")) {
          if (!int_from_string (v, &v, &stream->width))
            GST_WARNING ("Error while reading RESOLUTION width");
          if (!v || *v != 'x') {
            GST_WARNING ("Missing height");
          } else {
            v = g_utf8_next_char (v);
            if (!int_from_string (v, NULL, &stream->height))
              GST_WARNING ("Error while reading RESOLUTION height");
          }
        } else if (stream->iframe && g_str_equal (a, "URI")) {
          stream->uri = uri_join (base_uri, v);
          if (stream->uri != NULL) {
            stream->name = g_strdup (stream->uri);
            gst_m3u8_set_uri (stream->m3u8, stream->uri, NULL, stream->name);
          } else {
            gst_hls_variant_stream_unref (stream);
          }
        } else if (g_str_equal (a, "AUDIO")) {
          g_free (stream->media_groups[GST_HLS_MEDIA_TYPE_AUDIO]);
          stream->media_groups[GST_HLS_MEDIA_TYPE_AUDIO] = gst_m3u8_unquote (v);
        } else if (g_str_equal (a, "SUBTITLES")) {
          g_free (stream->media_groups[GST_HLS_MEDIA_TYPE_SUBTITLES]);
          stream->media_groups[GST_HLS_MEDIA_TYPE_SUBTITLES] =
              gst_m3u8_unquote (v);
        } else if (g_str_equal (a, "VIDEO")) {
          g_free (stream->media_groups[GST_HLS_MEDIA_TYPE_VIDEO]);
          stream->media_groups[GST_HLS_MEDIA_TYPE_VIDEO] = gst_m3u8_unquote (v);
        } else if (g_str_equal (a, "CLOSED-CAPTIONS")) {
          /* closed captions will be embedded inside the video stream, ignore */
        }
      }

      if (stream->iframe) {
        if (find_variant_stream_by_uri (playlist->iframe_variants, stream->uri)) {
          GST_DEBUG ("Already have a list with this URI");
          gst_hls_variant_stream_unref (stream);
        } else {
          playlist->iframe_variants =
              g_list_append (playlist->iframe_variants, stream);
        }
      } else {
        if (pending_stream != NULL) {
          GST_WARNING ("variant stream without uri, dropping");
          gst_hls_variant_stream_unref (pending_stream);
        }
        pending_stream = stream;
      }
    } else if (g_str_has_prefix (data, "#EXT-X-MEDIA:")) {
      GstHLSMedia *media;
      GList *list;

      media = gst_m3u8_parse_media (data + strlen ("#EXT-X-MEDIA:"), base_uri);

      if (media == NULL)
        goto next_line;

      if (media_groups[media->mtype] == NULL) {
        media_groups[media->mtype] =
            g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      }

      list = g_hash_table_lookup (media_groups[media->mtype], media->group_id);

      /* make sure there isn't already a media with the same name */
      if (!g_list_find_custom (list, media->name, hls_media_name_compare_func)) {
        g_hash_table_replace (media_groups[media->mtype],
            g_strdup (media->group_id), g_list_append (list, media));
        GST_INFO ("Added media %s to group %s", media->name, media->group_id);
      } else {
        GST_WARNING ("  media with name '%s' already exists in group '%s'!",
            media->name, media->group_id);
        gst_hls_media_unref (media);
      }
    } else if (*data != '\0') {
      GST_LOG ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  if (pending_stream != NULL) {
    GST_WARNING ("#EXT-X-STREAM-INF without uri, dropping");
    gst_hls_variant_stream_unref (pending_stream);
  }

  g_free (free_data);

  /* Add alternative renditions media to variant streams */
  for (l = playlist->variants; l != NULL; l = l->next) {
    GstHLSVariantStream *stream = l->data;
    GList *mlist;

    for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
      if (stream->media_groups[i] != NULL && media_groups[i] != NULL) {
        GST_INFO ("Adding %s group '%s' to stream '%s'",
            GST_HLS_MEDIA_TYPE_NAME (i), stream->media_groups[i], stream->name);

        mlist = g_hash_table_lookup (media_groups[i], stream->media_groups[i]);

        if (mlist == NULL)
          GST_WARNING ("Group '%s' does not exist!", stream->media_groups[i]);

        while (mlist != NULL) {
          GstHLSMedia *media = mlist->data;

          GST_DEBUG ("  %s media %s, uri: %s", GST_HLS_MEDIA_TYPE_NAME (i),
              media->name, media->uri);

          stream->media[i] =
              g_list_append (stream->media[i], gst_hls_media_ref (media));
          mlist = mlist->next;
        }
      }
    }
  }

  /* clean up our temporary alternative rendition groups hash tables */
  for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
    if (media_groups[i] != NULL) {
      GList *groups, *mlist;

      groups = g_hash_table_get_keys (media_groups[i]);
      for (l = groups; l != NULL; l = l->next) {
        mlist = g_hash_table_lookup (media_groups[i], l->data);
        g_list_free_full (mlist, (GDestroyNotify) gst_hls_media_unref);
      }
      g_list_free (groups);
      g_hash_table_unref (media_groups[i]);
    }
  }

  if (playlist->variants == NULL) {
    GST_WARNING ("Master playlist without any media playlists!");
    gst_hls_master_playlist_unref (playlist);
    return NULL;
  }

  /* reorder variants by bitrate */
  playlist->variants =
      g_list_sort (playlist->variants,
      (GCompareFunc) gst_hls_variant_stream_compare_by_bitrate);

  playlist->iframe_variants =
      g_list_sort (playlist->iframe_variants,
      (GCompareFunc) gst_hls_variant_stream_compare_by_bitrate);

  /* FIXME: restore old current_variant after master playlist update
   * (move into code that does that update) */
#if 0
  {
    gchar *top_variant_uri = NULL;
    gboolean iframe = FALSE;

    if (!self->current_variant) {
      top_variant_uri = GST_M3U8 (self->lists->data)->uri;
    } else {
      top_variant_uri = GST_M3U8 (self->current_variant->data)->uri;
      iframe = GST_M3U8 (self->current_variant->data)->iframe;
    }

    /* here we sorted the lists */

    if (iframe)
      playlist->current_variant =
          find_variant_stream_by_uri (playlist->iframe_variants,
          top_variant_uri);
    else
      playlist->current_variant =
          find_variant_stream_by_uri (playlist->variants, top_variant_uri);
  }
#endif

  GST_DEBUG ("parsed master playlist with %d streams and %d I-frame streams",
      g_list_length (playlist->variants),
      g_list_length (playlist->iframe_variants));


  return playlist;
}

gboolean
gst_hls_variant_stream_is_live (GstHLSVariantStream * variant)
{
  gboolean is_live;

  g_return_val_if_fail (variant != NULL, FALSE);

  is_live = gst_m3u8_is_live (variant->m3u8);

  return is_live;
}

static gint
compare_media (const GstHLSMedia * a, const GstHLSMedia * b)
{
  return strcmp (a->name, b->name);
}

GstHLSMedia *
gst_hls_variant_find_matching_media (GstHLSVariantStream * stream,
    GstHLSMedia * media)
{
  GList *mlist = stream->media[media->mtype];
  GList *match;

  if (mlist == NULL)
    return NULL;

  match = g_list_find_custom (mlist, media, (GCompareFunc) compare_media);
  if (match == NULL)
    return NULL;

  return match->data;
}

GstHLSVariantStream *
gst_hls_master_playlist_get_variant_for_bitrate (GstHLSMasterPlaylist *
    playlist, GstHLSVariantStream * current_variant, guint bitrate)
{
  GstHLSVariantStream *variant = current_variant;
  GList *l;

  /* variant lists are sorted low to high, so iterate from highest to lowest */
  if (current_variant == NULL || !current_variant->iframe)
    l = g_list_last (playlist->variants);
  else
    l = g_list_last (playlist->iframe_variants);

  while (l != NULL) {
    variant = l->data;
    if (variant->bandwidth <= bitrate)
      break;
    l = l->prev;
  }

  return variant;
}

GstHLSVariantStream *
gst_hls_master_playlist_get_matching_variant (GstHLSMasterPlaylist * playlist,
    GstHLSVariantStream * current_variant)
{
  if (current_variant->iframe) {
    return find_variant_stream_by_uri (playlist->iframe_variants,
        current_variant->uri);
  }

  return find_variant_stream_by_uri (playlist->variants, current_variant->uri);
}
