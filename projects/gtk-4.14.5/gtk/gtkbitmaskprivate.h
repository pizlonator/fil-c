/*
 * Copyright © 2011 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <glib.h>
#include "gtkallocatedbitmaskprivate.h"

G_BEGIN_DECLS

static inline GtkBitmask *      _gtk_bitmask_new                  (void);
static inline GtkBitmask *      _gtk_bitmask_copy                 (const GtkBitmask  *mask);
static inline void              _gtk_bitmask_free                 (GtkBitmask        *mask);

static inline char *            _gtk_bitmask_to_string            (const GtkBitmask  *mask);
static inline void              _gtk_bitmask_print                (const GtkBitmask  *mask,
                                                                   GString           *string);

static inline GtkBitmask *      _gtk_bitmask_intersect            (GtkBitmask        *mask,
                                                                   const GtkBitmask  *other) G_GNUC_WARN_UNUSED_RESULT;
static inline GtkBitmask *      _gtk_bitmask_union                (GtkBitmask        *mask,
                                                                   const GtkBitmask  *other) G_GNUC_WARN_UNUSED_RESULT;
static inline GtkBitmask *      _gtk_bitmask_subtract             (GtkBitmask        *mask,
                                                                   const GtkBitmask  *other) G_GNUC_WARN_UNUSED_RESULT;

static inline gboolean          _gtk_bitmask_get                  (const GtkBitmask  *mask,
                                                                   guint              index_);
static inline GtkBitmask *      _gtk_bitmask_set                  (GtkBitmask        *mask,
                                                                   guint              index_,
                                                                   gboolean           value) G_GNUC_WARN_UNUSED_RESULT;

static inline GtkBitmask *      _gtk_bitmask_invert_range         (GtkBitmask        *mask,
                                                                   guint              start,
                                                                   guint              end) G_GNUC_WARN_UNUSED_RESULT;

static inline gboolean          _gtk_bitmask_is_empty             (const GtkBitmask  *mask);
static inline gboolean          _gtk_bitmask_equals               (const GtkBitmask  *mask,
                                                                   const GtkBitmask  *other);
static inline gboolean          _gtk_bitmask_intersects           (const GtkBitmask  *mask,
                                                                   const GtkBitmask  *other);


/* This is the actual implementation of the functions declared above.
 * We put it in a separate file so people don’t get scared from looking at this
 * file when reading source code.
 */
#include "gtkbitmaskprivateimpl.h"


G_END_DECLS

