/*
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdkcontentprovider.h>

G_BEGIN_DECLS


void                    gdk_content_provider_attach_clipboard   (GdkContentProvider     *provider,
                                                                 GdkClipboard           *clipboard);
void                    gdk_content_provider_detach_clipboard   (GdkContentProvider     *provider,
                                                                 GdkClipboard           *clipboard);

G_END_DECLS

