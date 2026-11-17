/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_USAGE_PAGE (cc_usage_page_get_type ())
G_DECLARE_FINAL_TYPE (CcUsagePage, cc_usage_page, CC, USAGE_PAGE, AdwNavigationPage)

typedef enum {
  CC_USAGE_PAGE_PURGE_AFTER_1_HOUR  = 0,
  CC_USAGE_PAGE_PURGE_AFTER_1_DAY   = 1,
  CC_USAGE_PAGE_PURGE_AFTER_2_DAYS  = 2,
  CC_USAGE_PAGE_PURGE_AFTER_3_DAYS  = 3,
  CC_USAGE_PAGE_PURGE_AFTER_4_DAYS  = 4,
  CC_USAGE_PAGE_PURGE_AFTER_5_DAYS  = 5,
  CC_USAGE_PAGE_PURGE_AFTER_6_DAYS  = 6,
  CC_USAGE_PAGE_PURGE_AFTER_7_DAYS  = 7,
  CC_USAGE_PAGE_PURGE_AFTER_14_DAYS = 14,
  CC_USAGE_PAGE_PURGE_AFTER_30_DAYS = 30,
} CcUsagePagePurgeAfter;

typedef enum {
  CC_USAGE_PAGE_RETAIN_HISTORY_1_DAY   = 1,
  CC_USAGE_PAGE_RETAIN_HISTORY_7_DAYS  = 7,
  CC_USAGE_PAGE_RETAIN_HISTORY_30_DAYS = 30,
  CC_USAGE_PAGE_RETAIN_HISTORY_FOREVER = -1,
} CcUsagePageRetainHistory;

G_END_DECLS
