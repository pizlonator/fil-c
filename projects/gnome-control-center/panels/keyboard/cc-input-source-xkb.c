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
#include "cc-input-source-xkb.h"

struct _CcInputSourceXkb
{
  CcInputSource  parent_instance;

  GnomeXkbInfo  *xkb_info;
  gchar         *layout;
  gchar         *variant;
};

G_DEFINE_TYPE (CcInputSourceXkb, cc_input_source_xkb, CC_TYPE_INPUT_SOURCE)

static gchar *
cc_input_source_xkb_get_label (CcInputSource *source)
{
  CcInputSourceXkb *self = CC_INPUT_SOURCE_XKB (source);
  g_autofree gchar *id = NULL;
  const gchar *name;

  id = cc_input_source_xkb_get_id (self);
  gnome_xkb_info_get_layout_info (self->xkb_info, id, &name, NULL, NULL, NULL);
  if (name)
    return g_strdup (name);
  else
    return g_strdup (id);
}

static gboolean
cc_input_source_xkb_matches (CcInputSource *source,
                             CcInputSource *source2)
{
  if (!CC_IS_INPUT_SOURCE_XKB (source2))
    return FALSE;

  return g_strcmp0 (CC_INPUT_SOURCE_XKB (source)->layout, CC_INPUT_SOURCE_XKB (source2)->layout) == 0 &&
         g_strcmp0 (CC_INPUT_SOURCE_XKB (source)->variant, CC_INPUT_SOURCE_XKB (source2)->variant) == 0;
}

static void
cc_input_source_xkb_dispose (GObject *object)
{
  CcInputSourceXkb *self = CC_INPUT_SOURCE_XKB (object);

  g_clear_object (&self->xkb_info);
  g_clear_pointer (&self->layout, g_free);
  g_clear_pointer (&self->variant, g_free);

  G_OBJECT_CLASS (cc_input_source_xkb_parent_class)->dispose (object);
}

static const gchar *
cc_input_source_xkb_get_layout (CcInputSource *source)
{
  return CC_INPUT_SOURCE_XKB (source)->layout;
}

static const gchar *
cc_input_source_xkb_get_layout_variant (CcInputSource *source)
{
  return CC_INPUT_SOURCE_XKB (source)->variant;
}

void
cc_input_source_xkb_class_init (CcInputSourceXkbClass *klass)
{
  CcInputSourceClass *input_source_class = CC_INPUT_SOURCE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  input_source_class->get_label = cc_input_source_xkb_get_label;
  input_source_class->matches = cc_input_source_xkb_matches;
  input_source_class->get_layout = cc_input_source_xkb_get_layout;
  input_source_class->get_layout_variant = cc_input_source_xkb_get_layout_variant;
  object_class->dispose = cc_input_source_xkb_dispose;
}

void
cc_input_source_xkb_init (CcInputSourceXkb *source)
{
}

CcInputSourceXkb *
cc_input_source_xkb_new (GnomeXkbInfo *xkb_info,
                         const gchar  *layout,
                         const gchar  *variant)
{
  CcInputSourceXkb *source;

  source = g_object_new (CC_TYPE_INPUT_SOURCE_XKB, NULL);
  source->xkb_info = g_object_ref (xkb_info);
  source->layout = g_strdup (layout);
  source->variant = g_strdup (variant);

  return source;
}

CcInputSourceXkb *
cc_input_source_xkb_new_from_id (GnomeXkbInfo *xkb_info,
                                 const gchar  *id)
{
  g_auto(GStrv) tokens = NULL;

  tokens = g_strsplit (id, "+", 2);

  return cc_input_source_xkb_new (xkb_info, tokens[0], tokens[1]);
}

gchar *
cc_input_source_xkb_get_id (CcInputSourceXkb *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE_XKB (source), NULL);
  if (source->variant != NULL)
    return g_strdup_printf ("%s+%s", source->layout, source->variant);
  else
    return g_strdup (source->layout);
}
