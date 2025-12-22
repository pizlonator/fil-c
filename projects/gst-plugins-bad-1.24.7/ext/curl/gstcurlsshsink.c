/* GStreamer
 * Copyright (C) 2011 Axis Communications <dev-gstreamer@axis.com>
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

/**
 * SECTION:element-curlsshsink
 * @title: curlsshsink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcurlbasesink.h"
#include "gstcurlsshsink.h"

#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Default values */
#define GST_CAT_DEFAULT    gst_curl_ssh_sink_debug

/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_ssh_sink_debug);

enum
{
  PROP_0,
  PROP_SSH_AUTH_TYPE,
  PROP_SSH_PUB_KEYFILE,
  PROP_SSH_PRIV_KEYFILE,
  PROP_SSH_KEY_PASSPHRASE,
  PROP_SSH_KNOWNHOSTS,
  PROP_SSH_HOST_PUBLIC_KEY_MD5,
  PROP_SSH_HOST_PUBLIC_KEY_SHA256,
  PROP_SSH_ACCEPT_UNKNOWNHOST
};


/* curl SSH-key matching callback */
static gint curl_ssh_sink_sshkey_cb (CURL * easy_handle,
    const struct curl_khkey *knownkey, const struct curl_khkey *foundkey,
    enum curl_khmatch, void *clientp);


/* Object class function declarations */

static void gst_curl_ssh_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_ssh_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_curl_ssh_sink_finalize (GObject * gobject);
static gboolean gst_curl_ssh_sink_set_options_unlocked (GstCurlBaseSink *
    bcsink);


/* private functions */

#define gst_curl_ssh_sink_parent_class      parent_class
G_DEFINE_TYPE (GstCurlSshSink, gst_curl_ssh_sink, GST_TYPE_CURL_BASE_SINK);

/* Register the auth types with the GLib type system */
#define GST_TYPE_CURL_SSH_SINK_AUTH_TYPE (gst_curl_ssh_sink_auth_get_type ())
static GType
gst_curl_ssh_sink_auth_get_type (void)
{
  static GType gtype = 0;

  if (!gtype) {
    static const GEnumValue auth_types[] = {
      {GST_CURLSSH_AUTH_NONE, "Not allowed", "none"},
      {GST_CURLSSH_AUTH_PUBLICKEY, "Public/private key files", "pubkey"},
      {GST_CURLSSH_AUTH_PASSWORD, "Password authentication", "password"},
      {0, NULL, NULL}
    };
    gtype = g_enum_register_static ("GstCurlSshAuthType", auth_types);
  }
  return gtype;
}

