/* GStreamer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <microdns/microdns.h>

#include "gstmicrodnsdevice.h"

typedef struct _ListenerContext ListenerContext;

struct _GstMDNSDeviceProvider
{
  GstDeviceProvider parent;
  ListenerContext *current_ctx;
};

#define LISTEN_INTERVAL_SECONDS 2

/* #GstDeviceProvider.stop() is synchronous, but libmicrodns' stop mechanism
 * isn't, as it polls and queries the application's provided stop function
 * before each new loop crank. This means there can potentially exist N
 * contexts at any given time, if the provider is started and stopped in
 * rapid succession. At most one of them can be active however (stop == false),
 * with the other N - 1 in the process of stopping (stop == true).
 *
 * Additionally, mdns_listen() is a blocking call, thus the need to run it in
 * its own thread.
 */
struct _ListenerContext
{
  GMutex lock;
  GCond stop_cond;
  GstDeviceProvider *provider;

  /* The following fields are protected by @lock */
  bool stop;
  GHashTable *devices;
  GSequence *last_seen_devices;
};

G_DEFINE_TYPE (GstMDNSDeviceProvider, gst_mdns_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

struct _GstMDNSDevice
{
  GstDevice parent;

  GstURIType uri_type;
  gchar *uri;
  GSequenceIter *iter;
  gint64 last_seen;
};

G_DEFINE_TYPE (GstMDNSDevice, gst_mdns_device, GST_TYPE_DEVICE);

static gint
cmp_last_seen (GstMDNSDevice * a, GstMDNSDevice * b,
    gpointer G_GNUC_UNUSED user_data)
{
  if (a->last_seen < b->last_seen)
    return -1;
  if (a->last_seen == b->last_seen)
    return 0;
  return 1;
}

static gint
cmp_last_seen_iter (GSequenceIter * ia, GSequenceIter * ib, gpointer user_data)
{
  return cmp_last_seen (GST_MDNS_DEVICE (g_sequence_get (ia)),
      GST_MDNS_DEVICE (g_sequence_get (ib)), user_data);
}

static void
gst_mdns_device_finalize (GObject * object)
{
  GstMDNSDevice *self = GST_MDNS_DEVICE (object);

  g_free (self->uri);

  G_OBJECT_CLASS (gst_mdns_device_parent_class)->finalize (object);
}

static GstElement *
gst_mdns_device_create_element (GstDevice * device, const gchar * name)
{
  GstMDNSDevice *self = GST_MDNS_DEVICE (device);
  GstElement *ret;
  GError *err = NULL;

  ret = gst_element_make_from_uri (self->uri_type, self->uri, name, &err);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to create element for URI %s: %s",
        self->uri, err->message);
    g_clear_error (&err);
  }

  return ret;
}

static void
gst_mdns_device_init (GstMDNSDevice * self)
{
}

static void
gst_mdns_device_class_init (GstMDNSDeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceClass *gst_device_class = GST_DEVICE_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_mdns_device_finalize);

  gst_device_class->create_element =
      GST_DEBUG_FUNCPTR (gst_mdns_device_create_element);
}

/* Slightly unoptimized, ideally add gst_element_factory_from_uri */
static GstElementFactory *
get_factory_for_uri (GstURIType type, const gchar * uri)
{
  GError *err = NULL;
  GstElementFactory *ret = NULL;
  GstElement *elem = gst_element_make_from_uri (type, uri, NULL, &err);

  if (!elem) {
    GST_LOG ("Failed to make element from uri: %s", err->message);
    g_clear_error (&err);
    goto done;
  }

  ret = gst_element_get_factory (elem);

  gst_object_unref (elem);

done:
  return ret;
}

static GstDevice *
gst_mdns_device_new (GstElementFactory * factory, const gchar * name,
    const gchar * uri)
{
  GstDevice *ret = NULL;
  const GList *templates;
  GstCaps *caps;

  templates = gst_element_factory_get_static_pad_templates (factory);
  caps = gst_static_pad_template_get_caps ((GstStaticPadTemplate *)
      templates->data);

  ret = GST_DEVICE (g_object_new (GST_TYPE_MDNS_DEVICE,
          "display-name", name,
          "device-class", gst_element_factory_get_metadata (factory, "klass"),
          "caps", caps, NULL));

  GST_MDNS_DEVICE (ret)->uri = g_strdup (uri);
  GST_MDNS_DEVICE (ret)->uri_type = gst_element_factory_get_uri_type (factory);

  gst_caps_unref (caps);

  return ret;
}

