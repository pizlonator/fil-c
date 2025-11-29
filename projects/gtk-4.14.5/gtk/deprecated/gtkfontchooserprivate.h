/* gtkfontprivatechooser.h - Interface definitions for font selectors UI
 *
 * Copyright (C) 2006 Emmanuele Bassi
 *
 * All rights reserved
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkfontchooser.h"

#define GTK_FONT_CHOOSER_DEFAULT_FONT_NAME "Sans 10"

G_BEGIN_DECLS

void            _gtk_font_chooser_font_activated        (GtkFontChooser *chooser,
                                                         const char     *fontname);

G_END_DECLS

