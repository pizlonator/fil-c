/* GStreamer
 * Copyright (C) 2011 Wim Taymans <wim.taymans@gmail.com>
 *
 * gstmeta.c: metadata operations
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
 * SECTION:gstmeta
 * @title: GstMeta
 * @short_description: Buffer metadata
 *
 * The #GstMeta structure should be included as the first member of a #GstBuffer
 * metadata structure. The structure defines the API of the metadata and should
 * be accessible to all elements using the metadata.
 *
 * A metadata API is registered with gst_meta_api_type_register() which takes a
 * name for the metadata API and some tags associated with the metadata.
 * With gst_meta_api_type_has_tag() one can check if a certain metadata API
 * contains a given tag.
 *
 * Multiple implementations of a metadata API can be registered.
 * To implement a metadata API, gst_meta_register() should be used. This
 * function takes all parameters needed to create, free and transform metadata
 * along with the size of the metadata. The function returns a #GstMetaInfo
 * structure that contains the information for the implementation of the API.
 *
 * A specific implementation can be retrieved by name with gst_meta_get_info().
 *
 * See #GstBuffer for how the metadata can be added, retrieved and removed from
 * buffers.
 */
#include "gst_private.h"

#include "gstbuffer.h"
#include "gstmeta.h"
#include "gstinfo.h"
#include "gstutils.h"
#include "gstquark.h"

static GHashTable *metainfo = NULL;
static GRWLock lock;

GQuark _gst_meta_transform_copy;
GQuark _gst_meta_tag_memory;
GQuark _gst_meta_tag_memory_reference;

typedef struct
{
  GstMetaInfo info;
  GstCustomMetaTransformFunction custom_transform_func;
  gpointer custom_transform_user_data;
  GDestroyNotify custom_transform_destroy_notify;
  gboolean is_custom;
} GstMetaInfoImpl;

static void
free_info (gpointer data)
{
  g_free (data);
}

void
_priv_gst_meta_initialize (void)
{
  g_rw_lock_init (&lock);
  metainfo = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_info);

  _gst_meta_transform_copy = g_quark_from_static_string ("gst-copy");
  _gst_meta_tag_memory = g_quark_from_static_string ("memory");
  _gst_meta_tag_memory_reference =
      g_quark_from_static_string ("memory-reference");
}

static gboolean
notify_custom (gchar * key, GstMetaInfo * info, gpointer unused)
{
  GstMetaInfoImpl *impl = (GstMetaInfoImpl *) info;

  if (impl->is_custom) {
    if (impl->custom_transform_destroy_notify)
      impl->custom_transform_destroy_notify (impl->custom_transform_user_data);
  }
  return TRUE;
}

void
_priv_gst_meta_cleanup (void)
{
  if (metainfo != NULL) {
    g_hash_table_foreach_remove (metainfo, (GHRFunc) notify_custom, NULL);
    g_hash_table_unref (metainfo);
    metainfo = NULL;
  }
}

/**
 * gst_meta_api_type_register:
 * @api: an API to register
 * @tags: (array zero-terminated=1): tags for @api
 *
 * Register and return a GType for the @api and associate it with
 * @tags.
 *
 * Returns: a unique GType for @api.
 */
GType
gst_meta_api_type_register (const gchar * api, const gchar ** tags)
{
  GType type;

  g_return_val_if_fail (api != NULL, 0);
  g_return_val_if_fail (tags != NULL, 0);

  GST_CAT_DEBUG (GST_CAT_META, "register API \"%s\"", api);
  type = g_pointer_type_register_static (api);

  if (type != G_TYPE_INVALID) {
    gint i;

    for (i = 0; tags[i]; i++) {
      GST_CAT_DEBUG (GST_CAT_META, "  adding tag \"%s\"", tags[i]);
      g_type_set_qdata (type, g_quark_from_string (tags[i]),
          GINT_TO_POINTER (TRUE));
    }
  }

  g_type_set_qdata (type, GST_QUARK (TAGS), g_strdupv ((gchar **) tags));

  return type;
}

