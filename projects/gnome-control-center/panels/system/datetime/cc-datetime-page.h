/*
 * cc-datetime-page.h
 *
 * Copyright 2023 Gotam Gorabh <gautamy672@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_DATE_TIME_PAGE (cc_date_time_page_get_type ())
G_DECLARE_FINAL_TYPE (CcDateTimePage, cc_date_time_page, CC, DATE_TIME_PAGE, AdwNavigationPage)

G_END_DECLS
