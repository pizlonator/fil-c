/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtktypes.h"

G_BEGIN_DECLS

typedef struct _GtkCssKeyframes GtkCssKeyframes;

GtkCssKeyframes *   _gtk_css_keyframes_parse                  (GtkCssParser           *parser);

GtkCssKeyframes *   _gtk_css_keyframes_ref                    (GtkCssKeyframes        *keyframes);
void                _gtk_css_keyframes_unref                  (GtkCssKeyframes        *keyframes);

void                _gtk_css_keyframes_print                  (GtkCssKeyframes        *keyframes,
                                                               GString                *string);

GtkCssKeyframes *   _gtk_css_keyframes_compute                (GtkCssKeyframes         *keyframes,
                                                               GtkStyleProvider        *provider,
                                                               GtkCssStyle             *style,
                                                               GtkCssStyle             *parent_style);

guint               _gtk_css_keyframes_get_n_properties       (GtkCssKeyframes        *keyframes) G_GNUC_PURE;
guint               _gtk_css_keyframes_get_property_id        (GtkCssKeyframes        *keyframes,
                                                               guint                   id);
GtkCssValue *       _gtk_css_keyframes_get_value              (GtkCssKeyframes        *keyframes,
                                                               guint                   id,
                                                               double                  progress,
                                                               GtkCssValue            *default_value);

G_END_DECLS