static gboolean
custom_init_func (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstCustomMeta *cmeta = (GstCustomMeta *) meta;

  cmeta->structure = gst_structure_new_empty (g_type_name (meta->info->type));

  gst_structure_set_parent_refcount (cmeta->structure,
      &GST_MINI_OBJECT_REFCOUNT (buffer));

  return TRUE;
}

static void
custom_free_func (GstMeta * meta, GstBuffer * buffer)
{
  GstCustomMeta *cmeta = (GstCustomMeta *) meta;

  gst_structure_set_parent_refcount (cmeta->structure, NULL);
  gst_structure_free (cmeta->structure);
}

static gboolean
custom_transform_func (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstCustomMeta *custom, *cmeta = (GstCustomMeta *) meta;
  GstMetaInfoImpl *info = (GstMetaInfoImpl *) meta->info;

  if (info->custom_transform_func)
    return info->custom_transform_func (transbuf, cmeta,
        buffer, type, data, info->custom_transform_user_data);

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    custom = (GstCustomMeta *) gst_buffer_add_meta (transbuf, meta->info, NULL);
    gst_structure_set_parent_refcount (custom->structure, NULL);
    gst_structure_take (&custom->structure,
        gst_structure_copy (cmeta->structure));
    gst_structure_set_parent_refcount (custom->structure,
        &GST_MINI_OBJECT_REFCOUNT (transbuf));
  } else {
    return FALSE;
  }

  return TRUE;
}

static gboolean
custom_serialize_func (const GstMeta * meta, GstByteArrayInterface * data,
    guint8 * version)
{
  const GstCustomMeta *cmeta = (const GstCustomMeta *) meta;
  gchar *str = gst_structure_serialize_full (cmeta->structure,
      GST_SERIALIZE_FLAG_STRICT);
  if (str == NULL)
    return FALSE;

  gboolean ret = gst_byte_array_interface_append_data (data, (guint8 *) str,
      strlen (str) + 1);
  g_free (str);

  return ret;
}

static GstMeta *
custom_deserialize_func (const GstMetaInfo * info, GstBuffer * buffer,
    const guint8 * data, gsize size, guint8 version)
{
  if (version != 0 || size < 1 || data[size - 1] != '\0')
    return NULL;

  GstStructure *structure =
      gst_structure_new_from_string ((const gchar *) data);
  if (structure == NULL)
    return NULL;

  GstMeta *meta = gst_buffer_add_meta (buffer, info, NULL);
  GstCustomMeta *cmeta = (GstCustomMeta *) meta;

  gst_structure_set_parent_refcount (cmeta->structure, NULL);
  gst_structure_take (&cmeta->structure, structure);
  gst_structure_set_parent_refcount (cmeta->structure,
      &GST_MINI_OBJECT_REFCOUNT (buffer));

  return meta;
}

/**
 * gst_custom_meta_get_structure:
 *
 * Retrieve the #GstStructure backing a custom meta, the structure's mutability
 * is conditioned to the writability of the #GstBuffer @meta is attached to.
 *
 * Returns: (transfer none): the #GstStructure backing @meta
 * Since: 1.20
 */
GstStructure *
gst_custom_meta_get_structure (GstCustomMeta * meta)
{
  g_return_val_if_fail (meta != NULL, NULL);
  g_return_val_if_fail (gst_meta_info_is_custom (((GstMeta *) meta)->info),
      NULL);

  return meta->structure;
}

/**
 * gst_custom_meta_has_name:
 *
 * Checks whether the name of the custom meta is @name
 *
 * Returns: Whether @name is the name of the custom meta
 * Since: 1.20
 */
gboolean
gst_custom_meta_has_name (GstCustomMeta * meta, const gchar * name)
{
  g_return_val_if_fail (meta != NULL, FALSE);
  g_return_val_if_fail (gst_meta_info_is_custom (((GstMeta *) meta)->info),
      FALSE);

  return gst_structure_has_name (meta->structure, name);
}

