/*
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Simple device provider example.
 *
 * Usage:
 *
 * GST_PLUGIN_PATH=$GST_PLUGIN_PATH:/path/to/libexample_device_provider.so/folder gst-device-monitor-1.0 -f
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#define NEW_DEVICE_INTERVAL 1   /* seconds */

#define EXAMPLE_TYPE_DEVICE_PROVIDER example_device_provider_get_type()
#define EXAMPLE_DEVICE_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),EXAMPLE_TYPE_DEVICE_PROVIDER,ExampleDeviceProvider))

typedef struct _ExampleDeviceProvider ExampleDeviceProvider;
typedef struct _ExampleDeviceProviderClass ExampleDeviceProviderClass;

struct _ExampleDeviceProviderClass
{
  GstDeviceProviderClass parent_class;
};

/**
 * Our device provider instance.
 *
 * @factory: the videotestsrc factory
 * @patterns: When started, the list of videotestsrc pattern
 *            (as strings) to iterate through when adding new devices,
 *            eg "smpte", "snow", ...
 * @timeout_id: When started, we will add a new device every
 *            %NEW_DEVICE_INTERVAL seconds
 */
struct _ExampleDeviceProvider
{
  GstDeviceProvider parent;
  GstElementFactory *factory;
  GList *patterns;
  guint timeout_id;
};

static GType example_device_provider_get_type (void);

