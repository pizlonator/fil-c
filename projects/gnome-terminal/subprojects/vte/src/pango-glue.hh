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

#include <pango/pango.h>

#include "std-glue.hh"

namespace vte {

VTE_DECLARE_FREEABLE(PangoAttrList, pango_attr_list_unref);
VTE_DECLARE_FREEABLE(PangoFontDescription, pango_font_description_free);

#if PANGO_VERSION_CHECK(1, 44, 0)
VTE_DECLARE_FREEABLE(PangoFontMetrics, pango_font_metrics_unref);
#endif

} // namespace vte
