/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2022 YouView TV Ltd.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 */

#include "config.h"
#include "glib.h"

#include "gtlssessioncache.h"

#include <errno.h>
#include <string.h>

/* Session cache support. We try to be careful of TLS session tracking
 * and so have adopted the recommendations of arXiv:1810.07304 section 6
 * in using a 10-minute cache lifetime and in never updating the
 * expiration time of cache entries when they are accessed to ensure a
 * new session gets used after 10 minutes even if the cached one was
 * resumed more recently.
 *
 * https://arxiv.org/abs/1810.07304
 */

G_LOCK_DEFINE_STATIC (session_cache_lock);
static GHashTable *client_session_cache; /* (owned) GString -> (owned) GTlsCacheData */

#define SESSION_CACHE_MAX_SIZE 50
#define SESSION_CACHE_MAX_AGE (10ll * 60ll * G_USEC_PER_SEC) /* ten minutes */

typedef struct {
  gpointer       tls1_2_session_ticket;
  GQueue        *tls1_3_session_tickets;
  gint64         expiration_time;
  SessionDup     session_dup;
  SessionAcquire inc_ref;
  SessionRelease dec_ref;
} GTlsCacheData;

static void
session_cache_cleanup (GHashTable *cache)
{
  gint64 time;
  GHashTableIter iter;
  gpointer key, value;
  GTlsCacheData *cache_data;
  gchar *session_id = NULL;
  gint64 oldest_expiration_time = INT_MAX;
  gboolean removed = FALSE;

  time = g_get_monotonic_time ();

  g_hash_table_iter_init (&iter, cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      cache_data = value;
      if (cache_data->expiration_time < oldest_expiration_time)
        {
          oldest_expiration_time = cache_data->expiration_time;
          session_id = key;
        }

      if (time > cache_data->expiration_time)
        {
          removed = TRUE;
          g_hash_table_iter_remove (&iter);
        }
    }

  if (!removed && session_id)
    g_hash_table_remove (cache, session_id);
}

static void
cache_data_free (GTlsCacheData *data)
{
  g_queue_free_full (data->tls1_3_session_tickets, (GDestroyNotify)data->dec_ref);

  if (data->dec_ref && data->tls1_2_session_ticket)
    data->dec_ref (data->tls1_2_session_ticket);

  g_free (data);
}

static GHashTable *
get_session_cache (gboolean create)
{
  if (!client_session_cache && create)
    {
      client_session_cache = g_hash_table_new_full ((GHashFunc)g_str_hash,
                                                    (GEqualFunc)g_str_equal,
                                                    (GDestroyNotify)g_free,
                                                    (GDestroyNotify)cache_data_free);
    }
  return client_session_cache;
}

void
g_tls_store_session_data (gchar              *session_id,
                          gpointer            session_data,
                          SessionDup          session_dup,
                          SessionAcquire      inc_ref,
                          SessionRelease      dec_ref,
                          GTlsProtocolVersion protocol_version)
{
  gpointer session_data_tmp = NULL;
  GTlsCacheData *cache_data;
  GHashTable *cache;

  if (!session_id || !session_data)
    return;

  G_LOCK (session_cache_lock);

  cache = get_session_cache (TRUE);
  cache_data = g_hash_table_lookup (cache, session_id);
  if (!cache_data)
    {
      if (g_hash_table_size (cache) >= SESSION_CACHE_MAX_SIZE)
        session_cache_cleanup (cache);

      cache_data = g_new (GTlsCacheData, 1);
      cache_data->tls1_2_session_ticket = NULL;
      cache_data->tls1_3_session_tickets = g_queue_new ();
      cache_data->inc_ref = inc_ref;
      cache_data->dec_ref = dec_ref;
      cache_data->expiration_time = g_get_monotonic_time () + SESSION_CACHE_MAX_AGE;
      g_hash_table_insert (cache, g_strdup (session_id), cache_data);
  }

  if (session_dup)
    session_data_tmp = session_dup (session_data);

  g_assert (session_data_tmp);

  if ((protocol_version >= G_TLS_PROTOCOL_VERSION_TLS_1_3 &&
       protocol_version < G_TLS_PROTOCOL_VERSION_DTLS_1_0) ||
      protocol_version > G_TLS_PROTOCOL_VERSION_DTLS_1_2)
    {
      g_queue_push_tail (cache_data->tls1_3_session_tickets, session_data_tmp);
    }
  else
    {
      if (cache_data->dec_ref && cache_data->tls1_2_session_ticket)
        dec_ref (cache_data->tls1_2_session_ticket);

      cache_data->tls1_2_session_ticket = session_data_tmp;
    }

  G_UNLOCK (session_cache_lock);
}

gpointer
g_tls_lookup_session_data (gchar *session_id)
{
  GTlsCacheData *cache_data;
  gpointer session_data = NULL;
  GHashTable *cache;

  if (!session_id)
    return NULL;

  G_LOCK (session_cache_lock);

  cache = get_session_cache (FALSE);
  if (cache)
    {
      cache_data = g_hash_table_lookup (cache, session_id);
      if (cache_data)
        {
          if (g_get_monotonic_time () > cache_data->expiration_time)
            {
              g_hash_table_remove (cache, session_id);
              G_UNLOCK (session_cache_lock);
              return NULL;
            }

          /* Note that session tickets should be used only once since TLS 1.3,
           * so we remove from the queue after retrieval. See RFC 8446 §C.4.
           */
          session_data = g_queue_pop_head (cache_data->tls1_3_session_tickets);

          if (!session_data)
            {
              session_data = cache_data->tls1_2_session_ticket;
              if (session_data && cache_data->inc_ref && !cache_data->inc_ref (session_data))
                {
                  g_debug ("Failed to acquire cached TLS session, will not try to resume session");
                  session_data = NULL;
                }
            }

          /* If the session data is NULL at this point, we do not have a valid
           * session stored, so we can remove this entry from the cache
           */
          if (!session_data)
            g_hash_table_remove (cache, session_id);
        }
    }

  G_UNLOCK (session_cache_lock);

  return session_data;
}