G_DEFINE_TYPE (ExampleDeviceProvider, example_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

#define EXAMPLE_TYPE_DEVICE example_device_get_type()
#define EXAMPLE_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),EXAMPLE_TYPE_DEVICE,ExampleDevice))

typedef struct _ExampleDevice ExampleDevice;
typedef struct _ExampleDeviceClass ExampleDeviceClass;

struct _ExampleDeviceClass
{
  GstDeviceClass parent_class;
};

/* Our example device, it simply exposes a videotestsrc with a specific
 * pattern.
 */
struct _ExampleDevice
{
  GstDevice parent;

  gchar *pattern;
  GstElementFactory *factory;
};

static GType example_device_get_type (void);

G_DEFINE_TYPE (ExampleDevice, example_device, GST_TYPE_DEVICE);

static void
example_device_init (ExampleDevice * self)
{
}

static void
example_device_finalize (GObject * object)
{
  ExampleDevice *self = EXAMPLE_DEVICE (object);

  g_free (self->pattern);

  G_OBJECT_CLASS (example_device_parent_class)->finalize (object);
}

static void
example_device_dispose (GObject * object)
{
  ExampleDevice *self = EXAMPLE_DEVICE (object);

  gst_object_replace ((GstObject **) & self->factory, NULL);

  G_OBJECT_CLASS (example_device_parent_class)->dispose (object);
}

static GstElement *
example_device_create_element (GstDevice * device, const gchar * name)
{
  ExampleDevice *self = EXAMPLE_DEVICE (device);
  GstElement *ret;

  ret = gst_element_factory_create (self->factory, name);

  gst_util_set_object_arg (G_OBJECT (ret), "pattern", self->pattern);

  return ret;
}

static void
example_device_class_init (ExampleDeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceClass *gst_device_class = GST_DEVICE_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (example_device_finalize);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (example_device_dispose);

  gst_device_class->create_element =
      GST_DEBUG_FUNCPTR (example_device_create_element);
}

static GstDevice *
example_device_new (GstElementFactory * factory, const gchar * pattern)
{
  GstDevice *ret;
  gchar *display_name;
  GstCaps *caps;
  const GList *templates;

  templates = gst_element_factory_get_static_pad_templates (factory);
  caps = gst_static_pad_template_get_caps ((GstStaticPadTemplate *)
      templates->data);

  display_name = g_strdup_printf ("example-device-%s", pattern);

  ret = GST_DEVICE (g_object_new (EXAMPLE_TYPE_DEVICE,
          "display-name", display_name,
          "device-class", "Video/Source", "caps", caps, NULL));

  g_free (display_name);
  gst_caps_unref (caps);

  EXAMPLE_DEVICE (ret)->pattern = g_strdup (pattern);
  EXAMPLE_DEVICE (ret)->factory =
      GST_ELEMENT_FACTORY (gst_object_ref (factory));

  return ret;
}

static void
example_device_provider_init (ExampleDeviceProvider * self)
{
  self->factory = gst_element_factory_find ("videotestsrc");

  /* Ensure we can introspect the factory */
  gst_object_unref (gst_plugin_feature_load (GST_PLUGIN_FEATURE
          (self->factory)));

}

/* Called when gst_device_provider_get_devices() is called on a provider that
 * hasn't been started, or doesn't implement #GstDeviceProvider.start().
 *
 * In that case, let's return a single example device, with a snow pattern.
 */
static GList *
example_device_provider_probe (GstDeviceProvider * provider)
{
  ExampleDeviceProvider *self = EXAMPLE_DEVICE_PROVIDER (provider);
  GList *ret = NULL;

  ret = g_list_prepend (ret, example_device_new (self->factory, "snow"));

  return ret;
}

static gboolean
example_device_provider_next_device (ExampleDeviceProvider * self)
{
  GstDevice *device;
  gboolean ret = G_SOURCE_CONTINUE;

  if (!self->patterns)
    goto no_more_patterns;

  device = example_device_new (self->factory, (gchar *) self->patterns->data);
  gst_device_provider_device_add (GST_DEVICE_PROVIDER (self), device);
  g_free (self->patterns->data);
  self->patterns = g_list_delete_link (self->patterns, self->patterns);

done:
  return ret;

no_more_patterns:
  GST_DEBUG_OBJECT (self, "Went through all videotestsrc patterns!");
  ret = G_SOURCE_REMOVE;
  goto done;
}

/* Start adding devices every %NEW_DEVICE_INTERVAL seconds.
 * We will stop once we have consumed all the available videotestsrc
 * patterns, or when our #GstDeviceProvider.stop() implementation is
 * called.
 */
static gboolean
example_device_provider_start (GstDeviceProvider * provider)
{
  ExampleDeviceProvider *self = EXAMPLE_DEVICE_PROVIDER (provider);
  GType element_type;
  GTypeClass *element_class;
  GParamSpec *pspec;
  GEnumClass *value_class;
  guint i;

  g_assert (!self->timeout_id);

  element_type = gst_element_factory_get_element_type (self->factory);
  element_class = (GTypeClass *) g_type_class_ref (element_type);
  pspec =
      g_object_class_find_property ((GObjectClass *) element_class, "pattern");
  value_class = (GEnumClass *) g_type_class_ref (pspec->value_type);

  for (i = 0; i < value_class->n_values; i++) {
    GEnumValue *val = &value_class->values[i];

    self->patterns = g_list_append (self->patterns, g_strdup (val->value_nick));
  }

  g_type_class_unref (value_class);
  g_type_class_unref (element_class);

  self->timeout_id =
      g_timeout_add_seconds (NEW_DEVICE_INTERVAL,
      (GSourceFunc) example_device_provider_next_device, self);

  return TRUE;
}

/* Simply stop adding devices by removing our timeout. */
static void
example_device_provider_stop (GstDeviceProvider * provider)
{
  ExampleDeviceProvider *self = EXAMPLE_DEVICE_PROVIDER (provider);

  g_assert (self->timeout_id);

  if (self->patterns) {
    g_list_free_full (self->patterns, g_free);
    self->patterns = NULL;
  }

  g_source_remove (self->timeout_id);
  self->timeout_id = 0;
}

static void
example_device_provider_dispose (GObject * object)
{
  ExampleDeviceProvider *self = EXAMPLE_DEVICE_PROVIDER (object);

  gst_object_replace ((GstObject **) & self->factory, NULL);

  G_OBJECT_CLASS (example_device_provider_parent_class)->dispose (object);
}

static void
example_device_provider_finalize (GObject * object)
{
  ExampleDeviceProvider *self = EXAMPLE_DEVICE_PROVIDER (object);

  if (self->patterns)
    g_list_free_full (self->patterns, g_free);

  G_OBJECT_CLASS (example_device_provider_parent_class)->finalize (object);
}

static void
example_device_provider_class_init (ExampleDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = GST_DEBUG_FUNCPTR (example_device_provider_dispose);
  gobject_class->finalize =
      GST_DEBUG_FUNCPTR (example_device_provider_finalize);

  dm_class->probe = GST_DEBUG_FUNCPTR (example_device_provider_probe);
  dm_class->start = GST_DEBUG_FUNCPTR (example_device_provider_start);
  dm_class->stop = GST_DEBUG_FUNCPTR (example_device_provider_stop);

  gst_device_provider_class_set_static_metadata (dm_class,
      "Example Device Provider", "Source/Video",
      "List and provides example source devices",
      "Mathieu Duponchelle <mathieu@centricular.com>");
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_device_provider_register (plugin, "exampledeviceprovider",
      GST_RANK_PRIMARY, EXAMPLE_TYPE_DEVICE_PROVIDER);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    example_device_provider,
    "Example device provider",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