static void
gst_curl_ssh_sink_class_init (GstCurlSshSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_ssh_sink_debug, "curlsshsink", 0,
      "curl ssh sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl SSH sink", "Sink/Network",
      "Upload data over SSH/SFTP using libcurl", "Sorin L. <sorin@axis.com>");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_ssh_sink_finalize);

  gobject_class->set_property = gst_curl_ssh_sink_set_property;
  gobject_class->get_property = gst_curl_ssh_sink_get_property;

  klass->set_options_unlocked = gst_curl_ssh_sink_set_options_unlocked;

  g_object_class_install_property (gobject_class, PROP_SSH_AUTH_TYPE,
      g_param_spec_enum ("ssh-auth-type", "SSH authentication type",
          "SSH authentication method to authenticate on the SSH/SFTP server",
          GST_TYPE_CURL_SSH_SINK_AUTH_TYPE, GST_CURLSSH_AUTH_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSH_PUB_KEYFILE,
      g_param_spec_string ("ssh-pub-keyfile",
          "SSH public key file",
          "The complete path & filename of the SSH public key file",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSH_PRIV_KEYFILE,
      g_param_spec_string ("ssh-priv-keyfile",
          "SSH private key file",
          "The complete path & filename of the SSH private key file",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSH_KEY_PASSPHRASE,
      g_param_spec_string ("ssh-key-passphrase", "Passphrase of the priv key",
          "The passphrase used to protect the SSH private key file",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSH_KNOWNHOSTS,
      g_param_spec_string ("ssh-knownhosts",
          "SSH known hosts",
          "The complete path & filename of the SSH 'known_hosts' file",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSH_HOST_PUBLIC_KEY_MD5,
      g_param_spec_string ("ssh-host-pubkey-md5",
          "MD5 checksum of the remote host's public key",
          "MD5 checksum (32 hexadecimal digits, case-insensitive) of the "
          "remote host's public key",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#if CURL_AT_LEAST_VERSION(7, 80, 0)
  g_object_class_install_property (gobject_class,
      PROP_SSH_HOST_PUBLIC_KEY_SHA256,
      g_param_spec_string ("ssh-host-pubkey-sha256",
          "SHA256 checksum of the remote host's public key",
          "SHA256 checksum (Base64 encoded) of the remote host's public key",
          NULL, G_PARAM_READWRITE | GST_PARAM_CONDITIONALLY_AVAILABLE |
          G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_SSH_ACCEPT_UNKNOWNHOST,
      g_param_spec_boolean ("ssh-accept-unknownhost",
          "SSH accept unknown host",
          "Accept an unknown remote public host key",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_CURL_SSH_SINK_AUTH_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_CURL_SSH_SINK, 0);
}

static void
gst_curl_ssh_sink_init (GstCurlSshSink * sink)
{
  sink->ssh_auth_type = GST_CURLSSH_AUTH_NONE;
  sink->ssh_pub_keyfile = NULL;
  sink->ssh_priv_keyfile = NULL;
  sink->ssh_key_passphrase = NULL;
  sink->ssh_knownhosts = NULL;
  sink->ssh_host_public_key_md5 = NULL;
  sink->ssh_host_public_key_sha256 = NULL;
  sink->ssh_accept_unknownhost = FALSE;
}

static void
gst_curl_ssh_sink_finalize (GObject * gobject)
{
  GstCurlSshSink *this = GST_CURL_SSH_SINK (gobject);

  GST_DEBUG ("finalizing curlsshsink");

  g_free (this->ssh_pub_keyfile);
  g_free (this->ssh_priv_keyfile);
  g_free (this->ssh_key_passphrase);
  g_free (this->ssh_knownhosts);
  g_free (this->ssh_host_public_key_md5);
  g_free (this->ssh_host_public_key_sha256);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_curl_ssh_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlSshSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_SSH_SINK (object));
  sink = GST_CURL_SSH_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);

  if (cur_state == GST_STATE_PLAYING || cur_state == GST_STATE_PAUSED) {
    return;
  }

  GST_OBJECT_LOCK (sink);
  switch (prop_id) {
    case PROP_SSH_AUTH_TYPE:
      sink->ssh_auth_type = g_value_get_enum (value);
      GST_DEBUG_OBJECT (sink, "ssh_auth_type set to %d", sink->ssh_auth_type);
      break;

    case PROP_SSH_PUB_KEYFILE:
      g_free (sink->ssh_pub_keyfile);
      sink->ssh_pub_keyfile = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_pub_keyfile set to %s",
          sink->ssh_pub_keyfile);
      break;

    case PROP_SSH_PRIV_KEYFILE:
      g_free (sink->ssh_priv_keyfile);
      sink->ssh_priv_keyfile = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_priv_keyfile set to %s",
          sink->ssh_priv_keyfile);
      break;

    case PROP_SSH_KEY_PASSPHRASE:
      g_free (sink->ssh_key_passphrase);
      sink->ssh_key_passphrase = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_key_passphrase set to %s",
          sink->ssh_key_passphrase);
      break;

    case PROP_SSH_KNOWNHOSTS:
      g_free (sink->ssh_knownhosts);
      sink->ssh_knownhosts = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_knownhosts set to %s", sink->ssh_knownhosts);
      break;

    case PROP_SSH_HOST_PUBLIC_KEY_MD5:
      g_free (sink->ssh_host_public_key_md5);
      sink->ssh_host_public_key_md5 = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_host_public_key_md5 set to %s",
          sink->ssh_host_public_key_md5);
      break;

    case PROP_SSH_HOST_PUBLIC_KEY_SHA256:
      g_free (sink->ssh_host_public_key_sha256);
      sink->ssh_host_public_key_sha256 = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "ssh_host_public_key_sha256 set to %s",
          sink->ssh_host_public_key_sha256);
      break;

    case PROP_SSH_ACCEPT_UNKNOWNHOST:
      sink->ssh_accept_unknownhost = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (sink, "ssh_accept_unknownhost set to %d",
          sink->ssh_accept_unknownhost);
      break;

    default:
      GST_DEBUG_OBJECT (sink, "invalid property id %d", prop_id);
      break;
  }
  GST_OBJECT_UNLOCK (sink);
}

static void
gst_curl_ssh_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlSshSink *sink;

  g_return_if_fail (GST_IS_CURL_SSH_SINK (object));
  sink = GST_CURL_SSH_SINK (object);

  switch (prop_id) {
    case PROP_SSH_AUTH_TYPE:
      g_value_set_enum (value, sink->ssh_auth_type);
      break;

    case PROP_SSH_PUB_KEYFILE:
      g_value_set_string (value, sink->ssh_pub_keyfile);
      break;

    case PROP_SSH_PRIV_KEYFILE:
      g_value_set_string (value, sink->ssh_priv_keyfile);
      break;

    case PROP_SSH_KEY_PASSPHRASE:
      g_value_set_string (value, sink->ssh_key_passphrase);
      break;

    case PROP_SSH_KNOWNHOSTS:
      g_value_set_string (value, sink->ssh_knownhosts);
      break;

    case PROP_SSH_HOST_PUBLIC_KEY_MD5:
      g_value_set_string (value, sink->ssh_host_public_key_md5);
      break;

    case PROP_SSH_HOST_PUBLIC_KEY_SHA256:
      g_value_set_string (value, sink->ssh_host_public_key_sha256);
      break;

    case PROP_SSH_ACCEPT_UNKNOWNHOST:
      g_value_set_boolean (value, sink->ssh_accept_unknownhost);
      break;

    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}

static gboolean
gst_curl_ssh_sink_set_options_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlSshSink *sink = GST_CURL_SSH_SINK (bcsink);
  CURLcode curl_err = CURLE_OK;

  /* set SSH specific options here */
  if (sink->ssh_pub_keyfile) {
    if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_SSH_PUBLIC_KEYFILE,
                sink->ssh_pub_keyfile)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set public key file: %s",
          curl_easy_strerror (curl_err));
      return FALSE;
    }
  }

  if (sink->ssh_priv_keyfile) {
    if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_SSH_PRIVATE_KEYFILE,
                sink->ssh_priv_keyfile)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set private key file: %s",
          curl_easy_strerror (curl_err));
      return FALSE;
    }
  }

  if (sink->ssh_knownhosts) {
    if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_SSH_KNOWNHOSTS,
                sink->ssh_knownhosts)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set known_hosts file: %s",
          curl_easy_strerror (curl_err));
      return FALSE;
    }
  }

  if (sink->ssh_host_public_key_md5) {
    /* libcurl is freaking tricky. If the input string is not exactly 32
     * hexdigits long it silently ignores CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 and
     * performs the transfer without authenticating the server! */
    if (strlen (sink->ssh_host_public_key_md5) != 32) {
      bcsink->error = g_strdup ("MD5-hash string has invalid length, "
          "must be exactly 32 hexdigits!");
      return FALSE;
    }

    if ((curl_err =
            curl_easy_setopt (bcsink->curl, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5,
                sink->ssh_host_public_key_md5)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set remote host's public "
          "key MD5: %s", curl_easy_strerror (curl_err));
      return FALSE;
    }
  }