/**
 * gst_meta_register_custom:
 * @name: the name of the #GstMeta implementation
 * @tags: (array zero-terminated=1): tags for @api
 * @transform_func: (scope notified) (nullable): a #GstMetaTransformFunction
 * @user_data: (closure): user data passed to @transform_func
 * @destroy_data: #GDestroyNotify for user_data
 *
 * Register a new custom #GstMeta implementation, backed by an opaque
 * structure holding a #GstStructure.
 *
 * The registered info can be retrieved later with gst_meta_get_info() by using
 * @name as the key.
 *
 * The backing #GstStructure can be retrieved with
 * gst_custom_meta_get_structure(), its mutability is conditioned by the
 * writability of the buffer the meta is attached to.
 *
 * When @transform_func is %NULL, the meta and its backing #GstStructure
 * will always be copied when the transform operation is copy, other operations
 * are discarded, copy regions are ignored.
 *
 * Returns: (transfer none): a #GstMetaInfo that can be used to
 * access metadata.
 * Since: 1.20
 */
const GstMetaInfo *
gst_meta_register_custom (const gchar * name, const gchar ** tags,
    GstCustomMetaTransformFunction transform_func,
    gpointer user_data, GDestroyNotify destroy_data)
{
  gchar *api_name = g_strdup_printf ("%s-api", name);
  GType api;
  GstMetaInfo *info;
  GstMetaInfoImpl *impl;
  const GstMetaInfo *ret = NULL;

  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  api = gst_meta_api_type_register (api_name, tags);
  g_free (api_name);
  if (api == G_TYPE_INVALID)
    goto done;

  info = gst_meta_info_new (api, name, sizeof (GstCustomMeta));
  if (info == NULL)
    goto done;

  impl = (GstMetaInfoImpl *) info;

  info->init_func = custom_init_func;
  info->free_func = custom_free_func;
  info->transform_func = custom_transform_func;
  info->serialize_func = custom_serialize_func;
  info->deserialize_func = custom_deserialize_func;

  impl->is_custom = TRUE;
  impl->custom_transform_func = transform_func;
  impl->custom_transform_user_data = user_data;
  impl->custom_transform_destroy_notify = destroy_data;

  ret = gst_meta_info_register (info);

done:
  return ret;
}

/**
 * gst_meta_register_custom_simple:
 * @name: the name of the #GstMeta implementation
 *
 * Simplified version of gst_meta_register_custom(), with no tags and no
 * transform function.
 *
 * Returns: (transfer none): a #GstMetaInfo that can be used to access metadata.
 * Since: 1.24
 */
const GstMetaInfo *
gst_meta_register_custom_simple (const gchar * name)
{
  const gchar *tags[] = { NULL };
  return gst_meta_register_custom (name, tags, NULL, NULL, NULL);
}

/**
 * gst_meta_info_is_custom:
 *
 * Returns: whether @info was registered as a #GstCustomMeta with
 *   gst_meta_register_custom()
 * Since:1.20
 */
gboolean
gst_meta_info_is_custom (const GstMetaInfo * info)
{
  g_return_val_if_fail (info != NULL, FALSE);

  return ((GstMetaInfoImpl *) info)->is_custom;
}

/**
 * gst_meta_api_type_has_tag:
 * @api: an API
 * @tag: the tag to check
 *
 * Check if @api was registered with @tag.
 *
 * Returns: %TRUE if @api was registered with @tag.
 */
gboolean
gst_meta_api_type_has_tag (GType api, GQuark tag)
{
  g_return_val_if_fail (api != 0, FALSE);
  g_return_val_if_fail (tag != 0, FALSE);

  return g_type_get_qdata (api, tag) != NULL;
}

/**
 * gst_meta_api_type_get_tags:
 * @api: an API
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type utf8): an array of tags as strings.
 *
 * Since: 1.2
 */
const gchar *const *
gst_meta_api_type_get_tags (GType api)
{
  const gchar **tags;
  g_return_val_if_fail (api != 0, FALSE);

  tags = g_type_get_qdata (api, GST_QUARK (TAGS));

  if (!tags[0])
    return NULL;

  return (const gchar * const *) tags;
}

