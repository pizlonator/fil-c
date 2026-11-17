/*
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-level-bar.h"
#include "cc-sound-enums.h"
#include "gvc-mixer-stream-private.h"

struct _CcLevelBar
{
  GtkWidget    parent_instance;

  GtkLevelBar *level_bar;
  pa_stream   *level_stream;
};

G_DEFINE_TYPE (CcLevelBar, cc_level_bar, GTK_TYPE_WIDGET)

#define SMOOTHING 0.3

static void
update_level (CcLevelBar *self,
              gdouble     value)
{
  /* Use Exponential Moving Average (EMA) to smooth out value changes and
   * reduce fluctuation and jitter.
   */
  double prev_ema = gtk_level_bar_get_value (self->level_bar);
  double ema = (value * SMOOTHING) + (prev_ema * (1.0 - SMOOTHING));

  ema = CLAMP (ema, 0.0, 1.0);

  gtk_level_bar_set_value (self->level_bar, ema);
}

static void
read_cb (pa_stream *stream,
         size_t     length,
         void      *userdata)
{
  CcLevelBar *self = userdata;
  const void *data;
  gdouble value;

  if (pa_stream_peek (stream, &data, &length) < 0)
    {
      g_warning ("Failed to read data from stream");
      return;
    }

  if (!data)
    {
      pa_stream_drop (stream);
      return;
    }

  assert (length > 0);
  assert (length % sizeof (float) == 0);

  value = ((const float *) data)[length / sizeof (float) -1];

  pa_stream_drop (stream);

  update_level (self, value);
}

static void
suspended_cb (pa_stream *stream,
              void      *userdata)
{
  CcLevelBar *self = userdata;

  if (pa_stream_is_suspended (stream))
    {
      g_debug ("Stream suspended");
      gtk_level_bar_set_value (self->level_bar, 0.0);
    }
}

static void
close_stream (pa_stream *stream)
{
  if (stream == NULL)
    return;

  /* Stop receiving data */
  pa_stream_set_read_callback (stream, NULL, NULL);
  pa_stream_set_suspended_callback (stream, NULL, NULL);

  /* Disconnect from the stream */
  pa_stream_disconnect (stream);
}

static void
cc_level_bar_dispose (GObject *object)
{
  CcLevelBar *self = CC_LEVEL_BAR (object);

  close_stream (self->level_stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  gtk_widget_unparent (GTK_WIDGET (self->level_bar));

  G_OBJECT_CLASS (cc_level_bar_parent_class)->dispose (object);
}

void
cc_level_bar_class_init (CcLevelBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_level_bar_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
cc_level_bar_init (CcLevelBar *self)
{
  self->level_bar = GTK_LEVEL_BAR (gtk_level_bar_new ());

  // Make the level bar all the same color by removing all pre-existing offsets
  gtk_level_bar_remove_offset_value (self->level_bar, GTK_LEVEL_BAR_OFFSET_LOW);
  gtk_level_bar_remove_offset_value (self->level_bar, GTK_LEVEL_BAR_OFFSET_HIGH);
  gtk_level_bar_remove_offset_value (self->level_bar, GTK_LEVEL_BAR_OFFSET_FULL);

  gtk_widget_set_parent (GTK_WIDGET (self->level_bar), GTK_WIDGET (self));
}

void
cc_level_bar_set_stream (CcLevelBar     *self,
                         GvcMixerStream *stream)
{
  pa_context *context;
  pa_sample_spec sample_spec;
  pa_proplist *proplist;
  pa_buffer_attr  attr;
  g_autofree gchar *device = NULL;

  g_return_if_fail (CC_IS_LEVEL_BAR (self));

  close_stream (self->level_stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  if (stream == NULL)
   {
     gtk_level_bar_set_value (self->level_bar, 0.0);
     return;
   }

  context = gvc_mixer_stream_get_pa_context (stream);

  if (pa_context_get_server_protocol_version (context) < 13)
    {
      g_warning ("Unsupported version of PulseAudio");
      return;
    }

  sample_spec.channels = 1;
  sample_spec.format = PA_SAMPLE_FLOAT32;
  sample_spec.rate = 25;

  proplist = pa_proplist_new ();
  pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, "org.gnome.VolumeControl");
  self->level_stream = pa_stream_new_with_proplist (context, "Peak detect", &sample_spec, NULL, proplist);
  pa_proplist_free (proplist);
  if (self->level_stream == NULL)
    {
      g_warning ("Failed to create monitoring stream");
      return;
    }

  pa_stream_set_read_callback (self->level_stream, read_cb, self);
  pa_stream_set_suspended_callback (self->level_stream, suspended_cb, self);

  memset (&attr, 0, sizeof (attr));
  attr.fragsize = sizeof (float);
  attr.maxlength = (uint32_t) -1;
  device = g_strdup_printf ("%u", gvc_mixer_stream_get_index (stream));
  if (pa_stream_connect_record (self->level_stream,
                                device,
                                &attr,
                                (pa_stream_flags_t) (PA_STREAM_DONT_MOVE |
                                                     PA_STREAM_PEAK_DETECT |
                                                     PA_STREAM_ADJUST_LATENCY)) < 0)
    {
      g_warning ("Failed to connect monitoring stream");
    }
}
