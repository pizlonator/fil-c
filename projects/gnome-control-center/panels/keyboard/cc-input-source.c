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

#include <config.h>
#include "cc-input-source.h"

enum
{
  SIGNAL_LABEL_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE (CcInputSource, cc_input_source, G_TYPE_OBJECT)

void
cc_input_source_class_init (CcInputSourceClass *klass)
{
  signals[SIGNAL_LABEL_CHANGED] =
    g_signal_new ("label-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

void
cc_input_source_init (CcInputSource *source)
{
}

void
cc_input_source_emit_label_changed (CcInputSource *source)
{
  g_return_if_fail (CC_IS_INPUT_SOURCE (source));
  g_signal_emit (source, signals[SIGNAL_LABEL_CHANGED], 0);
}

gchar *
cc_input_source_get_label (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_label (source);
}

gboolean
cc_input_source_matches (CcInputSource *source,
                         CcInputSource *source2)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), FALSE);
  return CC_INPUT_SOURCE_GET_CLASS (source)->matches (source, source2);
}

const gchar *
cc_input_source_get_layout (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_layout (source);
}

const gchar *
cc_input_source_get_layout_variant (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_layout_variant (source);
}