#if CURL_AT_LEAST_VERSION(7, 80, 0)
  if (sink->ssh_host_public_key_sha256) {
    if ((curl_err =
            curl_easy_setopt (bcsink->curl, CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256,
                sink->ssh_host_public_key_sha256)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set remote host's public "
          "key SHA256: %s", curl_easy_strerror (curl_err));
      return FALSE;
    }
  }
#endif

  /* make sure we only accept PASSWORD or PUBLICKEY auth methods
   * (can be extended later) */
  if (sink->ssh_auth_type == GST_CURLSSH_AUTH_PASSWORD ||
      sink->ssh_auth_type == GST_CURLSSH_AUTH_PUBLICKEY) {

    /* set the SSH_AUTH_TYPE */
    if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_SSH_AUTH_TYPES,
                sink->ssh_auth_type)) != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set authentication type: %s",
          curl_easy_strerror (curl_err));
      return FALSE;
    }

    /* if key authentication -> provide the private key passphrase as well */
    if (sink->ssh_auth_type == GST_CURLSSH_AUTH_PUBLICKEY) {
      if (sink->ssh_key_passphrase) {
        if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_KEYPASSWD,
                    sink->ssh_key_passphrase)) != CURLE_OK) {
          bcsink->error = g_strdup_printf ("failed to set private key "
              "passphrase: %s", curl_easy_strerror (curl_err));
          return FALSE;
        }
      } else {
        /* The user did not provide the passphrase for the private key.
         * This can still be a valid situation, if (s)he chose not to
         * protect the private key with a passphrase - but not recommended! */
        GST_WARNING_OBJECT (sink, "Warning: key authentication chosen but "
            "'ssh-key-passphrase' not provided!");
      }
    }

  } else {
    bcsink->error = g_strdup_printf ("Error: unsupported authentication type: "
        "%d.", sink->ssh_auth_type);
    return FALSE;
  }

  /* Install the SSH_KEYFUNCTION callback...
   * IMPORTANT: this callback gets called only if CURLOPT_SSH_KNOWNHOSTS
   * is also set! */
  if ((curl_err = curl_easy_setopt (bcsink->curl, CURLOPT_SSH_KEYFUNCTION,
              curl_ssh_sink_sshkey_cb)) != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set SSH_KEYFUNCTION callback: "
        "%s", curl_easy_strerror (curl_err));
    return FALSE;
  }
  /* SSH_KEYFUNCTION callback successfully installed so go on and
   * set the '*clientp' parameter as well */
  if ((curl_err =
          curl_easy_setopt (bcsink->curl, CURLOPT_SSH_KEYDATA,
              sink)) != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set CURLOPT_SSH_KEYDATA: %s",
        curl_easy_strerror (curl_err));
    return FALSE;
  }

  return TRUE;
}


