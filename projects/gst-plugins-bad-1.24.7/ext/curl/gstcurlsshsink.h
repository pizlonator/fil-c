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

#ifndef __GST_CURL_SSH_SINK__
#define __GST_CURL_SSH_SINK__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <curl/curl.h>
#include "gstcurlbasesink.h"

G_BEGIN_DECLS
#define GST_TYPE_CURL_SSH_SINK \
  (gst_curl_ssh_sink_get_type())
#define GST_CURL_SSH_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CURL_SSH_SINK,GstCurlSshSink))
#define GST_CURL_SSH_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CURL_SSH_SINK,GstCurlSshSinkClass))
#define GST_CURL_SSH_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_CURL_SSH_SINK,GstCurlSshSinkClass))
#define GST_IS_CURL_SSH_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CURL_SSH_SINK))
#define GST_IS_CURL_SSH_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CURL_SSH_SINK))

typedef enum
{
  /* Keep these in sync with the libcurl definitions. See <curl/curl.h> */
  GST_CURLSSH_AUTH_NONE = CURLSSH_AUTH_NONE,
  GST_CURLSSH_AUTH_PUBLICKEY = CURLSSH_AUTH_PUBLICKEY,
  GST_CURLSSH_AUTH_PASSWORD = CURLSSH_AUTH_PASSWORD
} GstCurlSshAuthType;

typedef struct _GstCurlSshSink GstCurlSshSink;
typedef struct _GstCurlSshSinkClass GstCurlSshSinkClass;


struct _GstCurlSshSink
{
  GstCurlBaseSink parent;

  /*< private > */
  /* for now, supporting only:
   * GST_CURLSSH_AUTH_PASSWORD (password authentication) and
   * GST_CURLSSH_AUTH_PUBLICKEY (public key authentication) */
  GstCurlSshAuthType ssh_auth_type;

  gchar *ssh_pub_keyfile;       /* filename for the public key:
                                   CURLOPT_SSH_PUBLIC_KEYFILE */
  gchar *ssh_priv_keyfile;      /* filename for the private key:
                                   CURLOPT_SSH_PRIVATE_KEYFILE */
  gchar *ssh_key_passphrase;    /* passphrase for the pvt key:
                                   CURLOPT_KEYPASSWD */

  gchar *ssh_knownhosts;        /* filename of the 'known_hosts' file:
                                   CURLOPT_SSH_KNOWN_HOSTS */
  gboolean ssh_accept_unknownhost;      /* accept or reject unknown public key
                                           from remote host */
  gchar *ssh_host_public_key_md5;   /* MD5-hash of the remote host's public key:
                                       CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 */
  gchar *ssh_host_public_key_sha256; /* SHA256-hash of the remote host's public
                                        key: CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256
                                      */
};

struct _GstCurlSshSinkClass
{
  GstCurlBaseSinkClass parent_class;

  /* vmethods */
    gboolean (*set_options_unlocked) (GstCurlBaseSink * sink);
};

GType gst_curl_ssh_sink_get_type (void);

G_END_DECLS
#endif
