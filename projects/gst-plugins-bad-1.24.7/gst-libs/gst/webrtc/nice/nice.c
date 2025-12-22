/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "nice.h"
#include "nicestream.h"
/* libnice */
#include <agent.h>

#define HTTP_PROXY_PORT_DEFAULT 3128

/* XXX:
 *
 * - are locally generated remote candidates meant to be readded to libnice?
 */

static GstUri *_validate_turn_server (GstWebRTCNice * ice, const gchar * s);

#define GST_CAT_DEFAULT gst_webrtc_nice_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_AGENT,
  PROP_ICE_TCP,
  PROP_ICE_UDP,
  PROP_MIN_RTP_PORT,
  PROP_MAX_RTP_PORT,
};

struct _GstWebRTCNicePrivate
{
  NiceAgent *nice_agent;

  GArray *nice_stream_map;

  GThread *thread;
  GMainContext *main_context;
  GMainLoop *loop;
  GMutex lock;
  GCond cond;

  GstWebRTCICEOnCandidateFunc on_candidate;
  gpointer on_candidate_data;
  GDestroyNotify on_candidate_notify;

  GstUri *stun_server;
  GstUri *turn_server;

  GHashTable *turn_servers;

  GstUri *http_proxy;
};

#define gst_webrtc_nice_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWebRTCNice, gst_webrtc_nice,
    GST_TYPE_WEBRTC_ICE, G_ADD_PRIVATE (GstWebRTCNice)
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_nice_debug, "webrtcnice", 0,
        "webrtcnice"););

static gboolean
_unlock_pc_thread (GMutex * lock)
{
  g_mutex_unlock (lock);
  return G_SOURCE_REMOVE;
}

static gpointer
_gst_nice_thread (GstWebRTCNice * ice)
{
  g_mutex_lock (&ice->priv->lock);
  ice->priv->main_context = g_main_context_new ();
  ice->priv->loop = g_main_loop_new (ice->priv->main_context, FALSE);

  g_cond_broadcast (&ice->priv->cond);
  g_main_context_invoke (ice->priv->main_context,
      (GSourceFunc) _unlock_pc_thread, &ice->priv->lock);

  g_main_context_push_thread_default (ice->priv->main_context);
  g_main_loop_run (ice->priv->loop);
  g_main_context_pop_thread_default (ice->priv->main_context);

  g_mutex_lock (&ice->priv->lock);
  g_main_context_unref (ice->priv->main_context);
  ice->priv->main_context = NULL;
  g_main_loop_unref (ice->priv->loop);
  ice->priv->loop = NULL;
  g_cond_broadcast (&ice->priv->cond);
  g_mutex_unlock (&ice->priv->lock);

  return NULL;
}

static void
_start_thread (GstWebRTCNice * ice)
{
  g_mutex_lock (&ice->priv->lock);
  ice->priv->thread = g_thread_new (GST_OBJECT_NAME (ice),
      (GThreadFunc) _gst_nice_thread, ice);

  while (!ice->priv->loop)
    g_cond_wait (&ice->priv->cond, &ice->priv->lock);
  g_mutex_unlock (&ice->priv->lock);
}

static void
_stop_thread (GstWebRTCNice * ice)
{
  g_mutex_lock (&ice->priv->lock);
  g_main_loop_quit (ice->priv->loop);
  while (ice->priv->loop)
    g_cond_wait (&ice->priv->cond, &ice->priv->lock);
  g_mutex_unlock (&ice->priv->lock);

  g_thread_unref (ice->priv->thread);
}

struct NiceStreamItem
{
  guint session_id;
  guint nice_stream_id;
  GstWebRTCICEStream *stream;
};

/* TRUE to continue, FALSE to stop */
typedef gboolean (*NiceStreamItemForeachFunc) (struct NiceStreamItem * item,
    gpointer user_data);

static void
_nice_stream_item_foreach (GstWebRTCNice * ice, NiceStreamItemForeachFunc func,
    gpointer data)
{
  int i, len;

  len = ice->priv->nice_stream_map->len;
  for (i = 0; i < len; i++) {
    struct NiceStreamItem *item =
        &g_array_index (ice->priv->nice_stream_map, struct NiceStreamItem,
        i);

    if (!func (item, data))
      break;
  }
}

/* TRUE for match, FALSE otherwise */
typedef gboolean (*NiceStreamItemFindFunc) (struct NiceStreamItem * item,
    gpointer user_data);

struct nice_find
{
  NiceStreamItemFindFunc func;
  gpointer data;
  struct NiceStreamItem *ret;
};

static gboolean
_find_nice_item (struct NiceStreamItem *item, gpointer user_data)
{
  struct nice_find *f = user_data;
  if (f->func (item, f->data)) {
    f->ret = item;
    return FALSE;
  }
  return TRUE;
}

static struct NiceStreamItem *
_nice_stream_item_find (GstWebRTCNice * ice, NiceStreamItemFindFunc func,
    gpointer data)
{
  struct nice_find f;

  f.func = func;
  f.data = data;
  f.ret = NULL;

  _nice_stream_item_foreach (ice, _find_nice_item, &f);

  return f.ret;
}

#define NICE_MATCH_INIT { -1, -1, NULL }

static gboolean
_match (struct NiceStreamItem *item, struct NiceStreamItem *m)
{
  if (m->session_id != -1 && m->session_id != item->session_id)
    return FALSE;
  if (m->nice_stream_id != -1 && m->nice_stream_id != item->nice_stream_id)
    return FALSE;
  if (m->stream != NULL && m->stream != item->stream)
    return FALSE;

  return TRUE;
}

static struct NiceStreamItem *
_find_item (GstWebRTCNice * ice, guint session_id, guint nice_stream_id,
    GstWebRTCICEStream * stream)
{
  struct NiceStreamItem m = NICE_MATCH_INIT;

  m.session_id = session_id;
  m.nice_stream_id = nice_stream_id;
  m.stream = stream;

  return _nice_stream_item_find (ice, (NiceStreamItemFindFunc) _match, &m);
}

static struct NiceStreamItem *
_create_nice_stream_item (GstWebRTCNice * ice, guint session_id)
{
  struct NiceStreamItem item;

  item.session_id = session_id;
  item.nice_stream_id = nice_agent_add_stream (ice->priv->nice_agent, 1);
  item.stream =
      GST_WEBRTC_ICE_STREAM (gst_webrtc_nice_stream_new (GST_WEBRTC_ICE (ice),
          item.nice_stream_id)
      );

  g_array_append_val (ice->priv->nice_stream_map, item);

  return _find_item (ice, item.session_id, item.nice_stream_id, item.stream);
}

