/*
 * Copyright (c) 2015, Collabora Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sctpassociation.h"

#include <gst/gst.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (gst_sctp_association_debug_category);
#define GST_CAT_DEFAULT gst_sctp_association_debug_category
GST_DEBUG_CATEGORY_STATIC (gst_sctp_debug_category);

#define GST_SCTP_ASSOCIATION_STATE_TYPE (gst_sctp_association_state_get_type())
static GType
gst_sctp_association_state_get_type (void)
{
  static const GEnumValue values[] = {
    {GST_SCTP_ASSOCIATION_STATE_NEW, "state-new", "state-new"},
    {GST_SCTP_ASSOCIATION_STATE_READY, "state-ready", "state-ready"},
    {GST_SCTP_ASSOCIATION_STATE_CONNECTING, "state-connecting",
        "state-connecting"},
    {GST_SCTP_ASSOCIATION_STATE_CONNECTED, "state-connected",
        "state-connected"},
    {GST_SCTP_ASSOCIATION_STATE_DISCONNECTING, "state-disconnecting",
        "state-disconnecting"},
    {GST_SCTP_ASSOCIATION_STATE_DISCONNECTED, "state-disconnected",
        "state-disconnected"},
    {GST_SCTP_ASSOCIATION_STATE_ERROR, "state-error", "state-error"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;
    _id = g_enum_register_static ("GstSctpAssociationState", values);
    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

G_DEFINE_TYPE (GstSctpAssociation, gst_sctp_association, G_TYPE_OBJECT);

enum
{
  SIGNAL_STREAM_RESET,
  LAST_SIGNAL
};


enum
{
  PROP_0,

  PROP_ASSOCIATION_ID,
  PROP_LOCAL_PORT,
  PROP_REMOTE_PORT,
  PROP_STATE,
  PROP_USE_SOCK_STREAM,

  NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_NUMBER_OF_SCTP_STREAMS 1024
#define DEFAULT_LOCAL_SCTP_PORT 0
#define DEFAULT_REMOTE_SCTP_PORT 0

static GHashTable *associations = NULL;
G_LOCK_DEFINE_STATIC (associations_lock);
static guint32 number_of_associations = 0;

/* Interface implementations */
static void gst_sctp_association_finalize (GObject * object);
static void gst_sctp_association_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sctp_association_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static struct socket *create_sctp_socket (GstSctpAssociation *
    gst_sctp_association);
static struct sockaddr_conn get_sctp_socket_address (GstSctpAssociation *
    gst_sctp_association, guint16 port);
static gboolean client_role_connect (GstSctpAssociation * self);
static int sctp_packet_out (void *addr, void *buffer, size_t length, guint8 tos,
    guint8 set_df);
static int receive_cb (struct socket *sock, union sctp_sockstore addr,
    void *data, size_t datalen, struct sctp_rcvinfo rcv_info, gint flags,
    void *ulp_info);
static void handle_notification (GstSctpAssociation * self,
    const union sctp_notification *notification, size_t length);
static void handle_association_changed (GstSctpAssociation * self,
    const struct sctp_assoc_change *sac);
static void handle_stream_reset_event (GstSctpAssociation * self,
    const struct sctp_stream_reset_event *ssr);
static void handle_message (GstSctpAssociation * self, guint8 * data,
    guint32 datalen, guint16 stream_id, guint32 ppid);

static void maybe_set_state_to_ready (GstSctpAssociation * self);
static gboolean gst_sctp_association_change_state (GstSctpAssociation * self,
    GstSctpAssociationState new_state, gboolean lock);