static void
remove_old_devices (ListenerContext * ctx)
{
  GstMDNSDeviceProvider *self = GST_MDNS_DEVICE_PROVIDER (ctx->provider);
  GstClockTime now = g_get_monotonic_time ();
  GSequenceIter *iter = g_sequence_get_begin_iter (ctx->last_seen_devices);

  while (!g_sequence_iter_is_end (iter)) {
    GstMDNSDevice *dev = GST_MDNS_DEVICE (g_sequence_get (iter));
    GstClockTime age = now - dev->last_seen;

    GST_LOG_OBJECT (self,
        "Device %" GST_PTR_FORMAT " last seen %" GST_TIME_FORMAT " ago", dev,
        GST_TIME_ARGS (age));

    if (age > 4 * LISTEN_INTERVAL_SECONDS * G_USEC_PER_SEC) {
      GSequenceIter *next = g_sequence_iter_next (iter);

      GST_INFO_OBJECT (self, "Removing device %" GST_PTR_FORMAT, dev);

      gst_device_provider_device_remove (ctx->provider, GST_DEVICE (dev));
      g_hash_table_remove (ctx->devices, dev->uri);
      g_sequence_remove (iter);
      iter = next;
    } else {
      GST_LOG_OBJECT (self, "Keeping device %" GST_PTR_FORMAT, dev);
      iter = g_sequence_get_end_iter (ctx->last_seen_devices);
    }
  }
}

static bool
stop (void *p_cookie)
{
  bool ret;
  ListenerContext *ctx = (ListenerContext *) p_cookie;

  g_mutex_lock (&ctx->lock);
  ret = ctx->stop;

  if (!ctx->stop) {
    remove_old_devices (ctx);
  }

  g_mutex_unlock (&ctx->lock);

  return ret;
}

static void
callback (void *p_cookie, gint status, const struct rr_entry *entry)
{
  ListenerContext *ctx = (ListenerContext *) p_cookie;
  gchar err[128];
  const struct rr_entry *tmp;
  GHashTable *srvs = g_hash_table_new (g_str_hash, g_str_equal);
  GstMDNSDeviceProvider *self = GST_MDNS_DEVICE_PROVIDER (ctx->provider);

  g_mutex_lock (&ctx->lock);

  if (ctx->stop)
    goto done;

  GST_DEBUG_OBJECT (self, "received new entries");

  if (status < 0) {
    mdns_strerror (status, err, sizeof (err));
    GST_ERROR ("MDNS error: %s", err);
    goto done;
  }

  for (tmp = entry; tmp; tmp = tmp->next) {
    if (tmp->type == RR_SRV) {
      g_hash_table_insert (srvs, (gpointer) tmp->name, (gpointer) tmp);
    }
  }

  for (tmp = entry; tmp; tmp = tmp->next) {
    if (tmp->type == RR_TXT) {
      const struct rr_entry *srv;

      srv = (const struct rr_entry *) g_hash_table_lookup (srvs, tmp->name);

      if (!srv) {
        GST_LOG_OBJECT (self, "No SRV associated with TXT entry for %s",
            tmp->name);
        continue;
      }

      if (g_str_has_suffix (tmp->name, "._rtsp._tcp.local")) {
        gchar *path = NULL;
        gchar *uri;
        struct rr_data_txt *tmp_txt;
        GstMDNSDevice *dev;

        for (tmp_txt = tmp->data.TXT; tmp_txt; tmp_txt = tmp_txt->next) {
          if (g_str_has_prefix (tmp_txt->txt, "path=")) {
            path = tmp_txt->txt + 5;
          }
        }

        if (path) {
          uri =
              g_strdup_printf ("rtsp://%s:%d/%s", srv->data.SRV.target,
              srv->data.SRV.port, path);
        } else {
          uri =
              g_strdup_printf ("rtsp://%s:%d", srv->data.SRV.target,
              srv->data.SRV.port);
        }

        dev = GST_MDNS_DEVICE (g_hash_table_lookup (ctx->devices, uri));

        GST_LOG_OBJECT (self, "Saw device at uri %s", uri);

        if (dev) {
          dev->last_seen = g_get_monotonic_time ();
          GST_LOG_OBJECT (self,
              "updating last_seen for device %" GST_PTR_FORMAT ": %"
              G_GINT64_FORMAT, dev, dev->last_seen);
          g_sequence_sort_changed_iter (dev->iter, cmp_last_seen_iter, NULL);
        } else {
          GstElementFactory *factory;
          gchar *display_name;

          if (!(factory = get_factory_for_uri (GST_URI_SRC, uri))) {
            GST_LOG_OBJECT (self,
                "Not registering device %s as no compatible factory was found",
                tmp->name);
            goto done;
          }

          display_name = g_strndup (tmp->name, strlen (tmp->name) - 17);
          dev =
              GST_MDNS_DEVICE (gst_mdns_device_new (factory, display_name,
                  uri));
          g_free (display_name);
          dev->last_seen = g_get_monotonic_time ();
          GST_INFO_OBJECT (self,
              "Saw new device %" GST_PTR_FORMAT " at %" G_GINT64_FORMAT
              " with factory %" GST_PTR_FORMAT, dev, dev->last_seen, factory);
          dev->iter =
              g_sequence_insert_sorted (ctx->last_seen_devices, (gpointer) dev,
              (GCompareDataFunc) cmp_last_seen, NULL);
          g_hash_table_insert (ctx->devices, g_strdup (uri),
              gst_object_ref (dev));
          gst_device_provider_device_add (ctx->provider, GST_DEVICE (dev));
        }

        g_free (uri);
      } else {
        GST_LOG_OBJECT (self, "unknown protocol for %s", tmp->name);
        continue;
      }
    }
  }

done:
  g_hash_table_unref (srvs);
  g_mutex_unlock (&ctx->lock);
}