static void
_parse_userinfo (const gchar * userinfo, gchar ** user, gchar ** pass)
{
  const gchar *colon;

  if (!userinfo) {
    *user = NULL;
    *pass = NULL;
    return;
  }

  colon = g_strstr_len (userinfo, -1, ":");
  if (!colon) {
    *user = g_uri_unescape_string (userinfo, NULL);
    *pass = NULL;
    return;
  }

  /* Check that the first occurence is also the last occurence */
  if (colon != g_strrstr (userinfo, ":"))
    GST_WARNING ("userinfo %s contains more than one ':', will assume that the "
        "first ':' delineates user:pass. You should escape the user and pass "
        "before adding to the URI.", userinfo);

  *user = g_uri_unescape_segment (userinfo, colon, NULL);
  *pass = g_uri_unescape_string (&colon[1], NULL);
}

typedef void (*GstResolvedCallback)
  (GstWebRTCNice * nice, GList * addresses, GError * error, gpointer user_data);

struct resolve_host_data
{
  GWeakRef nice_weak;
  char *host;
  gboolean main_context_handled;
  GstResolvedCallback resolved_callback;
  gpointer user_data;
  GDestroyNotify notify;
};

static struct resolve_host_data *
resolve_host_data_new (GstWebRTCNice * ice, const char *host)
{
  struct resolve_host_data *rh =
      g_atomic_rc_box_new0 (struct resolve_host_data);

  g_weak_ref_init (&rh->nice_weak, ice);
  rh->host = g_strdup (host);

  return rh;
}

static struct resolve_host_data *
resolve_host_data_ref (struct resolve_host_data *rh)
{
  return (struct resolve_host_data *) g_atomic_rc_box_acquire (rh);
}

static void
resolve_host_data_clear (struct resolve_host_data *rh)
{
  GST_TRACE ("Freeing data %p for resolving host %s", rh, rh->host);

  if (rh->notify)
    rh->notify (rh->user_data);

  g_weak_ref_clear (&rh->nice_weak);
  g_free (rh->host);
}

static void
resolve_host_data_unref (struct resolve_host_data *rh)
{
  g_atomic_rc_box_release_full (rh, (GDestroyNotify) resolve_host_data_clear);
}

static void
on_resolve_host (GResolver * resolver, GAsyncResult * res, gpointer user_data)
{
  struct resolve_host_data *rh = user_data;
  GstWebRTCNice *nice = g_weak_ref_get (&rh->nice_weak);
  GError *error = NULL;
  GList *addresses;

  if (!nice) {
    error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
    rh->resolved_callback (NULL, NULL, error, rh->user_data);
    resolve_host_data_unref (rh);
    g_error_free (error);

    return;
  }

  if (!(addresses = g_resolver_lookup_by_name_finish (resolver, res, &error))) {
    GST_ERROR ("failed to resolve: %s", error->message);

    rh->resolved_callback (nice, NULL, error, rh->user_data);
    gst_object_unref (nice);
    resolve_host_data_unref (rh);
    g_error_free (error);

    return;
  }

  GST_DEBUG_OBJECT (nice, "Resolved %d addresses for host %s with data %p",
      g_list_length (addresses), rh->host, rh);

  rh->resolved_callback (nice, addresses, error, rh->user_data);
  gst_object_unref (nice);
  resolve_host_data_unref (rh);
  g_resolver_free_addresses (addresses);
}

static gboolean
resolve_host_main_cb (gpointer user_data)
{
  GResolver *resolver = g_resolver_get_default ();
  struct resolve_host_data *rh = user_data;
  GstWebRTCNice *nice = g_weak_ref_get (&rh->nice_weak);

  if (nice) {
    /* no need to error anymore if the main context disappears and this task is
     * not run */
    rh->main_context_handled = TRUE;

    GST_DEBUG_OBJECT (nice, "Resolving host %s", rh->host);
    g_resolver_lookup_by_name_async (resolver, rh->host, NULL,
        (GAsyncReadyCallback) on_resolve_host, resolve_host_data_ref (rh));
    gst_object_unref (nice);
  }

  return G_SOURCE_REMOVE;
}

static void
error_resolve_if_unhandled (struct resolve_host_data *rh)
{
  GstWebRTCNice *nice = g_weak_ref_get (&rh->nice_weak);

  if (!rh->main_context_handled) {
    if (nice) {
      GST_DEBUG_OBJECT (nice, "host resolve for %s with data %p was never "
          "executed, main context quit?", rh->host, rh);
    } else {
      GST_DEBUG ("host resolve for %s with data %p was never "
          "executed, main context quit?", rh->host, rh);
    }

    GError *error =
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
    rh->resolved_callback (nice, NULL, error, rh->user_data);
    g_error_free (error);
  }

  if (nice)
    gst_object_unref (nice);
  resolve_host_data_unref (rh);
}

static void
resolve_host_async (GstWebRTCNice * nice, const gchar * host,
    GstResolvedCallback resolved_callback, gpointer user_data,
    GDestroyNotify notify)
{
  struct resolve_host_data *rh = resolve_host_data_new (nice, host);

  rh->resolved_callback = resolved_callback;
  rh->user_data = user_data;
  rh->notify = notify;

  GST_TRACE_OBJECT (nice, "invoking main context for resolving host %s "
      "with data %p", host, rh);
  g_main_context_invoke_full (nice->priv->main_context, G_PRIORITY_DEFAULT,
      resolve_host_main_cb, rh, (GDestroyNotify) error_resolve_if_unhandled);
}

static void
_add_turn_server (GstWebRTCNice * ice, struct NiceStreamItem *item,
    GstUri * turn_server)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  const gchar *host;
  NiceRelayType relays[4] = { 0, };
  gchar *user, *pass;
  const gchar *userinfo, *transport, *scheme;
  int i, relay_n = 0;

  host = gst_uri_get_host (turn_server);
  if (!host) {
    GST_ERROR_OBJECT (ice, "Turn server has no host");
    return;
  }

  scheme = gst_uri_get_scheme (turn_server);
  transport = gst_uri_get_query_value (turn_server, "transport");
  userinfo = gst_uri_get_userinfo (turn_server);
  _parse_userinfo (userinfo, &user, &pass);

  if (g_strcmp0 (scheme, "turns") == 0) {
    relays[relay_n++] = NICE_RELAY_TYPE_TURN_TLS;
  } else if (g_strcmp0 (scheme, "turn") == 0) {
    if (!transport || g_strcmp0 (transport, "udp") == 0)
      relays[relay_n++] = NICE_RELAY_TYPE_TURN_UDP;
    if (!transport || g_strcmp0 (transport, "tcp") == 0)
      relays[relay_n++] = NICE_RELAY_TYPE_TURN_TCP;
  }
  g_assert (relay_n < G_N_ELEMENTS (relays));

  for (i = 0; i < relay_n; i++) {
    if (!nice_agent_set_relay_info (nice->priv->nice_agent,
            item->nice_stream_id, NICE_COMPONENT_TYPE_RTP,
            gst_uri_get_host (turn_server), gst_uri_get_port (turn_server),
            user, pass, relays[i])) {
      gchar *uri_str = gst_uri_to_string (turn_server);
      GST_ERROR_OBJECT (ice, "Could not set TURN server %s on libnice",
          uri_str);
      g_free (uri_str);
    }
  }

  g_free (user);
  g_free (pass);

}