static const GstMetaInfo *
gst_meta_register_internal (GType api, const gchar * impl, gsize size,
    GstMetaInitFunction init_func, GstMetaFreeFunction free_func,
    GstMetaTransformFunction transform_func,
    GstMetaSerializeFunction serialize_func,
    GstMetaDeserializeFunction deserialize_func)
{
  GstMetaInfo *info;
  if (init_func == NULL)
    g_critical ("Registering meta implementation '%s' without init function",
        impl);

  info = gst_meta_info_new (api, impl, size);
  if (info == NULL)
    return NULL;

  info->init_func = init_func;
  info->free_func = free_func;
  info->transform_func = transform_func;
  info->serialize_func = serialize_func;
  info->deserialize_func = deserialize_func;
  ((GstMetaInfoImpl *) info)->is_custom = FALSE;

  return gst_meta_info_register (info);
}

/**
 * gst_meta_register: (skip):
 * @api: the type of the #GstMeta API
 * @impl: the name of the #GstMeta implementation
 * @size: the size of the #GstMeta structure
 * @init_func: a #GstMetaInitFunction
 * @free_func: a #GstMetaFreeFunction
 * @transform_func: a #GstMetaTransformFunction
 *
 * Register a new #GstMeta implementation.
 *
 * The same @info can be retrieved later with gst_meta_get_info() by using
 * @impl as the key.
 *
 * Returns: (transfer none): a #GstMetaInfo that can be used to
 * access metadata.
 */
const GstMetaInfo *
gst_meta_register (GType api, const gchar * impl, gsize size,
    GstMetaInitFunction init_func, GstMetaFreeFunction free_func,
    GstMetaTransformFunction transform_func)
{
  return gst_meta_register_internal (api, impl, size, init_func, free_func,
      transform_func, NULL, NULL);
}

/**
 * gst_meta_info_new: (skip):
 * @api:  the type of the #GstMeta API
 * @impl: the name of the #GstMeta implementation
 * @size: the size of the #GstMeta structure
 *
 * Creates a new structure that needs to be filled before being
 * registered.  This structure should filled and then registered with
 * gst_meta_info_register().
 *
 * Example:
 * ```c
 * const GstMetaInfo *
 * gst_my_meta_get_info (void)
 * {
 *   static const GstMetaInfo *meta_info = NULL;
 *
 *   if (g_once_init_enter_pointer ((GstMetaInfo **) & meta_info)) {
 *     GstMetaInfo *info = gst_meta_info_new (
 *       gst_my_meta_api_get_type (),
 *         "GstMyMeta",
 *        sizeof (GstMyMeta));
 *     const GstMetaInfo *meta = NULL;
 *
 *     info->init_func = my_meta_init;
 *     info->free_func = my_meta_free;
 *     info->transform_func = my_meta_transform;
 *     info->serialize_func = my_meta_serialize;
 *     info->deserialize_func = my_meta_deserialize;
 *     meta = gst_meta_info_register (info);
 *     g_once_init_leave_pointer ((GstMetaInfo **) & meta_info, (GstMetaInfo *) meta);
 *   }
 *
 *   return meta_info;
 * }
 * ```
 *
 * Returns: a new #GstMetaInfo that needs to be filled
 *
 * Since: 1.24
 */

GstMetaInfo *
gst_meta_info_new (GType api, const gchar * impl, gsize size)
{
  GType type;
  GstMetaInfo *info;

  g_return_val_if_fail (api != 0, NULL);
  g_return_val_if_fail (impl != NULL, NULL);
  g_return_val_if_fail (size != 0, NULL);

  /* first try to register the implementation name. It's possible
   * that this fails because it was already registered. Don't warn,
   * glib did this for us already. */
  type = g_pointer_type_register_static (impl);

  info = (GstMetaInfo *) g_new0 (GstMetaInfoImpl, 1);
  info->api = api;
  info->type = type;
  info->size = size;

  return info;
}

/**
 * gst_meta_info_register:
 * @info: (transfer full): a new #GstMetaInfo created by gst_meta_info_new()
 *
 * Registers a new meta.
 *
 * Use the structure returned by gst_meta_info_new(), it consumes it and the
 * structure shouldnt be used after. The one returned by the function can be
 * kept.
 *
 * Returns: (transfer none): the registered meta
 *
 * Since: 1.24
 */

