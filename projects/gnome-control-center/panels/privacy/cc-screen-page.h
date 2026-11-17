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

#define CC_TYPE_SCREEN_PAGE (cc_screen_page_get_type ())
G_DECLARE_FINAL_TYPE (CcScreenPage, cc_screen_page, CC, SCREEN_PAGE, AdwNavigationPage)

typedef enum {
  CC_SCREEN_PAGE_LOCK_AFTER_SCREEN_OFF = 0,
  CC_SCREEN_PAGE_LOCK_AFTER_30_SEC     = 30,
  CC_SCREEN_PAGE_LOCK_AFTER_1_MIN      = 60,
  CC_SCREEN_PAGE_LOCK_AFTER_2_MIN      = 120,
  CC_SCREEN_PAGE_LOCK_AFTER_3_MIN      = 180,
  CC_SCREEN_PAGE_LOCK_AFTER_5_MIN      = 300,
  CC_SCREEN_PAGE_LOCK_AFTER_30_MIN     = 1800,
  CC_SCREEN_PAGE_LOCK_AFTER_1_HR       = 3600,
} CcScreenPageLockAfter;

typedef enum {
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_1_MIN  = 60,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_2_MIN  = 120,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_3_MIN  = 180,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_4_MIN  = 240,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_5_MIN  = 300,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_8_MIN  = 480,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_10_MIN = 600,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_12_MIN = 720,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_15_MIN = 900,
  CC_SCREEN_PAGE_BLANK_SCREEN_DELAY_NEVER  = 0,
} CcScreenPageBlankScreenDelay;


G_END_DECLS