typedef struct
{
  GstWebRTCNice *ice;
  struct NiceStreamItem *item;
} AddTurnServerData;

static void
_add_turn_server_func (const gchar * uri, GstUri * turn_server,
    AddTurnServerData * data)
{
  _add_turn_server (data->ice, data->item, turn_server);
}

static void
_add_stun_server (GstWebRTCNice * ice, GstUri * stun_server)
{
  const gchar *msg = "must be of the form stun://<host>:<port>";
  const gchar *host;
  gchar *s = NULL;
  guint port;

  s = gst_uri_to_string (stun_server);
  GST_DEBUG_OBJECT (ice, "adding stun server, %s", s);

  host = gst_uri_get_host (stun_server);
  if (!host) {
    GST_ERROR_OBJECT (ice, "Stun server '%s' has no host, %s", s, msg);
    goto out;
  }

  port = gst_uri_get_port (stun_server);
  if (port == GST_URI_NO_PORT) {
    GST_INFO_OBJECT (ice, "Stun server '%s' has no port, assuming 3478", s);
    port = 3478;
    gst_uri_set_port (stun_server, port);
  }

  g_object_set (ice->priv->nice_agent, "stun-server", host,
      "stun-server-port", port, NULL);

out:
  g_free (s);
}

static GstWebRTCICEStream *
gst_webrtc_nice_add_stream (GstWebRTCICE * ice, guint session_id)
{
  struct NiceStreamItem m = NICE_MATCH_INIT;
  struct NiceStreamItem *item;
  AddTurnServerData add_data;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  m.session_id = session_id;
  item = _nice_stream_item_find (nice, (NiceStreamItemFindFunc) _match, &m);
  if (item) {
    GST_ERROR_OBJECT (nice, "stream already added with session_id=%u",
        session_id);
    return 0;
  }

  if (nice->priv->stun_server) {
    _add_stun_server (nice, nice->priv->stun_server);
  }

  item = _create_nice_stream_item (nice, session_id);

  if (nice->priv->turn_server) {
    _add_turn_server (nice, item, nice->priv->turn_server);
  }

  add_data.ice = nice;
  add_data.item = item;

  g_hash_table_foreach (nice->priv->turn_servers,
      (GHFunc) _add_turn_server_func, &add_data);

  return item->stream;
}

static void
_on_new_candidate (NiceAgent * agent, NiceCandidate * candidate,
    GstWebRTCNice * ice)
{
  struct NiceStreamItem *item;
  gchar *attr;

  item = _find_item (ice, -1, candidate->stream_id, NULL);
  if (!item) {
    GST_WARNING_OBJECT (ice, "received signal for non-existent stream %u",
        candidate->stream_id);
    return;
  }

  if (!candidate->username || !candidate->password) {
    gboolean got_credentials;
    gchar *ufrag, *password;

    got_credentials = nice_agent_get_local_credentials (ice->priv->nice_agent,
        candidate->stream_id, &ufrag, &password);
    g_warn_if_fail (got_credentials);

    if (!candidate->username)
      candidate->username = ufrag;
    else
      g_free (ufrag);

    if (!candidate->password)
      candidate->password = password;
    else
      g_free (password);
  }

  attr = nice_agent_generate_local_candidate_sdp (agent, candidate);

  if (ice->priv->on_candidate)
    ice->priv->on_candidate (GST_WEBRTC_ICE (ice), item->session_id, attr,
        ice->priv->on_candidate_data);

  g_free (attr);
}

static GstWebRTCICETransport *
gst_webrtc_nice_find_transport (GstWebRTCICE * ice, GstWebRTCICEStream * stream,
    GstWebRTCICEComponent component)
{
  struct NiceStreamItem *item;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  item = _find_item (nice, -1, -1, stream);
  g_return_val_if_fail (item != NULL, NULL);

  return gst_webrtc_ice_stream_find_transport (item->stream, component);
}

#if 0
/* TODO don't rely on libnice to (de)serialize candidates */
static NiceCandidateType
_candidate_type_from_string (const gchar * s)
{
  if (g_strcmp0 (s, "host") == 0) {
    return NICE_CANDIDATE_TYPE_HOST;
  } else if (g_strcmp0 (s, "srflx") == 0) {
    return NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
  } else if (g_strcmp0 (s, "prflx") == 0) {     /* FIXME: is the right string? */
    return NICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
  } else if (g_strcmp0 (s, "relay") == 0) {
    return NICE_CANDIDATE_TYPE_RELAY;
  } else {
    g_assert_not_reached ();
    return 0;
  }
}