static void
gst_sctp_association_class_init (GstSctpAssociationClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_sctp_association_finalize;
  gobject_class->set_property = gst_sctp_association_set_property;
  gobject_class->get_property = gst_sctp_association_get_property;

  signals[SIGNAL_STREAM_RESET] =
      g_signal_new ("stream-reset", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GstSctpAssociationClass,
          on_sctp_stream_reset), NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  properties[PROP_ASSOCIATION_ID] = g_param_spec_uint ("association-id",
      "The SCTP association-id", "The SCTP association-id.", 0, G_MAXUSHORT,
      DEFAULT_LOCAL_SCTP_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LOCAL_PORT] = g_param_spec_uint ("local-port", "Local SCTP",
      "The local SCTP port for this association", 0, G_MAXUSHORT,
      DEFAULT_LOCAL_SCTP_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_REMOTE_PORT] =
      g_param_spec_uint ("remote-port", "Remote SCTP",
      "The remote SCTP port for this association", 0, G_MAXUSHORT,
      DEFAULT_LOCAL_SCTP_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_STATE] = g_param_spec_enum ("state", "SCTP Association state",
      "The state of the SCTP association", GST_SCTP_ASSOCIATION_STATE_TYPE,
      GST_SCTP_ASSOCIATION_STATE_NEW,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_USE_SOCK_STREAM] =
      g_param_spec_boolean ("use-sock-stream", "Use sock-stream",
      "When set to TRUE, a sequenced, reliable, connection-based connection is used."
      "When TRUE the partial reliability parameters of the channel is ignored.",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

#if defined(SCTP_DEBUG) && !defined(GST_DISABLE_GST_DEBUG)
#define USRSCTP_GST_DEBUG_LEVEL GST_LEVEL_DEBUG
static void
gst_usrsctp_debug (const gchar * format, ...)
{
  va_list varargs;

  va_start (varargs, format);
  gst_debug_log_valist (gst_sctp_debug_category, USRSCTP_GST_DEBUG_LEVEL,
      __FILE__, GST_FUNCTION, __LINE__, NULL, format, varargs);
  va_end (varargs);
}
#endif

static void
gst_sctp_association_init (GstSctpAssociation * self)
{
  /* No need to lock mutex here as long as the function is only called from gst_sctp_association_get */
  if (number_of_associations == 0) {
#if defined(SCTP_DEBUG) && !defined(GST_DISABLE_GST_DEBUG)
    usrsctp_init (0, sctp_packet_out, gst_usrsctp_debug);
#else
    usrsctp_init (0, sctp_packet_out, NULL);
#endif

    /* Explicit Congestion Notification */
    usrsctp_sysctl_set_sctp_ecn_enable (0);

    /* Do not send ABORTs in response to INITs (1).
     * Do not send ABORTs for received Out of the Blue packets (2).
     */
    usrsctp_sysctl_set_sctp_blackhole (2);

    /* Enable interleaving messages for different streams (incoming)
     * See: https://tools.ietf.org/html/rfc6458#section-8.1.20
     */
    usrsctp_sysctl_set_sctp_default_frag_interleave (2);

    usrsctp_sysctl_set_sctp_nr_outgoing_streams_default
        (DEFAULT_NUMBER_OF_SCTP_STREAMS);

#if defined(SCTP_DEBUG) && !defined(GST_DISABLE_GST_DEBUG)
    if (USRSCTP_GST_DEBUG_LEVEL <= GST_LEVEL_MAX
        && USRSCTP_GST_DEBUG_LEVEL <= _gst_debug_min
        && USRSCTP_GST_DEBUG_LEVEL <=
        gst_debug_category_get_threshold (gst_sctp_debug_category)) {
      usrsctp_sysctl_set_sctp_debug_on (SCTP_DEBUG_ALL);
    }
#endif
  }
  number_of_associations++;

  self->local_port = DEFAULT_LOCAL_SCTP_PORT;
  self->remote_port = DEFAULT_REMOTE_SCTP_PORT;
  self->sctp_ass_sock = NULL;

  g_mutex_init (&self->association_mutex);

  self->state = GST_SCTP_ASSOCIATION_STATE_NEW;

  self->use_sock_stream = TRUE;

  usrsctp_register_address ((void *) self);
}

static void
gst_sctp_association_finalize (GObject * object)
{
  GstSctpAssociation *self = GST_SCTP_ASSOCIATION (object);

  G_LOCK (associations_lock);

  g_hash_table_remove (associations, GUINT_TO_POINTER (self->association_id));

  usrsctp_deregister_address ((void *) self);
  number_of_associations--;
  if (number_of_associations == 0) {
    usrsctp_finish ();
  }
  G_UNLOCK (associations_lock);

  G_OBJECT_CLASS (gst_sctp_association_parent_class)->finalize (object);
}

static void
gst_sctp_association_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSctpAssociation *self = GST_SCTP_ASSOCIATION (object);

  g_mutex_lock (&self->association_mutex);
  if (self->state != GST_SCTP_ASSOCIATION_STATE_NEW) {
    switch (prop_id) {
      case PROP_LOCAL_PORT:
      case PROP_REMOTE_PORT:
        GST_ERROR_OBJECT (self, "These properties cannot be set in this state");
        goto error;
    }
  }

  switch (prop_id) {
    case PROP_ASSOCIATION_ID:
      self->association_id = g_value_get_uint (value);
      break;
    case PROP_LOCAL_PORT:
      self->local_port = g_value_get_uint (value);
      break;
    case PROP_REMOTE_PORT:
      self->remote_port = g_value_get_uint (value);
      break;
    case PROP_STATE:
      self->state = g_value_get_enum (value);
      break;
    case PROP_USE_SOCK_STREAM:
      self->use_sock_stream = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&self->association_mutex);
  if (prop_id == PROP_LOCAL_PORT || prop_id == PROP_REMOTE_PORT)
    maybe_set_state_to_ready (self);

  return;

error:
  g_mutex_unlock (&self->association_mutex);
}

static void
maybe_set_state_to_ready (GstSctpAssociation * self)
{
  gboolean signal_ready_state = FALSE;

  g_mutex_lock (&self->association_mutex);
  if ((self->state == GST_SCTP_ASSOCIATION_STATE_NEW) &&
      (self->local_port != 0 && self->remote_port != 0)
      && (self->packet_out_cb != NULL) && (self->packet_received_cb != NULL)) {
    signal_ready_state =
        gst_sctp_association_change_state (self,
        GST_SCTP_ASSOCIATION_STATE_READY, FALSE);
  }
  g_mutex_unlock (&self->association_mutex);

  if (signal_ready_state)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);

}

