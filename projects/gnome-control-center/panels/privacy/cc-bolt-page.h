/* Copyright © 2018 Red Hat, Inc
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
 * Authors: Christian J. Kellner <ckellner@redhat.com>
 *
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_BOLT_PAGE cc_bolt_page_get_type ()

G_DECLARE_FINAL_TYPE (CcBoltPage, cc_bolt_page, CC, BOLT_PAGE, AdwNavigationPage);

CcBoltPage *cc_bolt_page_new (void);

G_END_DECLS