static const gchar *
_candidate_type_to_string (NiceCandidateType type)
{
  switch (type) {
    case NICE_CANDIDATE_TYPE_HOST:
      return "host";
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
      return "srflx";
    case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
      return "prflx";
    case NICE_CANDIDATE_TYPE_RELAY:
      return "relay";
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static NiceCandidateTransport
_candidate_transport_from_string (const gchar * s)
{
  if (g_strcmp0 (s, "UDP") == 0) {
    return NICE_CANDIDATE_TRANSPORT_UDP;
  } else if (g_strcmp0 (s, "TCP tcptype") == 0) {
    return NICE_CANDIDATE_TRANSPORT_TCP_ACTIVE;
  } else if (g_strcmp0 (s, "tcp-passive") == 0) {       /* FIXME: is the right string? */
    return NICE_CANDIDATE_TRANSPORT_TCP_PASSIVE;
  } else if (g_strcmp0 (s, "tcp-so") == 0) {
    return NICE_CANDIDATE_TRANSPORT_TCP_SO;
  } else {
    g_assert_not_reached ();
    return 0;
  }
}

static const gchar *
_candidate_type_to_string (NiceCandidateType type)
{
  switch (type) {
    case NICE_CANDIDATE_TYPE_HOST:
      return "host";
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
      return "srflx";
    case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
      return "prflx";
    case NICE_CANDIDATE_TYPE_RELAY:
      return "relay";
    default:
      g_assert_not_reached ();
      return NULL;
  }
}
#endif

/* parse the address for possible resolution */
static gboolean
get_candidate_address (const gchar * candidate, gchar ** prefix,
    gchar ** address, gchar ** postfix)
{
  char **tokens = NULL;
  char *tmp_address = NULL;

  if (!g_str_has_prefix (candidate, "a=candidate:")) {
    GST_ERROR ("candidate \"%s\" does not start with \"a=candidate:\"",
        candidate);
    goto failure;
  }

  if (!(tokens = g_strsplit (candidate, " ", 6))) {
    GST_ERROR ("candidate \"%s\" could not be tokenized", candidate);
    goto failure;
  }

  if (g_strv_length (tokens) < 6) {
    GST_ERROR ("candidate \"%s\" tokenization resulted in not enough tokens",
        candidate);
    goto failure;
  }

  tmp_address = tokens[4];
  if (address)
    *address = g_strdup (tmp_address);
  tokens[4] = NULL;

  if (prefix)
    *prefix = g_strjoinv (" ", tokens);
  if (postfix)
    *postfix = g_strdup (tokens[5]);

  tokens[4] = tmp_address;

  g_strfreev (tokens);
  return TRUE;

failure:
  if (tokens)
    g_strfreev (tokens);
  return FALSE;
}

struct resolve_candidate_data
{
  guint nice_stream_id;
  char *prefix;
  char *postfix;
  GstPromise *promise;
};

static void
free_resolve_candidate_data (struct resolve_candidate_data *rc)
{
  g_free (rc->prefix);
  g_free (rc->postfix);
  if (rc->promise)
    gst_promise_unref (rc->promise);
  g_free (rc);
}

static void
add_ice_candidate_to_libnice (GstWebRTCICE * ice, guint nice_stream_id,
    NiceCandidate * cand)
{
  GSList *candidates = NULL;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  if (cand->component_id == 2) {
    /* we only support rtcp-mux so rtcp candidates are useless for us */
    GST_INFO_OBJECT (ice, "Dropping RTCP candidate");
    return;
  }

  candidates = g_slist_append (candidates, cand);

  nice_agent_set_remote_candidates (nice->priv->nice_agent, nice_stream_id,
      cand->component_id, candidates);

  g_slist_free (candidates);
}

static void
on_candidate_resolved (GstWebRTCNice * nice, GList * addresses,
    GError * error, gpointer user_data)
{
  struct resolve_candidate_data *rc = user_data;
  char *new_candv[4] = { NULL, };
  char *new_addr, *new_candidate;
  NiceCandidate *cand;

  if (!nice)
    error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

  if (error) {
    if (rc->promise) {
      GstStructure *s = gst_structure_new ("application/x-gst-promise", "error",
          G_TYPE_ERROR, error, NULL);
      gst_promise_reply (rc->promise, s);
    } else if (nice) {
      GST_WARNING_OBJECT (nice, "Could not resolve candidate address: %s",
          error->message);
    } else {
      GST_WARNING ("Could not resolve candidate address: %s", error->message);
    }

    if (!nice)
      g_clear_error (&error);

    return;
  }

  GstWebRTCICE *ice = GST_WEBRTC_ICE (nice);

  new_addr = g_inet_address_to_string (addresses->data);

  new_candv[0] = rc->prefix;
  new_candv[1] = new_addr;
  new_candv[2] = rc->postfix;
  new_candv[3] = NULL;
  new_candidate = g_strjoinv (" ", new_candv);

  GST_DEBUG_OBJECT (ice, "resolved to candidate %s", new_candidate);

  cand =
      nice_agent_parse_remote_candidate_sdp (nice->priv->nice_agent,
      rc->nice_stream_id, new_candidate);
  g_free (new_candidate);
  if (!cand) {
    if (rc->promise) {
      GError *error =
          g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE,
          "Could not parse candidate \'%s\'", new_candidate);
      GstStructure *s = gst_structure_new ("application/x-gst-promise", "error",
          G_TYPE_ERROR, error, NULL);
      gst_promise_reply (rc->promise, s);
      g_clear_error (&error);
    } else {
      GST_WARNING_OBJECT (ice, "Could not parse candidate \'%s\'",
          new_candidate);
    }
    return;
  }

  g_free (new_addr);

  add_ice_candidate_to_libnice (ice, rc->nice_stream_id, cand);
  nice_candidate_free (cand);
}

/* candidate must start with "a=candidate:" or be NULL*/
static void
gst_webrtc_nice_add_candidate (GstWebRTCICE * ice, GstWebRTCICEStream * stream,
    const gchar * candidate, GstPromise * promise)
{
  struct NiceStreamItem *item;
  NiceCandidate *cand;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  item = _find_item (nice, -1, -1, stream);
  g_return_if_fail (item != NULL);

  if (candidate == NULL) {
    nice_agent_peer_candidate_gathering_done (nice->priv->nice_agent,
        item->nice_stream_id);
    return;
  }

  cand =
      nice_agent_parse_remote_candidate_sdp (nice->priv->nice_agent,
      item->nice_stream_id, candidate);
  if (!cand) {
    /* might be a .local candidate */
    char *prefix = NULL, *address = NULL, *postfix = NULL;
    struct resolve_candidate_data *rc;

    if (!get_candidate_address (candidate, &prefix, &address, &postfix)) {
      if (promise) {
        GError *error =
            g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "Failed to retrieve address from candidate %s",
            candidate);
        GstStructure *s = gst_structure_new ("application/x-gst-promise",
            "error", G_TYPE_ERROR, error, NULL);
        gst_promise_reply (promise, s);
        g_clear_error (&error);
      } else {
        GST_WARNING_OBJECT (nice,
            "Failed to retrieve address from candidate %s", candidate);
      }
      goto done;
    }

    if (!g_str_has_suffix (address, ".local")) {
      if (promise) {
        GError *error =
            g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "candidate address \'%s\' does not end " "with \'.local\'",
            address);
        GstStructure *s = gst_structure_new ("application/x-gst-promise",
            "error", G_TYPE_ERROR, error, NULL);
        gst_promise_reply (promise, s);
        g_clear_error (&error);
      } else {
        GST_WARNING_OBJECT (nice,
            "candidate address \'%s\' does not end "
            "with \'.local\'", address);
      }
      goto done;
    }

    rc = g_new0 (struct resolve_candidate_data, 1);
    rc->nice_stream_id = item->nice_stream_id;
    rc->prefix = prefix;
    rc->postfix = postfix;
    rc->promise = promise ? gst_promise_ref (promise) : NULL;
    resolve_host_async (nice, address,
        on_candidate_resolved, rc,
        (GDestroyNotify) free_resolve_candidate_data);

    prefix = NULL;
    postfix = NULL;

  done:
    g_clear_pointer (&address, g_free);
    g_clear_pointer (&prefix, g_free);
    g_clear_pointer (&postfix, g_free);

    return;
  }

  add_ice_candidate_to_libnice (ice, item->nice_stream_id, cand);
  nice_candidate_free (cand);
}

static gboolean
gst_webrtc_nice_set_remote_credentials (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, const gchar * ufrag, const gchar * pwd)
{
  struct NiceStreamItem *item;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  g_return_val_if_fail (ufrag != NULL, FALSE);
  g_return_val_if_fail (pwd != NULL, FALSE);
  item = _find_item (nice, -1, -1, stream);
  g_return_val_if_fail (item != NULL, FALSE);

  GST_DEBUG_OBJECT (nice, "Setting remote ICE credentials on "
      "ICE stream %u ufrag:%s pwd:%s", item->nice_stream_id, ufrag, pwd);

  nice_agent_set_remote_credentials (nice->priv->nice_agent,
      item->nice_stream_id, ufrag, pwd);

  return TRUE;
}

