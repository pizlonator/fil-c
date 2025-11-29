/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtkstyleprovider.h>
#include "gtk/gtkcountingbloomfilterprivate.h"
#include "gtk/gtkcsskeyframesprivate.h"
#include "gtk/gtkcsslookupprivate.h"
#include "gtk/gtkcssnodeprivate.h"
#include "gtk/gtkcssvalueprivate.h"
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_STYLE_PROVIDER_GET_INTERFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), GTK_TYPE_STYLE_PROVIDER, GtkStyleProviderInterface))

typedef struct _GtkStyleProviderInterface GtkStyleProviderInterface;

struct _GtkStyleProviderInterface
{
  GTypeInterface g_iface;

  GtkCssValue *         (* get_color)           (GtkStyleProvider        *provider,
                                                 const char              *name);
  GtkSettings *         (* get_settings)        (GtkStyleProvider        *provider);
  GtkCssKeyframes *     (* get_keyframes)       (GtkStyleProvider        *provider,
                                                 const char              *name);
  int                   (* get_scale)           (GtkStyleProvider        *provider);
  void                  (* lookup)              (GtkStyleProvider        *provider,
                                                 const GtkCountingBloomFilter *filter,
                                                 GtkCssNode              *node,
                                                 GtkCssLookup            *lookup,
                                                 GtkCssChange            *out_change);
  void                  (* emit_error)          (GtkStyleProvider        *provider,
                                                 GtkCssSection           *section,
                                                 const GError            *error);
  /* signal */
  void                  (* changed)             (GtkStyleProvider        *provider);
};

GtkSettings *           gtk_style_provider_get_settings          (GtkStyleProvider        *provider);
GtkCssValue *           gtk_style_provider_get_color             (GtkStyleProvider        *provider,
                                                                  const char              *name);
GtkCssKeyframes *       gtk_style_provider_get_keyframes         (GtkStyleProvider        *provider,
                                                                  const char              *name);
int                     gtk_style_provider_get_scale             (GtkStyleProvider        *provider);
void                    gtk_style_provider_lookup                (GtkStyleProvider        *provider,
                                                                  const GtkCountingBloomFilter *filter,
                                                                  GtkCssNode              *node,
                                                                  GtkCssLookup            *lookup,
                                                                  GtkCssChange            *out_change);

void                    gtk_style_provider_changed               (GtkStyleProvider        *provider);

void                    gtk_style_provider_emit_error            (GtkStyleProvider        *provider,
                                                                  GtkCssSection           *section,
                                                                  GError                  *error);

G_END_DECLS