static gpointer
_listen (ListenerContext * ctx)
{
  gint r = 0;
  gchar err[128];
  struct mdns_ctx *mctx;
  const gchar *ppsz_names[] = { "_rtsp._tcp.local" };
  gint i_nb_names = 1;

  if ((r = mdns_init (&mctx, MDNS_ADDR_IPV4, MDNS_PORT)) < 0)
    goto err;

  GST_INFO_OBJECT (ctx->provider, "Start listening");

  if ((r = mdns_listen (mctx, ppsz_names, i_nb_names, RR_PTR,
              LISTEN_INTERVAL_SECONDS, stop, callback, ctx)) < 0) {
    mdns_destroy (mctx);
    goto err;
  }

done:
  GST_INFO_OBJECT (ctx->provider, "Done listening");

  /* Wait until we're told to stop, or gst_mdns_device_provider_stop()
     can access a freed context */
  g_mutex_lock (&ctx->lock);
  while (!ctx->stop) {
    g_cond_wait (&ctx->stop_cond, &ctx->lock);
  }
  g_mutex_unlock (&ctx->lock);

  g_sequence_free (ctx->last_seen_devices);
  g_hash_table_unref (ctx->devices);
  g_mutex_clear (&ctx->lock);
  g_cond_clear (&ctx->stop_cond);
  g_free (ctx);

  return NULL;

err:
  if (r < 0) {
    mdns_strerror (r, err, sizeof (err));
    GST_ERROR ("MDNS error: %s", err);
  }

  goto done;
}

static gboolean
gst_mdns_device_provider_start (GstDeviceProvider * provider)
{
  GstMDNSDeviceProvider *self = GST_MDNS_DEVICE_PROVIDER (provider);
  ListenerContext *ctx = g_new0 (ListenerContext, 1);

  g_mutex_init (&ctx->lock);
  g_cond_init (&ctx->stop_cond);
  ctx->provider = provider;
  ctx->devices =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  ctx->last_seen_devices = g_sequence_new (NULL);
  self->current_ctx = ctx;

  g_thread_new (NULL, (GThreadFunc) _listen, ctx);

  return TRUE;
}

static void
gst_mdns_device_provider_stop (GstDeviceProvider * provider)
{
  GstMDNSDeviceProvider *self = GST_MDNS_DEVICE_PROVIDER (provider);

  g_assert (self->current_ctx);

  g_mutex_lock (&self->current_ctx->lock);
  self->current_ctx->stop = true;
  g_cond_broadcast (&self->current_ctx->stop_cond);
  g_mutex_unlock (&self->current_ctx->lock);

  self->current_ctx = NULL;
}

static void
gst_mdns_device_provider_init (GstMDNSDeviceProvider * self)
{
}

static void
gst_mdns_device_provider_class_init (GstMDNSDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  dm_class->start = GST_DEBUG_FUNCPTR (gst_mdns_device_provider_start);
  dm_class->stop = GST_DEBUG_FUNCPTR (gst_mdns_device_provider_stop);

  gst_device_provider_class_set_static_metadata (dm_class,
      "MDNS Device Provider", "Source/Network",
      "List and provides MDNS-advertised source devices",
      "Mathieu Duponchelle <mathieu@centricular.com>");
}