const GstMetaInfo *
gst_meta_info_register (GstMetaInfo * info)
{
  if (info->type == G_TYPE_INVALID) {
    g_free (info);
    return NULL;
  }

  GST_CAT_DEBUG (GST_CAT_META,
      "register \"%s\" implementing \"%s\" of size %" G_GSIZE_FORMAT,
      g_type_name (info->type), g_type_name (info->api), info->size);

  g_rw_lock_writer_lock (&lock);
  g_hash_table_insert (metainfo,
      (gpointer) g_intern_string (g_type_name (info->type)), (gpointer) info);
  g_rw_lock_writer_unlock (&lock);

  return info;
}

/**
 * gst_meta_get_info:
 * @impl: the name
 *
 * Lookup a previously registered meta info structure by its implementation name
 * @impl.
 *
 * Returns: (transfer none) (nullable): a #GstMetaInfo with @impl, or
 * %NULL when no such metainfo exists.
 */
const GstMetaInfo *
gst_meta_get_info (const gchar * impl)
{
  GstMetaInfo *info;

  g_return_val_if_fail (impl != NULL, NULL);

  g_rw_lock_reader_lock (&lock);
  info = g_hash_table_lookup (metainfo, impl);
  g_rw_lock_reader_unlock (&lock);

  return info;
}

/**
 * gst_meta_get_seqnum:
 * @meta: a #GstMeta
 *
 * Gets seqnum for this meta.
 *
 * Since: 1.16
 */
guint64
gst_meta_get_seqnum (const GstMeta * meta)
{
  GstMetaItem *meta_item;
  guint8 *p;

  g_return_val_if_fail (meta != NULL, 0);

  p = (guint8 *) meta;
  p -= G_STRUCT_OFFSET (GstMetaItem, meta);
  meta_item = (GstMetaItem *) p;
  return meta_item->seq_num;
}

/**
 * gst_meta_compare_seqnum:
 * @meta1: a #GstMeta
 * @meta2: a #GstMeta
 *
 * Meta sequence number compare function. Can be used as #GCompareFunc
 * or a #GCompareDataFunc.
 *
 * Returns: a negative number if @meta1 comes before @meta2, 0 if both metas
 *   have an equal sequence number, or a positive integer if @meta1 comes
 *   after @meta2.
 *
 * Since: 1.16
 */
gint
gst_meta_compare_seqnum (const GstMeta * meta1, const GstMeta * meta2)
{
  guint64 seqnum1 = gst_meta_get_seqnum (meta1);
  guint64 seqnum2 = gst_meta_get_seqnum (meta2);

  if (seqnum1 == seqnum2)
    return 0;

  return (seqnum1 < seqnum2) ? -1 : 1;
}

/**
 * gst_meta_serialize:
 * @meta: a #GstMeta
 * @data: #GstByteArrayInterface to append serialization data
 *
 * Serialize @meta into a format that can be stored or transmitted and later
 * deserialized by gst_meta_deserialize().
 *
 * This is only supported for meta that implements #GstMetaInfo.serialize_func,
 * %FALSE is returned otherwise.
 *
 * Upon failure, @data->data pointer could have been reallocated, but @data->len
 * won't be modified. This is intended to be able to append multiple metas
 * into the same #GByteArray.
 *
 * Since serialization size is often the same for every buffer, caller may want
 * to remember the size of previous data to preallocate the next.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 1.24
 */
