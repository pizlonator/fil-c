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

#include <glib-object.h>

#include "gtkcsstypesprivate.h"
#include "gtkstyleprovider.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_VALUE           (_gtk_css_value_get_type ())

/* A GtkCssValue is a refcounted immutable value type */

typedef struct _GtkCssValue           GtkCssValue;
typedef struct _GtkCssValueClass      GtkCssValueClass;

/* using define instead of struct here so compilers get the packing right */
#define GTK_CSS_VALUE_BASE \
  const GtkCssValueClass *class; \
  int ref_count; \
  guint is_computed: 1;

struct _GtkCssValueClass {
  const char *type_name;
  void          (* free)                              (GtkCssValue                *value);

  GtkCssValue * (* compute)                           (GtkCssValue                *value,
                                                       guint                       property_id,
                                                       GtkStyleProvider           *provider,
                                                       GtkCssStyle                *style,
                                                       GtkCssStyle                *parent_style);
  gboolean      (* equal)                             (const GtkCssValue          *value1,
                                                       const GtkCssValue          *value2);
  GtkCssValue * (* transition)                        (GtkCssValue                *start,
                                                       GtkCssValue                *end,
                                                       guint                       property_id,
                                                       double                      progress);
  gboolean      (* is_dynamic)                        (const GtkCssValue          *value);
  GtkCssValue * (* get_dynamic_value)                 (GtkCssValue                *value,
                                                       gint64                      monotonic_time);
  void          (* print)                             (const GtkCssValue          *value,
                                                       GString                    *string);
};

GType        _gtk_css_value_get_type                  (void) G_GNUC_CONST;

GtkCssValue *_gtk_css_value_alloc                     (const GtkCssValueClass     *klass,
                                                       gsize                       size);
#define _gtk_css_value_new(_name, _klass) ((_name *) _gtk_css_value_alloc ((_klass), sizeof (_name)))

#define _gtk_css_value_ref gtk_css_value_ref
GtkCssValue *   gtk_css_value_ref                     (GtkCssValue                *value);
#define _gtk_css_value_unref gtk_css_value_unref
void            gtk_css_value_unref                   (GtkCssValue                *value);

GtkCssValue *_gtk_css_value_compute                   (GtkCssValue                *value,
                                                       guint                       property_id,
                                                       GtkStyleProvider           *provider,
                                                       GtkCssStyle                *style,
                                                       GtkCssStyle                *parent_style) G_GNUC_PURE;
gboolean     _gtk_css_value_equal                     (const GtkCssValue          *value1,
                                                       const GtkCssValue          *value2) G_GNUC_PURE;
gboolean     _gtk_css_value_equal0                    (const GtkCssValue          *value1,
                                                       const GtkCssValue          *value2) G_GNUC_PURE;
GtkCssValue *_gtk_css_value_transition                (GtkCssValue                *start,
                                                       GtkCssValue                *end,
                                                       guint                       property_id,
                                                       double                      progress);
gboolean        gtk_css_value_is_dynamic              (const GtkCssValue          *value) G_GNUC_PURE;
GtkCssValue *   gtk_css_value_get_dynamic_value       (GtkCssValue                *value,
                                                       gint64                      monotonic_time);

char *       _gtk_css_value_to_string                 (const GtkCssValue          *value);
void         _gtk_css_value_print                     (const GtkCssValue          *value,
                                                       GString                    *string);
gboolean     gtk_css_value_is_computed                (const GtkCssValue          *value) G_GNUC_PURE;

G_END_DECLS