/* A 'curl_sshkey_cb' callback function. It gets called when the known_host
 * matching has been done, to allow the application to act and decide for
 * libcurl how to proceed.
 * The callback will only be called if CURLOPT_SSH_KNOWNHOSTS is also set!
 * NOTE:
 *  * use CURLOPT_SSH_KEYFUNCTION to install the callback func
 *  * use CURLOPT_SSH_KEYDATA to pass in the actual "*clientp"
 */
static gint
curl_ssh_sink_sshkey_cb (CURL * easy_handle,    /* easy handle */
    const struct curl_khkey *knownkey,  /* known - key from known_hosts */
    const struct curl_khkey *foundkey,  /* found - key from remote end */
    enum curl_khmatch match,    /* libcurl's view on the keys */
    void *clientp)
{
  GstCurlSshSink *sink = (GstCurlSshSink *) clientp;

  /* the default action to be taken upon pub key matching */
  guint res_action = CURLKHSTAT_REJECT;

  switch (match) {
    case CURLKHMATCH_OK:
      res_action = CURLKHSTAT_FINE;
      GST_INFO_OBJECT (sink,
          "Remote public host key is matching known_hosts, OK to proceed.");
      break;

    case CURLKHMATCH_MISMATCH:
      GST_WARNING_OBJECT (sink,
          "Remote public host key mismatch in known_hosts, aborting "
          "connection.");
      /* Reject the connection. The old mismatching key has to be manually
       * removed from 'known_hosts' before being able to connect again to
       * the respective host. */
      break;

    case CURLKHMATCH_MISSING:
      GST_OBJECT_LOCK (sink);
      if (sink->ssh_accept_unknownhost == TRUE) {
        /* the key was not found in known_hosts but the user chose to
         * accept it */
        res_action = CURLKHSTAT_FINE_ADD_TO_FILE;
        GST_INFO_OBJECT (sink, "Accepting and adding new public host key to "
            "known_hosts.");
      } else {
        /* the key was not found in known_hosts and the user chose not
         * to accept connections to unknown hosts */
        GST_WARNING_OBJECT (sink,
            "Remote public host key is unknown, rejecting connection.");
      }
      GST_OBJECT_UNLOCK (sink);
      break;

    default:
      /* something went wrong, we got some bogus key match result */
      GST_CURL_BASE_SINK (sink)->error =
          g_strdup ("libcurl internal error during known_host matching");
      break;
  }

  return res_action;
}