gboolean
gst_meta_serialize (const GstMeta * meta, GstByteArrayInterface * data)
{
  g_return_val_if_fail (meta != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (meta->info->serialize_func != NULL) {
    const gchar *name = g_type_name (meta->info->type);
    guint32 name_len = strlen (name);
    guint32 orig_len = data->len;
    guint8 version = 0;

    /* Format: [total size][name_len][name][\0][version][payload]
     * Preallocate space for header but only write it on success because we
     * don't have every info yet.
     */
    guint8 header_size = 2 * sizeof (guint32) + name_len + 2;
    if (!gst_byte_array_interface_set_size (data, data->len + header_size))
      return FALSE;
    if (meta->info->serialize_func (meta, data, &version)) {
      guint8 *header = data->data + orig_len;
      GST_WRITE_UINT32_LE (header + 0, data->len - orig_len);
      GST_WRITE_UINT32_LE (header + 4, name_len);
      memcpy (header + 8, name, name_len + 1);
      header[header_size - 1] = version;
      return TRUE;
    }
    // Serialization failed, rollback.
    gst_byte_array_interface_set_size (data, orig_len);
  }

  return FALSE;
}

typedef struct
{
  GstByteArrayInterface parent;
  GByteArray *data;
} ByteArrayImpl;

static gboolean
byte_array_impl_resize (GstByteArrayInterface * parent, gsize length)
{
  ByteArrayImpl *self = (ByteArrayImpl *) parent;

  g_byte_array_set_size (self->data, length);
  parent->data = self->data->data;
  return TRUE;
}

/**
 * gst_meta_serialize_simple:
 * @meta: a #GstMeta
 * @data: #GByteArray to append serialization data
 *
 * Same as gst_meta_serialize() but with a #GByteArray instead of
 * #GstByteArrayInterface.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 1.24
 */
gboolean
gst_meta_serialize_simple (const GstMeta * meta, GByteArray * data)
{
  ByteArrayImpl impl;

  gst_byte_array_interface_init (&impl.parent);
  impl.parent.data = data->data;
  impl.parent.len = data->len;
  impl.parent.resize = byte_array_impl_resize;
  impl.data = data;
  return gst_meta_serialize (meta, (GstByteArrayInterface *) & impl);
}

/**
 * gst_meta_deserialize:
 * @buffer: a #GstBuffer
 * @data: serialization data obtained from gst_meta_serialize()
 * @size: size of @data
 * @consumed: (out): total size used by this meta, could be less than @size
 *
 * Recreate a #GstMeta from serialized data returned by
 * gst_meta_serialize() and add it to @buffer.
 *
 * Note that the meta must have been previously registered by calling one of
 * `gst_*_meta_get_info ()` functions.
 *
 * @consumed is set to the number of bytes that can be skipped from @data to
 * find the next meta serialization, if any. In case of parsing error that does
 * not allow to determine that size, @consumed is set to 0.
 *
 * Returns: (transfer none) (nullable): the metadata owned by @buffer, or %NULL.
 *
 * Since: 1.24
 */
GstMeta *
gst_meta_deserialize (GstBuffer * buffer, const guint8 * data, gsize size,
    guint32 * consumed)
{
  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (consumed != NULL, NULL);

  *consumed = 0;

  /* Format: [total size][name_len][name][\0][version][payload] */
  if (size < 2 * sizeof (guint32))
    goto bad_header;

  guint32 total_size = GST_READ_UINT32_LE (data + 0);
  guint32 name_len = GST_READ_UINT32_LE (data + 4);
  guint32 header_size = 2 * sizeof (guint32) + name_len + 2;
  if (size < total_size || total_size < header_size)
    goto bad_header;

  guint8 version = data[header_size - 1];
  const gchar *name = (const gchar *) (data + 2 * sizeof (guint32));
  if (name[name_len] != '\0')
    goto bad_header;

  *consumed = total_size;

  const GstMetaInfo *info = gst_meta_get_info (name);
  if (info == NULL) {
    GST_CAT_WARNING (GST_CAT_META,
        "%s does not correspond to a registered meta", name);
    return NULL;
  }

  if (info->deserialize_func == NULL) {
    GST_CAT_WARNING (GST_CAT_META, "Meta %s does not support deserialization",
        name);
    return NULL;
  }

  const guint8 *payload = data + header_size;
  guint32 payload_size = total_size - header_size;
  GstMeta *meta =
      info->deserialize_func (info, buffer, payload, payload_size, version);
  if (meta == NULL) {
    GST_CAT_WARNING (GST_CAT_META, "Failed to deserialize %s payload", name);
    GST_CAT_MEMDUMP (GST_CAT_META, "Meta serialization payload", payload,
        payload_size);
    return NULL;
  }

  return meta;

bad_header:
  GST_CAT_WARNING (GST_CAT_META, "Could not parse meta serialization header");
  GST_CAT_MEMDUMP (GST_CAT_META, "Meta serialization data", data, size);
  return NULL;
}
