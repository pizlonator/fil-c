/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

G_BEGIN_DECLS

typedef enum {
       GSD_SHARING_STATUS_OFFLINE,
       GSD_SHARING_STATUS_DISABLED_MOBILE_BROADBAND,
       GSD_SHARING_STATUS_DISABLED_LOW_SECURITY,
       GSD_SHARING_STATUS_AVAILABLE
} GsdSharingStatus;

G_END_DECLS
