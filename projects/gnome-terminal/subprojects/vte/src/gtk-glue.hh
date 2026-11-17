/*
 * Copyright © 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "std-glue.hh"

namespace vte::gtk {

} // namespace vte::gtk

namespace vte {

#if VTE_GTK == 3
VTE_DECLARE_FREEABLE(GtkTargetList, gtk_target_list_unref);
#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4
VTE_DECLARE_FREEABLE(GdkContentFormats, gdk_content_formats_unref);
VTE_DECLARE_FREEABLE(GdkContentFormatsBuilder, gdk_content_formats_builder_unref);
#endif /* VTE_GTK == 4 */

} // namespace vte