typedef struct
{
  GstWebRTCNice *ice;
  GstUri *turn_server;
} AddTurnServerToStreamData;

static gboolean
_add_turn_server_foreach_stream_func (struct NiceStreamItem *item,
    gpointer data)
{
  AddTurnServerToStreamData *add_data = (AddTurnServerToStreamData *) data;
  _add_turn_server (add_data->ice, item, add_data->turn_server);
  return TRUE;
}

static gboolean
gst_webrtc_nice_add_turn_server (GstWebRTCICE * ice, const gchar * uri)
{
  gboolean ret = FALSE;
  GstUri *valid_uri;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  gboolean inserted;
  AddTurnServerToStreamData add_data;

  if (!(valid_uri = _validate_turn_server (nice, uri)))
    goto done;

  inserted =
      g_hash_table_insert (nice->priv->turn_servers, g_strdup (uri), valid_uri);

  /* add the turn server to any streams that were already created */
  if (inserted) {
    add_data.ice = nice;
    add_data.turn_server = valid_uri;
    _nice_stream_item_foreach (nice, _add_turn_server_foreach_stream_func,
        &add_data);
  }

  ret = TRUE;

done:
  return ret;
}

static gboolean
gst_webrtc_nice_add_local_ip_address (GstWebRTCNice * ice,
    const gchar * address)
{
  gboolean ret = FALSE;
  NiceAddress nice_addr;

  nice_address_init (&nice_addr);

  ret = nice_address_set_from_string (&nice_addr, address);

  if (ret) {
    ret = nice_agent_add_local_address (ice->priv->nice_agent, &nice_addr);
    if (!ret) {
      GST_ERROR_OBJECT (ice, "Failed to add local address to NiceAgent");
    }
  } else {
    GST_ERROR_OBJECT (ice, "Failed to initialize NiceAddress [%s]", address);
  }

  return ret;
}

static gboolean
gst_webrtc_nice_set_local_credentials (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, const gchar * ufrag, const gchar * pwd)
{
  struct NiceStreamItem *item;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  g_return_val_if_fail (ufrag != NULL, FALSE);
  g_return_val_if_fail (pwd != NULL, FALSE);
  item = _find_item (nice, -1, -1, stream);
  g_return_val_if_fail (item != NULL, FALSE);

  GST_DEBUG_OBJECT (nice, "Setting local ICE credentials on "
      "ICE stream %u ufrag:%s pwd:%s", item->nice_stream_id, ufrag, pwd);

  nice_agent_set_local_credentials (nice->priv->nice_agent,
      item->nice_stream_id, ufrag, pwd);

  return TRUE;
}

static gboolean
gst_webrtc_nice_gather_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  struct NiceStreamItem *item;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  item = _find_item (nice, -1, -1, stream);
  g_return_val_if_fail (item != NULL, FALSE);

  GST_DEBUG_OBJECT (nice, "gather candidates for stream %u",
      item->nice_stream_id);

  return gst_webrtc_ice_stream_gather_candidates (stream);
}

static void
gst_webrtc_nice_set_is_controller (GstWebRTCICE * ice, gboolean controller)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  g_object_set (G_OBJECT (nice->priv->nice_agent), "controlling-mode",
      controller, NULL);
}

static gboolean
gst_webrtc_nice_get_is_controller (GstWebRTCICE * ice)
{
  gboolean ret;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  g_object_get (G_OBJECT (nice->priv->nice_agent), "controlling-mode",
      &ret, NULL);
  return ret;
}

static void
gst_webrtc_nice_set_force_relay (GstWebRTCICE * ice, gboolean force_relay)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  g_object_set (G_OBJECT (nice->priv->nice_agent), "force-relay", force_relay,
      NULL);
}

static void
gst_webrtc_nice_set_on_ice_candidate (GstWebRTCICE * ice,
    GstWebRTCICEOnCandidateFunc func, gpointer user_data, GDestroyNotify notify)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  if (nice->priv->on_candidate_notify)
    nice->priv->on_candidate_notify (nice->priv->on_candidate_data);
  nice->priv->on_candidate = NULL;

  nice->priv->on_candidate = func;
  nice->priv->on_candidate_data = user_data;
  nice->priv->on_candidate_notify = notify;
}

static void
gst_webrtc_nice_set_tos (GstWebRTCICE * ice, GstWebRTCICEStream * stream,
    guint tos)
{
  struct NiceStreamItem *item;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  item = _find_item (nice, -1, -1, stream);
  g_return_if_fail (item != NULL);

  nice_agent_set_stream_tos (nice->priv->nice_agent, item->nice_stream_id, tos);
}

static const gchar *
_relay_type_to_string (GstUri * turn_server)
{
  const gchar *scheme;
  const gchar *transport;

  if (!turn_server)
    return "none";

  scheme = gst_uri_get_scheme (turn_server);
  transport = gst_uri_get_query_value (turn_server, "transport");

  if (g_strcmp0 (scheme, "turns") == 0) {
    return "tls";
  } else if (g_strcmp0 (scheme, "turn") == 0) {
    if (!transport || g_strcmp0 (transport, "udp") == 0)
      return "udp";
    if (!transport || g_strcmp0 (transport, "tcp") == 0)
      return "tcp";
  }

  return "none";
}

static gchar *
_get_server_url (GstWebRTCNice * ice, NiceCandidate * cand)
{
  switch (cand->type) {
    case NICE_CANDIDATE_TYPE_RELAYED:{
      NiceAddress addr;
      gchar ipaddr[NICE_ADDRESS_STRING_LEN];
      nice_candidate_relay_address (cand, &addr);
      nice_address_to_string (&addr, ipaddr);
      return g_strdup (ipaddr);
    }
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:{
      NiceAddress addr;
      gchar ipaddr[NICE_ADDRESS_STRING_LEN];
      if (nice_candidate_stun_server_address (cand, &addr)) {
        nice_address_to_string (&addr, ipaddr);
        return g_strdup (ipaddr);
      } else {
        return g_strdup (gst_uri_get_host (ice->priv->stun_server));
      }
      return g_strdup (gst_uri_get_host (ice->priv->stun_server));
    }
    default:
      return g_strdup ("");
  }
}

/* TODO: replace it with nice_candidate_type_to_string()
 * when it's ready for use
 * https://libnice.freedesktop.org/libnice/NiceCandidate.html#nice-candidate-type-to-string
 */
