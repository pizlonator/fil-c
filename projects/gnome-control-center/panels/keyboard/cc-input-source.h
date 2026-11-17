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

#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_INPUT_SOURCE (cc_input_source_get_type ())
G_DECLARE_DERIVABLE_TYPE (CcInputSource, cc_input_source, CC, INPUT_SOURCE, GObject)

struct _CcInputSourceClass
{
  GObjectClass parent_class;

  gchar*       (*get_label)          (CcInputSource *source);
  gboolean     (*matches)            (CcInputSource *source,
                                      CcInputSource *source2);
  const gchar* (*get_layout)         (CcInputSource *source);
  const gchar* (*get_layout_variant) (CcInputSource *source);
};

void             cc_input_source_emit_label_changed (CcInputSource *source);

gchar           *cc_input_source_get_label          (CcInputSource *source);

gboolean         cc_input_source_matches            (CcInputSource *source,
                                                     CcInputSource *source2);

const gchar     *cc_input_source_get_layout         (CcInputSource *source);

const gchar     *cc_input_source_get_layout_variant (CcInputSource *source);

G_END_DECLS
