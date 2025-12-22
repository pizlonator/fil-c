/* GStreamer
 * Copyright (C) 2009 Tiago Katcipis <tiagokatcipis@gmail.com>
 * Copyright (C) 2009 Paulo Pizarro  <paulo.pizarro@gmail.com>
 * Copyright (C) 2009 Rogério Santos <rogerio.santos@digitro.com.br>
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include "vad_private.h"

#define VAD_POWER_ALPHA     0x0800      /* Q16 */
#define VAD_ZCR_THRESHOLD   0
#define VAD_BUFFER_SIZE     256


union pgen
{
  guint64 a;
  gpointer v;
  guint64 *l;
  guchar *b;
  guint16 *w;
  gint16 *s;
};

struct _cqueue_s
{
  union pgen base;
  union pgen tail;
  union pgen head;
  gint size;
};

typedef struct _cqueue_s cqueue_t;

struct _vad_s
{
  gint16 vad_buffer[VAD_BUFFER_SIZE];
  cqueue_t cqueue;
  gint vad_state;
  guint64 hysteresis;
  guint64 vad_samples;
  guint64 vad_power;
  guint64 threshold;
  long vad_zcr;
};

VADFilter *
vad_new (guint64 hysteresis, gint threshold)
{
  VADFilter *vad = malloc (sizeof (VADFilter));
  vad_reset (vad);
  vad->hysteresis = hysteresis;
  vad_set_threshold (vad, threshold);
  return vad;
}

void
vad_reset (VADFilter * vad)
{
  memset (vad, 0, sizeof (*vad));
  vad->cqueue.base.s = vad->vad_buffer;
  vad->cqueue.tail.a = vad->cqueue.head.a = 0;
  vad->cqueue.size = VAD_BUFFER_SIZE;
  vad->vad_state = VAD_SILENCE;
}

void
vad_destroy (VADFilter * p)
{
  free (p);
}

void
vad_set_hysteresis (struct _vad_s *p, guint64 hysteresis)
{
  p->hysteresis = hysteresis;
}

guint64
vad_get_hysteresis (struct _vad_s *p)
{
  return p->hysteresis;
}

void
vad_set_threshold (struct _vad_s *p, gint threshold_db)
{
  gint power = (gint) (threshold_db / 10.0);
  p->threshold = (guint64) (pow (10, (power)) * 4294967295UL);
}

gint
vad_get_threshold_as_db (struct _vad_s *p)
{
  return (gint) (10 * log10 (p->threshold / 4294967295.0));
}

gint
vad_update (struct _vad_s *p, gint16 * data, gint len)
{
  guint64 tail;
  gint frame_type;
  gint16 sample;
  gint i;

  for (i = 0; i < len; i++) {
    p->vad_power = VAD_POWER_ALPHA * ((data[i] * data[i] >> 14) & 0xFFFF) +
        (0xFFFF - VAD_POWER_ALPHA) * (p->vad_power >> 16) +
        ((0xFFFF - VAD_POWER_ALPHA) * (p->vad_power & 0xFFFF) >> 16);
    /* Update VAD buffer */
    p->cqueue.base.s[p->cqueue.head.a] = data[i];
    p->cqueue.head.a = (p->cqueue.head.a + 1) & (p->cqueue.size - 1);
    if (p->cqueue.head.a == p->cqueue.tail.a)
      p->cqueue.tail.a = (p->cqueue.tail.a + 1) & (p->cqueue.size - 1);
  }

  tail = p->cqueue.tail.a;
  p->vad_zcr = 0;
  for (;;) {
    sample = p->cqueue.base.s[tail];
    tail = (tail + 1) & (p->cqueue.size - 1);
    if (tail == p->cqueue.head.a)
      break;
    p->vad_zcr +=
        ((sample & 0x8000) != (p->cqueue.base.s[tail] & 0x8000)) ? 1 : -1;
  }

  frame_type = (p->vad_power > p->threshold
      && p->vad_zcr < VAD_ZCR_THRESHOLD) ? VAD_VOICE : VAD_SILENCE;

  if (p->vad_state != frame_type) {
    /* Voice to silence transition */
    if (p->vad_state == VAD_VOICE) {
      p->vad_samples += len;
      if (p->vad_samples >= p->hysteresis) {
        p->vad_state = frame_type;
        p->vad_samples = 0;
      }
    } else {
      p->vad_state = frame_type;
      p->vad_samples = 0;
    }
  } else {
    p->vad_samples = 0;
  }

  return p->vad_state;
}