static const gchar *
_candidate_type_to_string (NiceCandidateType type)
{
  switch (type) {
    case NICE_CANDIDATE_TYPE_HOST:
      return "host";
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
      return "srflx";
    case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
      return "prflx";
    case NICE_CANDIDATE_TYPE_RELAYED:
      return "relay";
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static void
_populate_candidate_stats (GstWebRTCNice * ice, NiceCandidate * cand,
    GstWebRTCICEStream * stream, GstWebRTCICECandidateStats * stats,
    gboolean is_local)
{
  gchar ipaddr[INET6_ADDRSTRLEN];

  g_assert (cand != NULL);

  nice_address_to_string (&cand->addr, ipaddr);
  stats->port = nice_address_get_port (&cand->addr);
  stats->ipaddr = g_strdup (ipaddr);
  stats->stream_id = stream->stream_id;
  stats->type = _candidate_type_to_string (cand->type);
  stats->prio = cand->priority;
  stats->proto =
      cand->transport == NICE_CANDIDATE_TRANSPORT_UDP ? "udp" : "tcp";
  if (is_local) {
    if (cand->type == NICE_CANDIDATE_TYPE_RELAYED)
      stats->relay_proto = _relay_type_to_string (ice->priv->turn_server);
    stats->url = _get_server_url (ice, cand);
  }
}

static void
_populate_candidate_list_stats (GstWebRTCNice * ice, GSList * cands,
    GstWebRTCICEStream * stream, GPtrArray * result, gboolean is_local)
{
  GSList *item;

  for (item = cands; item != NULL; item = item->next) {
    GstWebRTCICECandidateStats *stats =
        g_malloc0 (sizeof (GstWebRTCICECandidateStats));
    NiceCandidate *c = item->data;
    _populate_candidate_stats (ice, c, stream, stats, is_local);
    g_ptr_array_add (result, stats);
  }

  g_ptr_array_add (result, NULL);
}

static GstWebRTCICECandidateStats **
gst_webrtc_nice_get_local_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  GSList *cands = NULL;

  /* TODO: Use a g_ptr_array_new_null_terminated once when we depend on GLib 2.74 */
  GPtrArray *result = g_ptr_array_new ();

  cands = nice_agent_get_local_candidates (nice->priv->nice_agent,
      stream->stream_id, NICE_COMPONENT_TYPE_RTP);

  _populate_candidate_list_stats (nice, cands, stream, result, TRUE);
  g_slist_free_full (cands, (GDestroyNotify) nice_candidate_free);

  return (GstWebRTCICECandidateStats **) g_ptr_array_free (result, FALSE);
}

static GstWebRTCICECandidateStats **
gst_webrtc_nice_get_remote_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  GSList *cands = NULL;

  /* TODO: Use a g_ptr_array_new_null_terminated once when we depend on GLib 2.74 */
  GPtrArray *result = g_ptr_array_new ();

  cands = nice_agent_get_remote_candidates (nice->priv->nice_agent,
      stream->stream_id, NICE_COMPONENT_TYPE_RTP);

  _populate_candidate_list_stats (nice, cands, stream, result, FALSE);
  g_slist_free_full (cands, (GDestroyNotify) nice_candidate_free);

  return (GstWebRTCICECandidateStats **) g_ptr_array_free (result, FALSE);
}

static gboolean
gst_webrtc_nice_get_selected_pair (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, GstWebRTCICECandidateStats ** local_stats,
    GstWebRTCICECandidateStats ** remote_stats)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  NiceCandidate *local_cand = NULL;
  NiceCandidate *remote_cand = NULL;


  if (stream) {
    if (nice_agent_get_selected_pair (nice->priv->nice_agent, stream->stream_id,
            NICE_COMPONENT_TYPE_RTP, &local_cand, &remote_cand)) {
      *local_stats = g_new0 (GstWebRTCICECandidateStats, 1);
      _populate_candidate_stats (nice, local_cand, stream, *local_stats, TRUE);

      *remote_stats = g_new0 (GstWebRTCICECandidateStats, 1);
      _populate_candidate_stats (nice, remote_cand, stream, *remote_stats,
          FALSE);

      return TRUE;
    }
  }

  return FALSE;
}

static void
_clear_ice_stream (struct NiceStreamItem *item)
{
  GstWebRTCNice *ice = NULL;

  if (!item)
    return;

  if (item->stream) {
    g_object_get (item->stream, "ice", &ice, NULL);

    if (ice != NULL) {
      g_signal_handlers_disconnect_by_data (ice->priv->nice_agent,
          item->stream);
      gst_object_unref (ice);
    }
    gst_object_unref (item->stream);
  }
}

static GstUri *
_validate_turn_server (GstWebRTCNice * ice, const gchar * s)
{
  GstUri *uri = gst_uri_from_string_escaped (s);
  const gchar *userinfo, *scheme;
  GList *keys = NULL, *l;
  gchar *user = NULL, *pass = NULL;
  gboolean turn_tls = FALSE;
  guint port;

  GST_DEBUG_OBJECT (ice, "validating turn server, %s", s);

  if (!uri) {
    GST_ERROR_OBJECT (ice, "Could not parse turn server '%s'", s);
    return NULL;
  }

  scheme = gst_uri_get_scheme (uri);
  if (g_strcmp0 (scheme, "turn") == 0) {
  } else if (g_strcmp0 (scheme, "turns") == 0) {
    turn_tls = TRUE;
  } else {
    GST_ERROR_OBJECT (ice, "unknown scheme '%s'", scheme);
    goto out;
  }

  keys = gst_uri_get_query_keys (uri);
  for (l = keys; l; l = l->next) {
    gchar *key = l->data;

    if (g_strcmp0 (key, "transport") == 0) {
      const gchar *transport = gst_uri_get_query_value (uri, "transport");
      if (!transport) {
      } else if (g_strcmp0 (transport, "udp") == 0) {
      } else if (g_strcmp0 (transport, "tcp") == 0) {
      } else {
        GST_ERROR_OBJECT (ice, "unknown transport value, '%s'", transport);
        goto out;
      }
    } else {
      GST_ERROR_OBJECT (ice, "unknown query key, '%s'", key);
      goto out;
    }
  }

  /* TODO: Implement error checking similar to the stun server below */
  userinfo = gst_uri_get_userinfo (uri);
  _parse_userinfo (userinfo, &user, &pass);
  if (!user) {
    GST_ERROR_OBJECT (ice, "No username specified in '%s'", s);
    goto out;
  }
  if (!pass) {
    GST_ERROR_OBJECT (ice, "No password specified in '%s'", s);
    goto out;
  }

  port = gst_uri_get_port (uri);

  if (port == GST_URI_NO_PORT) {
    if (turn_tls) {
      gst_uri_set_port (uri, 5349);
    } else {
      gst_uri_set_port (uri, 3478);
    }
  }

  g_list_free (keys);
  g_free (user);
  g_free (pass);

  return uri;

out:
  g_list_free (keys);
  g_free (user);
  g_free (pass);
  gst_uri_unref (uri);

  return NULL;
}

