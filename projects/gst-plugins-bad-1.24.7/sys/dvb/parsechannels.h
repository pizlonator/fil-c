/*
 * parsechannels.h - Parse channels.conf
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
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

#ifndef PARSE_CHANNELS_H
#define PARSE_CHANNELS_H

gboolean set_properties_for_channel (GstElement  * dvbbasebin,
                                     const gchar * channel_name,
                                     GError     ** error);

#endif
