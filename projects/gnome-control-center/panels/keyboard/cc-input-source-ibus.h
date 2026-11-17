/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <config.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#include <gio/gdesktopappinfo.h>

#include "cc-input-source.h"

G_BEGIN_DECLS

#define CC_TYPE_INPUT_SOURCE_IBUS (cc_input_source_ibus_get_type ())
G_DECLARE_FINAL_TYPE (CcInputSourceIBus, cc_input_source_ibus, CC, INPUT_SOURCE_IBUS, CcInputSource)

CcInputSourceIBus *cc_input_source_ibus_new             (const gchar       *engine_name);

#ifdef HAVE_IBUS
void               cc_input_source_ibus_set_engine_desc (CcInputSourceIBus *source,
                                                         IBusEngineDesc    *engine_desc);
#endif

const gchar       *cc_input_source_ibus_get_engine_name (CcInputSourceIBus *source);

GDesktopAppInfo   *cc_input_source_ibus_get_app_info    (CcInputSourceIBus *source);

G_END_DECLS