static void
on_http_proxy_resolved (GstWebRTCNice * nice, GList * addresses,
    GError * error, gpointer user_data)
{
  GstUri *uri = user_data;
  const gchar *userinfo;
  gchar *user = NULL;
  gchar *pass = NULL;
  const gchar *alpn = NULL;
  gchar *ip = NULL;
  guint port = GST_URI_NO_PORT;
  GHashTable *extra_headers;

  if (error) {
    if (nice) {
      GST_WARNING_OBJECT (nice, "Failed to resolve http proxy: %s",
          error->message);
    } else {
      GST_WARNING ("Failed to resolve http proxy: %s", error->message);
    }

    return;
  }

  if (!nice) {
    GST_WARNING ("Missing GstWebRTCNice instance");
    return;
  }

  GstWebRTCICE *ice = GST_WEBRTC_ICE (nice);

  /* XXX: only the first IP is used */
  ip = g_inet_address_to_string (addresses->data);

  if (!ip) {
    GST_ERROR_OBJECT (ice, "failed to resolve host for proxy");
    gst_uri_unref (uri);
    return;
  }

  port = gst_uri_get_port (uri);
  if (port == GST_URI_NO_PORT) {
    port = HTTP_PROXY_PORT_DEFAULT;
    GST_DEBUG_OBJECT (ice, "Proxy server has no port, assuming %u",
        HTTP_PROXY_PORT_DEFAULT);
  }

  userinfo = gst_uri_get_userinfo (uri);
  _parse_userinfo (userinfo, &user, &pass);

  alpn = gst_uri_get_query_value (uri, "alpn");
  if (!alpn) {
    alpn = "webrtc";
  }
  extra_headers = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, g_free);
  g_hash_table_insert (extra_headers, g_strdup ("ALPN"), g_strdup (alpn));

  g_object_set (nice->priv->nice_agent,
      "proxy-ip", ip, "proxy-port", port, "proxy-type", NICE_PROXY_TYPE_HTTP,
      "proxy-username", user, "proxy-password", pass, "proxy-extra-headers",
      extra_headers, NULL);

  g_free (ip);
  g_free (user);
  g_free (pass);
  g_hash_table_unref (extra_headers);
}

static GstUri *
_set_http_proxy (GstWebRTCICE * ice, const gchar * s)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  GstUri *uri = gst_uri_from_string_escaped (s);
  const gchar *msg =
      "must be of the form http://[username:password@]<host>[:<port>]";
  const gchar *host = NULL;
  const gchar *userinfo;
  gchar *user = NULL, *pass = NULL;

  GST_DEBUG_OBJECT (ice, "setting http proxy %s", s);

  if (!uri) {
    GST_ERROR_OBJECT (ice, "Couldn't parse http proxy uri '%s', %s", s, msg);
    return NULL;
  }

  if (g_strcmp0 (gst_uri_get_scheme (uri), "http") != 0) {
    GST_ERROR_OBJECT (ice,
        "Couldn't parse uri scheme for http proxy server '%s', %s", s, msg);
    gst_uri_unref (uri);
    return NULL;
  }

  host = gst_uri_get_host (uri);
  if (!host) {
    GST_ERROR_OBJECT (ice, "http proxy server '%s' has no host, %s", s, msg);
    gst_uri_unref (uri);
    return NULL;
  }

  userinfo = gst_uri_get_userinfo (uri);
  _parse_userinfo (userinfo, &user, &pass);
  if ((pass && pass[0] != '\0') && (!user || user[0] == '\0')) {
    GST_ERROR_OBJECT (ice,
        "Password specified without user for http proxy '%s', %s", s, msg);
    uri = NULL;
    goto out;
  }

  resolve_host_async (nice, host, on_http_proxy_resolved,
      gst_uri_ref (uri), (GDestroyNotify) gst_uri_unref);

out:
  g_free (user);
  g_free (pass);

  return uri;
}

static void
gst_webrtc_nice_set_stun_server (GstWebRTCICE * ice, const gchar * uri_s)
{
  GstUri *uri = gst_uri_from_string_escaped (uri_s);
  const gchar *msg = "must be of the form stun://<host>:<port>";
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  GST_DEBUG_OBJECT (nice, "setting stun server, %s", uri_s);

  if (!uri) {
    GST_ERROR_OBJECT (nice, "Couldn't parse stun server '%s', %s", uri_s, msg);
    return;
  }

  if (nice->priv->stun_server)
    gst_uri_unref (nice->priv->stun_server);
  nice->priv->stun_server = uri;
}

static gchar *
gst_webrtc_nice_get_stun_server (GstWebRTCICE * ice)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  if (nice->priv->stun_server)
    return gst_uri_to_string (nice->priv->stun_server);
  else
    return NULL;
}

static void
gst_webrtc_nice_set_turn_server (GstWebRTCICE * ice, const gchar * uri_s)
{
  GstUri *uri;
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  uri = _validate_turn_server (nice, uri_s);

  if (uri) {
    if (nice->priv->turn_server)
      gst_uri_unref (nice->priv->turn_server);
    nice->priv->turn_server = uri;
  }
}

static gchar *
gst_webrtc_nice_get_turn_server (GstWebRTCICE * ice)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  if (nice->priv->turn_server)
    return gst_uri_to_string (nice->priv->turn_server);
  else
    return NULL;
}

static void
gst_webrtc_nice_set_http_proxy (GstWebRTCICE * ice, const gchar * http_proxy)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);
  GstUri *uri = _set_http_proxy (ice, http_proxy);

  if (uri) {
    if (nice->priv->http_proxy)
      gst_uri_unref (nice->priv->http_proxy);
    nice->priv->http_proxy = uri;
  }
}

static gchar *
gst_webrtc_nice_get_http_proxy (GstWebRTCICE * ice)
{
  GstWebRTCNice *nice = GST_WEBRTC_NICE (ice);

  if (nice->priv->http_proxy)
    return gst_uri_to_string (nice->priv->http_proxy);
  else
    return NULL;
}

