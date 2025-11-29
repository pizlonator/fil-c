/*
 * Copyright © 2022 Benjamin Otte
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

#include "config.h"

#include "gtksectionmodelprivate.h"

#include "gtkmarshalers.h"

/**
 * GtkSectionModel:
 *
 * `GtkSectionModel` is an interface that adds support for sections to list models.
 *
 * A `GtkSectionModel` groups successive items into so-called sections. List widgets
 * like `GtkListView` and `GtkGridView` then allow displaying section headers for
 * these sections by installing a header factory.
 *
 * Many GTK list models support sections inherently, or they pass through the sections
 * of a model they are wrapping.
 *
 * When the section groupings of a model change, the model will emit the
 * [signal@Gtk.SectionModel::sections-changed] signal by calling the
 * [method@Gtk.SectionModel.sections_changed] function. All sections in the given range
 * then need to be queried again.
 * The [signal@Gio.ListModel::items-changed] signal has the same effect, all sections in
 * that range are invalidated, too.
 *
 * Since: 4.12
 */

G_DEFINE_INTERFACE (GtkSectionModel, gtk_section_model, G_TYPE_LIST_MODEL)

enum {
  SECTIONS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_section_model_default_get_section (GtkSectionModel *self,
                                       guint            position,
                                       guint           *out_start,
                                       guint           *out_end)
{
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  if (position >= n_items)
    {
      *out_start = n_items;
      *out_end = G_MAXUINT;
      return;
    }

  *out_start = 0;
  *out_end = n_items;
}

static void
gtk_section_model_default_init (GtkSectionModelInterface *iface)
{
  iface->get_section = gtk_section_model_default_get_section;

  /**
   * GtkSectionModel::sections-changed:
   * @model: a `GtkSectionModel`
   * @position: The first item that may have changed
   * @n_items: number of items with changes
   *
   * Emitted when the start-of-section state of some of the items in @model changes.
   *
   * Note that this signal does not specify the new section state of the
   * items, they need to be queried manually. It is also not necessary for
   * a model to change the section state of any of the items in the section
   * model, though it would be rather useless to emit such a signal.
   *
   * The [signal@Gio.ListModel::items-changed] implies the effect of the
   * [signal@Gtk.SectionModel::sections-changed] signal for all the items
   * it covers.
   *
   * Since: 4.12
   */
  signals[SECTIONS_CHANGED] =
    g_signal_new ("sections-changed",
                  GTK_TYPE_SECTION_MODEL,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[SECTIONS_CHANGED],
                              GTK_TYPE_SECTION_MODEL,
                              _gtk_marshal_VOID__UINT_UINTv);
}

/**
 * gtk_section_model_get_section:
 * @self: a `GtkSectionModel`
 * @position: the position of the item to query
 * @out_start: (out): the position of the first item in the section
 * @out_end: (out): the position of the first item not part of the section
 *   anymore.
 *
 * Query the section that covers the given position. The number of
 * items in the section can be computed by `out_end - out_start`.
 *
 * If the position is larger than the number of items, a single
 * range from n_items to G_MAXUINT will be returned.
 *
 * Since: 4.12
 */
void
gtk_section_model_get_section (GtkSectionModel *self,
                               guint            position,
                               guint           *out_start,
                               guint           *out_end)
{
  GtkSectionModelInterface *iface;

  g_return_if_fail (GTK_IS_SECTION_MODEL (self));
  g_return_if_fail (out_start != NULL);
  g_return_if_fail (out_end != NULL);

  iface = GTK_SECTION_MODEL_GET_IFACE (self);
  iface->get_section (self, position, out_start, out_end);

  g_warn_if_fail (*out_start < *out_end);
}

/* A version of gtk_section_model_get_section() that handles NULL
 * (treats it as the empty list) and any GListModel (treats it as
 * a single section).
 **/
void
gtk_list_model_get_section (GListModel *self,
                            guint       position,
                            guint      *out_start,
                            guint      *out_end)
{
  g_return_if_fail (out_start != NULL);
  g_return_if_fail (out_end != NULL);

  if (self == NULL)
    {
      *out_start = 0;
      *out_end = G_MAXUINT;
      return;
    }

  g_return_if_fail (G_IS_LIST_MODEL (self));

  if (!GTK_IS_SECTION_MODEL (self))
    {
      guint n_items = g_list_model_get_n_items (self);

      if (position < n_items)
        {
          *out_start = 0;
          *out_end = G_MAXUINT;
        }
      else
        {
          *out_start = n_items;
          *out_end = G_MAXUINT;
        }

      return;
    }

  gtk_section_model_get_section (GTK_SECTION_MODEL (self), position, out_start, out_end);
}

/**
 * gtk_section_model_section_changed:
 * @self: a `GtkSectionModel`
 * @position: the first changed item
 * @n_items: the number of changed items
 *
 * This function emits the [signal@Gtk.SectionModel::section-changed]
 * signal to notify about changes to sections.
 *
 * It must cover all positions that used to be a section start or that
 * are now a section start. It does not have to cover all positions for
 * which the section has changed.
 *
 * The [signal@Gio.ListModel::items-changed] implies the effect of the
 * [signal@Gtk.SectionModel::section-changed] signal for all the items
 * it covers.
 *
 * It is recommended that when changes to the items cause section changes
 * in a larger range, that the larger range is included in the emission
 * of the [signal@Gio.ListModel::items-changed] instead of emitting
 * two signals.
 *
 * Since: 4.12
 */
void
gtk_section_model_sections_changed (GtkSectionModel *self,
                                    guint            position,
                                    guint            n_items)
{
  g_return_if_fail (GTK_IS_SECTION_MODEL (self));
  g_return_if_fail (n_items > 0);
  g_return_if_fail (position + n_items <= g_list_model_get_n_items (G_LIST_MODEL (self)));

  g_signal_emit (self, signals[SECTIONS_CHANGED], 0, position, n_items);
}
