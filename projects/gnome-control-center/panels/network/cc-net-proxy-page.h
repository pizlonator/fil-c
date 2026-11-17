/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_NET_PROXY_PAGE (cc_net_proxy_page_get_type ())

G_DECLARE_FINAL_TYPE (CcNetProxyPage, cc_net_proxy_page, CC, NET_PROXY_PAGE, AdwNavigationPage)

gboolean  cc_net_proxy_page_get_enabled     (CcNetProxyPage *self);
void      cc_net_proxy_page_set_enabled     (CcNetProxyPage *self,
                                             gboolean        enable);
gboolean  cc_net_proxy_page_has_modified    (CcNetProxyPage *self);
void      cc_net_proxy_page_save_changes    (CcNetProxyPage *self);
void      cc_net_proxy_page_cancel_changes  (CcNetProxyPage *self);

G_END_DECLS