static void
gst_webrtc_nice_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCICE *ice = GST_WEBRTC_ICE (object);
  GstWebRTCNice *nice = GST_WEBRTC_NICE (object);

  switch (prop_id) {
    case PROP_ICE_TCP:
      g_object_set_property (G_OBJECT (nice->priv->nice_agent),
          "ice-tcp", value);
      break;
    case PROP_ICE_UDP:
      g_object_set_property (G_OBJECT (nice->priv->nice_agent),
          "ice-udp", value);
      break;
    case PROP_MIN_RTP_PORT:
      ice->min_rtp_port = g_value_get_uint (value);
      if (ice->min_rtp_port > ice->max_rtp_port)
        g_warning ("Set min-rtp-port to %u which is larger than"
            " max-rtp-port %u", ice->min_rtp_port, ice->max_rtp_port);
      break;
    case PROP_MAX_RTP_PORT:
      ice->max_rtp_port = g_value_get_uint (value);
      if (ice->min_rtp_port > ice->max_rtp_port)
        g_warning ("Set max-rtp-port to %u which is smaller than"
            " min-rtp-port %u", ice->max_rtp_port, ice->min_rtp_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_nice_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCICE *ice = GST_WEBRTC_ICE (object);
  GstWebRTCNice *nice = GST_WEBRTC_NICE (object);

  switch (prop_id) {
    case PROP_AGENT:
      g_value_set_object (value, nice->priv->nice_agent);
      break;
    case PROP_ICE_TCP:
      g_object_get_property (G_OBJECT (nice->priv->nice_agent),
          "ice-tcp", value);
      break;
    case PROP_ICE_UDP:
      g_object_get_property (G_OBJECT (nice->priv->nice_agent),
          "ice-udp", value);
      break;
    case PROP_MIN_RTP_PORT:
      g_value_set_uint (value, ice->min_rtp_port);
      break;
    case PROP_MAX_RTP_PORT:
      g_value_set_uint (value, ice->max_rtp_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_nice_finalize (GObject * object)
{
  GstWebRTCNice *ice = GST_WEBRTC_NICE (object);

  g_signal_handlers_disconnect_by_data (ice->priv->nice_agent, ice);

  _stop_thread (ice);

  if (ice->priv->on_candidate_notify)
    ice->priv->on_candidate_notify (ice->priv->on_candidate_data);
  ice->priv->on_candidate = NULL;
  ice->priv->on_candidate_notify = NULL;

  if (ice->priv->turn_server)
    gst_uri_unref (ice->priv->turn_server);
  if (ice->priv->stun_server)
    gst_uri_unref (ice->priv->stun_server);
  if (ice->priv->http_proxy)
    gst_uri_unref (ice->priv->http_proxy);

  g_mutex_clear (&ice->priv->lock);
  g_cond_clear (&ice->priv->cond);

  g_array_free (ice->priv->nice_stream_map, TRUE);

  g_object_unref (ice->priv->nice_agent);

  g_hash_table_unref (ice->priv->turn_servers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_webrtc_nice_constructed (GObject * object)
{
  GstWebRTCNice *ice = GST_WEBRTC_NICE (object);
  NiceAgentOption options = 0;

  _start_thread (ice);

  options |= NICE_AGENT_OPTION_ICE_TRICKLE;
  options |= NICE_AGENT_OPTION_REGULAR_NOMINATION;

/*  https://gitlab.freedesktop.org/libnice/libnice/-/merge_requests/257 */
#if HAVE_LIBNICE_CONSENT_FIX
  options |= NICE_AGENT_OPTION_CONSENT_FRESHNESS;
#endif

  ice->priv->nice_agent = nice_agent_new_full (ice->priv->main_context,
      NICE_COMPATIBILITY_RFC5245, options);
  g_signal_connect (ice->priv->nice_agent, "new-candidate-full",
      G_CALLBACK (_on_new_candidate), ice);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_webrtc_nice_class_init (GstWebRTCNiceClass * klass)
{
  GstWebRTCICEClass *gst_webrtc_ice_class = GST_WEBRTC_ICE_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  // override virtual functions
  gst_webrtc_ice_class->add_candidate = gst_webrtc_nice_add_candidate;
  gst_webrtc_ice_class->add_stream = gst_webrtc_nice_add_stream;
  gst_webrtc_ice_class->add_turn_server = gst_webrtc_nice_add_turn_server;
  gst_webrtc_ice_class->find_transport = gst_webrtc_nice_find_transport;
  gst_webrtc_ice_class->gather_candidates = gst_webrtc_nice_gather_candidates;
  gst_webrtc_ice_class->get_is_controller = gst_webrtc_nice_get_is_controller;
  gst_webrtc_ice_class->get_stun_server = gst_webrtc_nice_get_stun_server;
  gst_webrtc_ice_class->get_turn_server = gst_webrtc_nice_get_turn_server;
  gst_webrtc_ice_class->get_http_proxy = gst_webrtc_nice_get_http_proxy;
  gst_webrtc_ice_class->set_force_relay = gst_webrtc_nice_set_force_relay;
  gst_webrtc_ice_class->set_is_controller = gst_webrtc_nice_set_is_controller;
  gst_webrtc_ice_class->set_local_credentials =
      gst_webrtc_nice_set_local_credentials;
  gst_webrtc_ice_class->set_remote_credentials =
      gst_webrtc_nice_set_remote_credentials;
  gst_webrtc_ice_class->set_stun_server = gst_webrtc_nice_set_stun_server;
  gst_webrtc_ice_class->set_tos = gst_webrtc_nice_set_tos;
  gst_webrtc_ice_class->set_turn_server = gst_webrtc_nice_set_turn_server;
  gst_webrtc_ice_class->set_http_proxy = gst_webrtc_nice_set_http_proxy;
  gst_webrtc_ice_class->set_on_ice_candidate =
      gst_webrtc_nice_set_on_ice_candidate;
  gst_webrtc_ice_class->get_local_candidates =
      gst_webrtc_nice_get_local_candidates;
  gst_webrtc_ice_class->get_remote_candidates =
      gst_webrtc_nice_get_remote_candidates;
  gst_webrtc_ice_class->get_selected_pair = gst_webrtc_nice_get_selected_pair;

  gobject_class->constructed = gst_webrtc_nice_constructed;
  gobject_class->get_property = gst_webrtc_nice_get_property;
  gobject_class->set_property = gst_webrtc_nice_set_property;
  gobject_class->finalize = gst_webrtc_nice_finalize;

  g_object_class_install_property (gobject_class,
      PROP_AGENT,
      g_param_spec_object ("agent", "ICE agent",
          "ICE agent in use by this object. WARNING! Accessing this property "
          "may have disastrous consequences for the operation of webrtcbin. "
          "Other ICE implementations may not have the same interface.",
          NICE_TYPE_AGENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_TCP,
      g_param_spec_boolean ("ice-tcp", "ICE TCP",
          "Whether the agent should use ICE-TCP when gathering candidates",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_UDP,
      g_param_spec_boolean ("ice-udp", "ICE UDP",
          "Whether the agent should use ICE-UDP when gathering candidates",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_signal_override_class_handler ("add-local-ip-address",
      G_TYPE_FROM_CLASS (klass),
      G_CALLBACK (gst_webrtc_nice_add_local_ip_address));
}

static void
gst_webrtc_nice_init (GstWebRTCNice * ice)
{
  ice->priv = gst_webrtc_nice_get_instance_private (ice);

  g_mutex_init (&ice->priv->lock);
  g_cond_init (&ice->priv->cond);

  ice->priv->turn_servers =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) gst_uri_unref);

  ice->priv->nice_stream_map =
      g_array_new (FALSE, TRUE, sizeof (struct NiceStreamItem));
  g_array_set_clear_func (ice->priv->nice_stream_map,
      (GDestroyNotify) _clear_ice_stream);
}

GstWebRTCNice *
gst_webrtc_nice_new (const gchar * name)
{
  return g_object_new (GST_TYPE_WEBRTC_NICE, "name", name, NULL);
}
