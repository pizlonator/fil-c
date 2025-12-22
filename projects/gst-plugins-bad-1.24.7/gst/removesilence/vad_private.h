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


#ifndef __VAD_FILTER_H__
#define __VAD_FILTER_H__

#define VAD_SILENCE  0
#define VAD_VOICE    1


typedef struct _vad_s VADFilter;

gint vad_update(VADFilter *p, gint16 *data, gint len);

void vad_set_hysteresis(VADFilter *p, guint64 hysteresis);

guint64 vad_get_hysteresis(VADFilter *p);

void vad_set_threshold(VADFilter *p, gint threshold_db);

gint vad_get_threshold_as_db(VADFilter *p);

VADFilter* vad_new(guint64 hysteresis, gint threshold);

void vad_reset(VADFilter *p);

void vad_destroy(VADFilter *p);

#endif /* __VAD_FILTER__ */
