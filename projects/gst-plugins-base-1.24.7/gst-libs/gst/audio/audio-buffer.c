/* GStreamer
 * Copyright (C) <2018> Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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
#  include "config.h"
#endif

#include "audio-buffer.h"


static void
gst_audio_buffer_unmap_internal (GstAudioBuffer * buffer, guint n_unmap)
{
  guint i;
  for (i = 0; i < n_unmap; i++) {
    gst_buffer_unmap (buffer->buffer, &buffer->map_infos[i]);
  }
  if (buffer->planes != buffer->priv_planes_arr)
    g_free (buffer->planes);
  if (buffer->map_infos != buffer->priv_map_infos_arr)
    g_free (buffer->map_infos);
}

/**
 * gst_audio_buffer_unmap:
 * @buffer: the #GstAudioBuffer to unmap
 *
 * Unmaps an audio buffer that was previously mapped with
 * gst_audio_buffer_map().
 *
 * Since: 1.16
 */
void
gst_audio_buffer_unmap (GstAudioBuffer * buffer)
{
  gst_audio_buffer_unmap_internal (buffer, buffer->n_planes);
}

/**
 * gst_audio_buffer_map:
 * @buffer: (out caller-allocates): pointer to a #GstAudioBuffer
 * @info: the audio properties of the buffer
 * @gstbuffer: (transfer none): the #GstBuffer to be mapped
 * @flags: the access mode for the memory
 *
 * Maps an audio @gstbuffer so that it can be read or written and stores the
 * result of the map operation in @buffer.
 *
 * This is especially useful when the @gstbuffer is in non-interleaved (planar)
 * layout, in which case this function will use the information in the
 * @gstbuffer's attached #GstAudioMeta in order to map each channel in a
 * separate "plane" in #GstAudioBuffer. If a #GstAudioMeta is not attached
 * on the @gstbuffer, then it must be in interleaved layout.
 *
 * If a #GstAudioMeta is attached, then the #GstAudioInfo on the meta is checked
 * against @info. Normally, they should be equal, but in case they are not,
 * a g_critical will be printed and the #GstAudioInfo from the meta will be
 * used.
 *
 * In non-interleaved buffers, it is possible to have each channel on a separate
 * #GstMemory. In this case, each memory will be mapped separately to avoid
 * copying their contents in a larger memory area. Do note though that it is
 * not supported to have a single channel spanning over two or more different
 * #GstMemory objects. Although the map operation will likely succeed in this
 * case, it will be highly sub-optimal and it is recommended to merge all the
 * memories in the buffer before calling this function.
 *
 * Note: The actual #GstBuffer is not ref'ed, but it is required to stay valid
 * as long as it's mapped.
 *
 * Returns: %TRUE if the map operation succeeded or %FALSE on failure
 *
 * Since: 1.16
 */
gboolean
gst_audio_buffer_map (GstAudioBuffer * buffer, const GstAudioInfo * info,
    GstBuffer * gstbuffer, GstMapFlags flags)
{
  GstAudioMeta *meta = NULL;
  guint i = 0, idx, length;
  gsize skip;

  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GST_AUDIO_INFO_IS_VALID (info), FALSE);
  g_return_val_if_fail (GST_AUDIO_INFO_FORMAT (info) !=
      GST_AUDIO_FORMAT_UNKNOWN, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (gstbuffer), FALSE);

  meta = gst_buffer_get_audio_meta (gstbuffer);

  /* be strict on the layout */
  g_return_val_if_fail ((!meta && info->layout == GST_AUDIO_LAYOUT_INTERLEAVED)
      || (meta && info->layout == meta->info.layout), FALSE);

  /* and not so strict on other fields */
  if (G_UNLIKELY (meta && !gst_audio_info_is_equal (&meta->info, info))) {
    g_critical ("the GstAudioInfo argument is not equal "
        "to the GstAudioMeta's attached info");
  }

  if (meta) {
    /* make sure that the meta doesn't imply having more samples than
     * what's actually possible to store in this buffer */
    g_return_val_if_fail (meta->samples <=
        gst_buffer_get_size (gstbuffer) / GST_AUDIO_INFO_BPF (&meta->info),
        FALSE);
    buffer->n_samples = meta->samples;
  } else {
    buffer->n_samples =
        gst_buffer_get_size (gstbuffer) / GST_AUDIO_INFO_BPF (info);
  }

  buffer->info = meta ? meta->info : *info;
  buffer->buffer = gstbuffer;

  if (GST_AUDIO_BUFFER_LAYOUT (buffer) == GST_AUDIO_LAYOUT_INTERLEAVED) {
    /* interleaved */
    buffer->n_planes = 1;
    buffer->planes = buffer->priv_planes_arr;
    buffer->map_infos = buffer->priv_map_infos_arr;

    if (!gst_buffer_map (gstbuffer, &buffer->map_infos[0], flags))
      return FALSE;

    buffer->planes[0] = buffer->map_infos[0].data;
  } else {
    /* non-interleaved */
    buffer->n_planes = GST_AUDIO_BUFFER_CHANNELS (buffer);

    if (G_UNLIKELY (buffer->n_planes > 8)) {
      buffer->planes = g_new (gpointer, buffer->n_planes);
      buffer->map_infos = g_new (GstMapInfo, buffer->n_planes);
    } else {
      buffer->planes = buffer->priv_planes_arr;
      buffer->map_infos = buffer->priv_map_infos_arr;
    }

    if (buffer->n_samples == 0) {
      memset (buffer->map_infos, 0,
          buffer->n_planes * sizeof (buffer->map_infos[0]));
      memset (buffer->planes, 0, buffer->n_planes * sizeof (buffer->planes[0]));
    } else {
      for (i = 0; i < buffer->n_planes; i++) {
        if (!gst_buffer_find_memory (gstbuffer, meta->offsets[i],
                GST_AUDIO_BUFFER_PLANE_SIZE (buffer), &idx, &length, &skip))
          goto no_memory;

        if (!gst_buffer_map_range (gstbuffer, idx, length,
                &buffer->map_infos[i], flags))
          goto cannot_map;

        buffer->planes[i] = buffer->map_infos[i].data + skip;
      }
    }
  }

  return TRUE;

no_memory:
  {
    GST_DEBUG ("plane %u, no memory at offset %" G_GSIZE_FORMAT, i,
        meta->offsets[i]);
    gst_audio_buffer_unmap_internal (buffer, i);
    return FALSE;
  }
cannot_map:
  {
    GST_DEBUG ("cannot map memory range %u-%u", idx, length);
    gst_audio_buffer_unmap_internal (buffer, i);
    return FALSE;
  }
}