static void
gst_sctp_association_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSctpAssociation *self = GST_SCTP_ASSOCIATION (object);

  switch (prop_id) {
    case PROP_ASSOCIATION_ID:
      g_value_set_uint (value, self->association_id);
      break;
    case PROP_LOCAL_PORT:
      g_value_set_uint (value, self->local_port);
      break;
    case PROP_REMOTE_PORT:
      g_value_set_uint (value, self->remote_port);
      break;
    case PROP_STATE:
      g_value_set_enum (value, self->state);
      break;
    case PROP_USE_SOCK_STREAM:
      g_value_set_boolean (value, self->use_sock_stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

/* Public functions */

GstSctpAssociation *
gst_sctp_association_get (guint32 association_id)
{
  GstSctpAssociation *association;

  G_LOCK (associations_lock);
  GST_DEBUG_CATEGORY_INIT (gst_sctp_association_debug_category,
      "sctpassociation", 0, "debug category for sctpassociation");
  GST_DEBUG_CATEGORY_INIT (gst_sctp_debug_category,
      "sctplib", 0, "debug category for messages from usrsctp");

  if (!associations) {
    associations =
        g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
  }

  association =
      g_hash_table_lookup (associations, GUINT_TO_POINTER (association_id));
  if (!association) {
    association =
        g_object_new (GST_SCTP_TYPE_ASSOCIATION, "association-id",
        association_id, NULL);
    g_hash_table_insert (associations, GUINT_TO_POINTER (association_id),
        association);
  } else {
    g_object_ref (association);
  }
  G_UNLOCK (associations_lock);
  return association;
}

gboolean
gst_sctp_association_start (GstSctpAssociation * self)
{
  if (self->state != GST_SCTP_ASSOCIATION_STATE_READY) {
    GST_WARNING_OBJECT (self,
        "SCTP association is in wrong state and cannot be started");
    goto configure_required;
  }

  if ((self->sctp_ass_sock = create_sctp_socket (self)) == NULL)
    goto error;

  /* TODO: Support both server and client role */
  if (!client_role_connect (self)) {
    gst_sctp_association_change_state (self, GST_SCTP_ASSOCIATION_STATE_ERROR,
        TRUE);
    goto error;
  }

  gst_sctp_association_change_state (self,
      GST_SCTP_ASSOCIATION_STATE_CONNECTING, TRUE);

  return TRUE;
error:
  gst_sctp_association_change_state (self, GST_SCTP_ASSOCIATION_STATE_ERROR,
      TRUE);
  return FALSE;
configure_required:
  return FALSE;
}

void
gst_sctp_association_set_on_packet_out (GstSctpAssociation * self,
    GstSctpAssociationPacketOutCb packet_out_cb, gpointer user_data,
    GDestroyNotify destroy_notify)
{
  g_return_if_fail (GST_SCTP_IS_ASSOCIATION (self));

  g_mutex_lock (&self->association_mutex);
  if (self->packet_out_destroy_notify)
    self->packet_out_destroy_notify (self->packet_out_user_data);
  self->packet_out_cb = packet_out_cb;
  self->packet_out_user_data = user_data;
  self->packet_out_destroy_notify = destroy_notify;
  g_mutex_unlock (&self->association_mutex);

  maybe_set_state_to_ready (self);
}

void
gst_sctp_association_set_on_packet_received (GstSctpAssociation * self,
    GstSctpAssociationPacketReceivedCb packet_received_cb, gpointer user_data,
    GDestroyNotify destroy_notify)
{
  g_return_if_fail (GST_SCTP_IS_ASSOCIATION (self));

  g_mutex_lock (&self->association_mutex);
  if (self->packet_received_destroy_notify)
    self->packet_received_destroy_notify (self->packet_received_user_data);
  self->packet_received_cb = packet_received_cb;
  self->packet_received_user_data = user_data;
  self->packet_received_destroy_notify = destroy_notify;
  g_mutex_unlock (&self->association_mutex);

  maybe_set_state_to_ready (self);
}

void
gst_sctp_association_incoming_packet (GstSctpAssociation * self,
    const guint8 * buf, guint32 length)
{
  usrsctp_conninput ((void *) self, (const void *) buf, (size_t) length, 0);
}

GstFlowReturn
gst_sctp_association_send_data (GstSctpAssociation * self, const guint8 * buf,
    guint32 length, guint16 stream_id, guint32 ppid, gboolean ordered,
    GstSctpAssociationPartialReliability pr, guint32 reliability_param,
    guint32 * bytes_sent_)
{
  GstFlowReturn flow_ret;
  struct sctp_sendv_spa spa;
  gint32 bytes_sent = 0;
  struct sockaddr_conn remote_addr;

  g_mutex_lock (&self->association_mutex);
  if (self->state != GST_SCTP_ASSOCIATION_STATE_CONNECTED) {
    if (self->state == GST_SCTP_ASSOCIATION_STATE_DISCONNECTED ||
        self->state == GST_SCTP_ASSOCIATION_STATE_DISCONNECTING) {
      GST_ERROR_OBJECT (self, "Disconnected");
      flow_ret = GST_FLOW_EOS;
      g_mutex_unlock (&self->association_mutex);
      goto end;
    } else {
      GST_ERROR_OBJECT (self, "Association not connected yet");
      flow_ret = GST_FLOW_ERROR;
      g_mutex_unlock (&self->association_mutex);
      goto end;
    }
  }
  remote_addr = get_sctp_socket_address (self, self->remote_port);
  g_mutex_unlock (&self->association_mutex);

  /* TODO: We probably want to split too large chunks into multiple packets
   * and only set the SCTP_EOR flag on the last one. Firefox is using 0x4000
   * as the maximum packet size
   */
  memset (&spa, 0, sizeof (spa));

  spa.sendv_sndinfo.snd_ppid = g_htonl (ppid);
  spa.sendv_sndinfo.snd_sid = stream_id;
  spa.sendv_sndinfo.snd_flags = SCTP_EOR | (ordered ? 0 : SCTP_UNORDERED);
  spa.sendv_sndinfo.snd_context = 0;
  spa.sendv_sndinfo.snd_assoc_id = 0;
  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
  if (pr != GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_NONE) {
    spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
    spa.sendv_prinfo.pr_value = g_htonl (reliability_param);
    if (pr == GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_TTL)
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
    else if (pr == GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_RTX)
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
    else if (pr == GST_SCTP_ASSOCIATION_PARTIAL_RELIABILITY_BUF)
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_BUF;
  }

  bytes_sent =
      usrsctp_sendv (self->sctp_ass_sock, buf, length,
      (struct sockaddr *) &remote_addr, 1, (void *) &spa,
      (socklen_t) sizeof (struct sctp_sendv_spa), SCTP_SENDV_SPA, 0);
  if (bytes_sent < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      bytes_sent = 0;
      /* Resending this buffer is taken care of by the gstsctpenc */
      flow_ret = GST_FLOW_OK;
      goto end;
    } else {
      GST_ERROR_OBJECT (self, "Error sending data on stream %u: (%u) %s",
          stream_id, errno, g_strerror (errno));
      flow_ret = GST_FLOW_ERROR;
      goto end;
    }
  }
  flow_ret = GST_FLOW_OK;

end:
  if (bytes_sent_)
    *bytes_sent_ = bytes_sent;

  return flow_ret;
}

void
gst_sctp_association_reset_stream (GstSctpAssociation * self, guint16 stream_id)
{
  struct sctp_reset_streams *srs;
  socklen_t length;

  length = (socklen_t) (sizeof (struct sctp_reset_streams) + sizeof (guint16));
  srs = (struct sctp_reset_streams *) g_malloc0 (length);
  srs->srs_assoc_id = SCTP_ALL_ASSOC;
  srs->srs_flags = SCTP_STREAM_RESET_OUTGOING;
  srs->srs_number_streams = 1;
  srs->srs_stream_list[0] = stream_id;

  usrsctp_setsockopt (self->sctp_ass_sock, IPPROTO_SCTP, SCTP_RESET_STREAMS,
      srs, length);

  g_free (srs);
}

void
gst_sctp_association_force_close (GstSctpAssociation * self)
{
  if (self->sctp_ass_sock) {
    struct socket *s = self->sctp_ass_sock;
    self->sctp_ass_sock = NULL;
    usrsctp_close (s);
  }

  gst_sctp_association_change_state (self,
      GST_SCTP_ASSOCIATION_STATE_DISCONNECTED, TRUE);
}

static struct socket *
create_sctp_socket (GstSctpAssociation * self)
{
  struct socket *sock;
  struct linger l;
  struct sctp_event event;
  struct sctp_assoc_value stream_reset;
  int buf_size = 1024 * 1024;
  int value = 1;
  guint16 event_types[] = {
    SCTP_ASSOC_CHANGE,
    SCTP_PEER_ADDR_CHANGE,
    SCTP_REMOTE_ERROR,
    SCTP_SEND_FAILED,
    SCTP_SEND_FAILED_EVENT,
    SCTP_SHUTDOWN_EVENT,
    SCTP_ADAPTATION_INDICATION,
    SCTP_PARTIAL_DELIVERY_EVENT,
    /*SCTP_AUTHENTICATION_EVENT, */
    SCTP_STREAM_RESET_EVENT,
    /*SCTP_SENDER_DRY_EVENT, */
    /*SCTP_NOTIFICATIONS_STOPPED_EVENT, */
    /*SCTP_ASSOC_RESET_EVENT, */
    SCTP_STREAM_CHANGE_EVENT
  };
  guint32 i;
  guint sock_type = self->use_sock_stream ? SOCK_STREAM : SOCK_SEQPACKET;

  if ((sock =
          usrsctp_socket (AF_CONN, sock_type, IPPROTO_SCTP, receive_cb, NULL, 0,
              (void *) self)) == NULL) {
    GST_ERROR_OBJECT (self, "Could not open SCTP socket: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  if (usrsctp_setsockopt (sock, SOL_SOCKET, SO_RCVBUF,
          (const void *) &buf_size, sizeof (buf_size)) < 0) {
    GST_ERROR_OBJECT (self, "Could not change receive buffer size: (%u) %s",
        errno, g_strerror (errno));
    goto error;
  }
  if (usrsctp_setsockopt (sock, SOL_SOCKET, SO_SNDBUF,
          (const void *) &buf_size, sizeof (buf_size)) < 0) {
    GST_ERROR_OBJECT (self, "Could not change send buffer size: (%u) %s",
        errno, g_strerror (errno));
    goto error;
  }

  /* Properly return errors */
  if (usrsctp_set_non_blocking (sock, 1) < 0) {
    GST_ERROR_OBJECT (self,
        "Could not set non-blocking mode on SCTP socket: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  memset (&l, 0, sizeof (l));
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt (sock, SOL_SOCKET, SO_LINGER, (const void *) &l,
          (socklen_t) sizeof (struct linger)) < 0) {
    GST_ERROR_OBJECT (self, "Could not set SO_LINGER on SCTP socket: (%u) %s",
        errno, g_strerror (errno));
    goto error;
  }

  if (usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_REUSE_PORT, &value,
          sizeof (int))) {
    GST_DEBUG_OBJECT (self, "Could not set SCTP_REUSE_PORT: (%u) %s", errno,
        g_strerror (errno));
  }

  if (usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_NODELAY, &value,
          sizeof (int))) {
    GST_DEBUG_OBJECT (self, "Could not set SCTP_NODELAY: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  if (usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_EXPLICIT_EOR, &value,
          sizeof (int))) {
    GST_ERROR_OBJECT (self, "Could not set SCTP_EXPLICIT_EOR: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  memset (&stream_reset, 0, sizeof (stream_reset));
  stream_reset.assoc_id = SCTP_ALL_ASSOC;
  stream_reset.assoc_value =
      SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
  if (usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET,
          &stream_reset, sizeof (stream_reset))) {
    GST_ERROR_OBJECT (self,
        "Could not set SCTP_ENABLE_STREAM_RESET | SCTP_ENABLE_CHANGE_ASSOC_REQ: (%u) %s",
        errno, g_strerror (errno));
    goto error;
  }

  memset (&event, 0, sizeof (event));
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (i = 0; i < sizeof (event_types) / sizeof (event_types[0]); i++) {
    event.se_type = event_types[i];
    if (usrsctp_setsockopt (sock, IPPROTO_SCTP, SCTP_EVENT,
            &event, sizeof (event)) < 0) {
      GST_ERROR_OBJECT (self, "Failed to register event %u: (%u) %s",
          event_types[i], errno, g_strerror (errno));
    }
  }

  return sock;
error:
  if (sock)
    usrsctp_close (sock);
  return NULL;
}

static struct sockaddr_conn
get_sctp_socket_address (GstSctpAssociation * gst_sctp_association,
    guint16 port)
{
  struct sockaddr_conn addr;

  memset ((void *) &addr, 0, sizeof (struct sockaddr_conn));
#ifdef __APPLE__
  addr.sconn_len = sizeof (struct sockaddr_conn);
#endif
  addr.sconn_family = AF_CONN;
  addr.sconn_port = g_htons (port);
  addr.sconn_addr = (void *) gst_sctp_association;

  return addr;
}

static gboolean
client_role_connect (GstSctpAssociation * self)
{
  struct sockaddr_conn local_addr, remote_addr;
  struct sctp_paddrparams paddrparams;
  socklen_t opt_len;
  gint ret;

  g_mutex_lock (&self->association_mutex);
  local_addr = get_sctp_socket_address (self, self->local_port);
  remote_addr = get_sctp_socket_address (self, self->remote_port);
  g_mutex_unlock (&self->association_mutex);

  ret =
      usrsctp_bind (self->sctp_ass_sock, (struct sockaddr *) &local_addr,
      sizeof (struct sockaddr_conn));
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "usrsctp_bind() error: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  ret =
      usrsctp_connect (self->sctp_ass_sock, (struct sockaddr *) &remote_addr,
      sizeof (struct sockaddr_conn));
  if (ret < 0 && errno != EINPROGRESS) {
    GST_ERROR_OBJECT (self, "usrsctp_connect() error: (%u) %s", errno,
        g_strerror (errno));
    goto error;
  }

  memset (&paddrparams, 0, sizeof (struct sctp_paddrparams));
  memcpy (&paddrparams.spp_address, &remote_addr,
      sizeof (struct sockaddr_conn));
  opt_len = (socklen_t) sizeof (struct sctp_paddrparams);
  ret =
      usrsctp_getsockopt (self->sctp_ass_sock, IPPROTO_SCTP,
      SCTP_PEER_ADDR_PARAMS, &paddrparams, &opt_len);
  if (ret < 0) {
    GST_WARNING_OBJECT (self,
        "usrsctp_getsockopt(SCTP_PEER_ADDR_PARAMS) error: (%u) %s", errno,
        g_strerror (errno));
  } else {
    /* draft-ietf-rtcweb-data-channel-13 section 5: max initial MTU IPV4 1200, IPV6 1280 */
    paddrparams.spp_pathmtu = 1200;
    paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
    paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
    opt_len = (socklen_t) sizeof (struct sctp_paddrparams);
    ret = usrsctp_setsockopt (self->sctp_ass_sock, IPPROTO_SCTP,
        SCTP_PEER_ADDR_PARAMS, &paddrparams, opt_len);
    if (ret < 0) {
      GST_WARNING_OBJECT (self,
          "usrsctp_setsockopt(SCTP_PEER_ADDR_PARAMS) error: (%u) %s", errno,
          g_strerror (errno));
    } else {
      GST_DEBUG_OBJECT (self, "PMTUD disabled, MTU set to %u",
          paddrparams.spp_pathmtu);
    }
  }

  return TRUE;
error:
  return FALSE;
}

static int
sctp_packet_out (void *addr, void *buffer, size_t length, guint8 tos,
    guint8 set_df)
{
  GstSctpAssociation *self = GST_SCTP_ASSOCIATION (addr);

  g_mutex_lock (&self->association_mutex);
  if (self->packet_out_cb) {
    self->packet_out_cb (self, buffer, length, self->packet_out_user_data);
  }
  g_mutex_unlock (&self->association_mutex);

  return 0;
}

static int
receive_cb (struct socket *sock, union sctp_sockstore addr, void *data,
    size_t datalen, struct sctp_rcvinfo rcv_info, gint flags, void *ulp_info)
{
  GstSctpAssociation *self = GST_SCTP_ASSOCIATION (ulp_info);

  if (!data) {
    /* Not sure if this can happend. */
    GST_WARNING_OBJECT (self, "Received empty data buffer");
  } else {
    if (flags & MSG_NOTIFICATION) {
      handle_notification (self, (const union sctp_notification *) data,
          datalen);

      /* We use this instead of a bare `free()` so that we use the `free` from
       * the C runtime that usrsctp was built with. This makes a difference on
       * Windows where libusrstcp and GStreamer can be linked to two different
       * CRTs. */
      usrsctp_freedumpbuffer (data);
    } else {
      handle_message (self, data, datalen, rcv_info.rcv_sid,
          ntohl (rcv_info.rcv_ppid));
    }
  }

  return 1;
}

static void
handle_notification (GstSctpAssociation * self,
    const union sctp_notification *notification, size_t length)
{
  g_assert (notification->sn_header.sn_length == length);

  switch (notification->sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
      GST_DEBUG_OBJECT (self, "Event: SCTP_ASSOC_CHANGE");
      handle_association_changed (self, &notification->sn_assoc_change);
      break;
    case SCTP_PEER_ADDR_CHANGE:
      GST_DEBUG_OBJECT (self, "Event: SCTP_PEER_ADDR_CHANGE");
      break;
    case SCTP_REMOTE_ERROR:
      GST_ERROR_OBJECT (self, "Event: SCTP_REMOTE_ERROR (%u)",
          notification->sn_remote_error.sre_error);
      break;
    case SCTP_SEND_FAILED:
      GST_ERROR_OBJECT (self, "Event: SCTP_SEND_FAILED");
      break;
    case SCTP_SHUTDOWN_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_SHUTDOWN_EVENT");
      gst_sctp_association_change_state (self,
          GST_SCTP_ASSOCIATION_STATE_DISCONNECTING, TRUE);
      break;
    case SCTP_ADAPTATION_INDICATION:
      GST_DEBUG_OBJECT (self, "Event: SCTP_ADAPTATION_INDICATION");
      break;
    case SCTP_PARTIAL_DELIVERY_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_PARTIAL_DELIVERY_EVENT");
      break;
    case SCTP_AUTHENTICATION_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_AUTHENTICATION_EVENT");
      break;
    case SCTP_STREAM_RESET_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_STREAM_RESET_EVENT");
      handle_stream_reset_event (self, &notification->sn_strreset_event);
      break;
    case SCTP_SENDER_DRY_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_SENDER_DRY_EVENT");
      break;
    case SCTP_NOTIFICATIONS_STOPPED_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_NOTIFICATIONS_STOPPED_EVENT");
      break;
    case SCTP_ASSOC_RESET_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_ASSOC_RESET_EVENT");
      break;
    case SCTP_STREAM_CHANGE_EVENT:
      GST_DEBUG_OBJECT (self, "Event: SCTP_STREAM_CHANGE_EVENT");
      break;
    case SCTP_SEND_FAILED_EVENT:
      GST_ERROR_OBJECT (self, "Event: SCTP_SEND_FAILED_EVENT (%u)",
          notification->sn_send_failed_event.ssfe_error);
      break;
    default:
      break;
  }
}

static void
handle_association_changed (GstSctpAssociation * self,
    const struct sctp_assoc_change *sac)
{
  gboolean change_state = FALSE;
  GstSctpAssociationState new_state;

  switch (sac->sac_state) {
    case SCTP_COMM_UP:
      GST_DEBUG_OBJECT (self, "SCTP_COMM_UP");
      g_mutex_lock (&self->association_mutex);
      if (self->state == GST_SCTP_ASSOCIATION_STATE_CONNECTING) {
        change_state = TRUE;
        new_state = GST_SCTP_ASSOCIATION_STATE_CONNECTED;
        GST_DEBUG_OBJECT (self, "SCTP association connected!");
      } else if (self->state == GST_SCTP_ASSOCIATION_STATE_CONNECTED) {
        GST_FIXME_OBJECT (self, "SCTP association already open");
      } else {
        GST_WARNING_OBJECT (self, "SCTP association in unexpected state");
      }
      g_mutex_unlock (&self->association_mutex);
      break;
    case SCTP_COMM_LOST:
      GST_WARNING_OBJECT (self, "SCTP event SCTP_COMM_LOST received");
      change_state = TRUE;
      new_state = GST_SCTP_ASSOCIATION_STATE_ERROR;
      break;
    case SCTP_RESTART:
      GST_DEBUG_OBJECT (self, "SCTP event SCTP_RESTART received");
      break;
    case SCTP_SHUTDOWN_COMP:
      GST_DEBUG_OBJECT (self, "SCTP event SCTP_SHUTDOWN_COMP received");
      change_state = TRUE;
      new_state = GST_SCTP_ASSOCIATION_STATE_DISCONNECTED;
      break;
    case SCTP_CANT_STR_ASSOC:
      GST_WARNING_OBJECT (self, "SCTP event SCTP_CANT_STR_ASSOC received");
      change_state = TRUE;
      new_state = GST_SCTP_ASSOCIATION_STATE_ERROR;
      break;
  }

  if (change_state)
    gst_sctp_association_change_state (self, new_state, TRUE);
}

static void
handle_stream_reset_event (GstSctpAssociation * self,
    const struct sctp_stream_reset_event *sr)
{
  guint32 i, n;
  if (!(sr->strreset_flags & SCTP_STREAM_RESET_DENIED) &&
      !(sr->strreset_flags & SCTP_STREAM_RESET_DENIED)) {
    n = (sr->strreset_length -
        sizeof (struct sctp_stream_reset_event)) / sizeof (uint16_t);
    for (i = 0; i < n; i++) {
      if (sr->strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN) {
        g_signal_emit (self, signals[SIGNAL_STREAM_RESET], 0,
            sr->strreset_stream_list[i]);
      }
    }
  }
}

static void
handle_message (GstSctpAssociation * self, guint8 * data, guint32 datalen,
    guint16 stream_id, guint32 ppid)
{
  g_mutex_lock (&self->association_mutex);
  if (self->packet_received_cb) {
    /* It's the callbacks job to free the data correctly */
    self->packet_received_cb (self, data, datalen, stream_id, ppid,
        self->packet_received_user_data);
  } else {
    /* We use this instead of a bare `free()` so that we use the `free` from
     * the C runtime that usrsctp was built with. This makes a difference on
     * Windows where libusrstcp and GStreamer can be linked to two different
     * CRTs. */
    usrsctp_freedumpbuffer ((gchar *) data);
  }
  g_mutex_unlock (&self->association_mutex);
}

/* Returns TRUE if lock==FALSE and notification is needed later.
 * Takes the mutex shortly if lock==TRUE! */
static gboolean
gst_sctp_association_change_state (GstSctpAssociation * self,
    GstSctpAssociationState new_state, gboolean lock)
{
  if (lock)
    g_mutex_lock (&self->association_mutex);
  if (self->state != new_state
      && self->state != GST_SCTP_ASSOCIATION_STATE_ERROR) {
    self->state = new_state;
    if (lock) {
      g_mutex_unlock (&self->association_mutex);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
      return FALSE;
    } else {
      return TRUE;
    }
  } else {
    if (lock)
      g_mutex_unlock (&self->association_mutex);
    return FALSE;
  }
}
