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

#include "cc-input-source-ibus.h"
#ifdef HAVE_IBUS
#include "cc-ibus-utils.h"
#endif

struct _CcInputSourceIBus
{
  CcInputSource   parent_instance;

  gchar          *engine_name;
#ifdef HAVE_IBUS
  IBusEngineDesc *engine_desc;
#endif
};

G_DEFINE_TYPE (CcInputSourceIBus, cc_input_source_ibus, CC_TYPE_INPUT_SOURCE)

static gchar *
cc_input_source_ibus_get_label (CcInputSource *source)
{
  CcInputSourceIBus *self = CC_INPUT_SOURCE_IBUS (source);
#ifdef HAVE_IBUS
  if (self->engine_desc)
    return g_strdup (engine_get_display_name (self->engine_desc));
  else
#endif
    return g_strdup (self->engine_name);
}

static gboolean
cc_input_source_ibus_matches (CcInputSource *source,
                              CcInputSource *source2)
{
  if (!CC_IS_INPUT_SOURCE_IBUS (source2))
    return FALSE;

  return g_strcmp0 (CC_INPUT_SOURCE_IBUS (source)->engine_name, CC_INPUT_SOURCE_IBUS (source2)->engine_name) == 0;
}

static const gchar *
cc_input_source_ibus_get_layout (CcInputSource *source)
{
#ifdef HAVE_IBUS
  CcInputSourceIBus *self = CC_INPUT_SOURCE_IBUS (source);
  if (self->engine_desc != NULL)
    return ibus_engine_desc_get_layout (self->engine_desc);
  else
#endif
    return NULL;
}

static const gchar *
cc_input_source_ibus_get_layout_variant (CcInputSource *source)
{
#ifdef HAVE_IBUS
  CcInputSourceIBus *self = CC_INPUT_SOURCE_IBUS (source);
  if (self->engine_desc != NULL)
    return ibus_engine_desc_get_layout_variant (self->engine_desc);
  else
#endif
    return NULL;
}

static void
cc_input_source_ibus_dispose (GObject *object)
{
  CcInputSourceIBus *self = CC_INPUT_SOURCE_IBUS (object);

  g_clear_pointer (&self->engine_name, g_free);
#ifdef HAVE_IBUS
  g_clear_object (&self->engine_desc);
#endif

  G_OBJECT_CLASS (cc_input_source_ibus_parent_class)->dispose (object);
}

void
cc_input_source_ibus_class_init (CcInputSourceIBusClass *klass)
{
  CcInputSourceClass *input_source_class = CC_INPUT_SOURCE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  input_source_class->get_label = cc_input_source_ibus_get_label;
  input_source_class->matches = cc_input_source_ibus_matches;
  input_source_class->get_layout = cc_input_source_ibus_get_layout;
  input_source_class->get_layout_variant = cc_input_source_ibus_get_layout_variant;
  object_class->dispose = cc_input_source_ibus_dispose;
}

void
cc_input_source_ibus_init (CcInputSourceIBus *source)
{
}

CcInputSourceIBus *
cc_input_source_ibus_new (const gchar *engine_name)
{
  CcInputSourceIBus *source;

  source = g_object_new (CC_TYPE_INPUT_SOURCE_IBUS, NULL);
  source->engine_name = g_strdup (engine_name);

  return source;
}

#ifdef HAVE_IBUS
void
cc_input_source_ibus_set_engine_desc (CcInputSourceIBus *source,
                                      IBusEngineDesc    *engine_desc)
{
  g_return_if_fail (CC_IS_INPUT_SOURCE_IBUS (source));

  g_clear_object (&source->engine_desc);
  source->engine_desc = g_object_ref (engine_desc);
  cc_input_source_emit_label_changed (CC_INPUT_SOURCE (source));
}
#endif

const gchar *
cc_input_source_ibus_get_engine_name (CcInputSourceIBus *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE_IBUS (source), NULL);
  return source->engine_name;
}

GDesktopAppInfo *
cc_input_source_ibus_get_app_info (CcInputSourceIBus *source)
{
  g_auto(GStrv) tokens = NULL;
  g_autofree gchar *desktop_file_name = NULL;

  g_return_val_if_fail (CC_IS_INPUT_SOURCE_IBUS (source), NULL);

  tokens = g_strsplit (source->engine_name, ":", 2);
  desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", tokens[0]);

  return g_desktop_app_info_new (desktop_file_name);
}
