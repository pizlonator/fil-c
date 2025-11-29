/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsstypesprivate.h"
#include "gtktypes.h"

G_BEGIN_DECLS

/*
 * The idea behind this file is that it provides an on-stack representation
 * for all the CSS boxes one can have to deal with in the CSS box model so that
 * higher level code can use convenient and readable function calls instead of
 * doing complicated math.
 *
 * However, because computing all those rectangles is prohibitively expensive,
 * this struct does it lazily.
 * And then we inline all the code, so that whenever we use this struct, the
 * compiler can optimize out the parts we don't need in that particular use
 * case.
 */

typedef struct _GtkCssBoxes GtkCssBoxes;

/* ahem... 
 * Let's extend GtkCssArea a bit here. */
#define GTK_CSS_AREA_MARGIN_BOX (3)
#define GTK_CSS_AREA_OUTLINE_BOX (4)
#define GTK_CSS_AREA_N_BOXES (5)


struct _GtkCssBoxes
{
  GtkCssStyle *style;
  GskRoundedRect box[GTK_CSS_AREA_N_BOXES];
  gboolean has_rect[GTK_CSS_AREA_N_BOXES]; /* TRUE if we have initialized just the bounds rect */
  gboolean has_box[GTK_CSS_AREA_N_BOXES]; /* TRUE if we have initialized the whole box */
};

static inline void                      gtk_css_boxes_init                      (GtkCssBoxes      *boxes,
                                                                                 GtkWidget        *widget);
static inline void                      gtk_css_boxes_init_content_box          (GtkCssBoxes      *boxes,
                                                                                 GtkCssStyle      *style,
                                                                                 double            x,
                                                                                 double            y,
                                                                                 double            width,
                                                                                 double            height);
static inline void                      gtk_css_boxes_init_border_box           (GtkCssBoxes      *boxes,
                                                                                 GtkCssStyle      *style,
                                                                                 double            x,
                                                                                 double            y,
                                                                                 double            width,
                                                                                 double            height);

static inline const graphene_rect_t *   gtk_css_boxes_get_rect                  (GtkCssBoxes      *boxes,
                                                                                 GtkCssArea        area);
static inline const graphene_rect_t *   gtk_css_boxes_get_margin_rect           (GtkCssBoxes      *boxes);
static inline const graphene_rect_t *   gtk_css_boxes_get_border_rect           (GtkCssBoxes      *boxes);
static inline const graphene_rect_t *   gtk_css_boxes_get_padding_rect          (GtkCssBoxes      *boxes);
static inline const graphene_rect_t *   gtk_css_boxes_get_content_rect          (GtkCssBoxes      *boxes);
static inline const graphene_rect_t *   gtk_css_boxes_get_outline_rect          (GtkCssBoxes      *boxes);

static inline const GskRoundedRect *    gtk_css_boxes_get_box                   (GtkCssBoxes      *boxes,
                                                                                 GtkCssArea        area);
static inline const GskRoundedRect *    gtk_css_boxes_get_border_box            (GtkCssBoxes      *boxes);
static inline const GskRoundedRect *    gtk_css_boxes_get_padding_box           (GtkCssBoxes      *boxes);
static inline const GskRoundedRect *    gtk_css_boxes_get_content_box           (GtkCssBoxes      *boxes);
static inline const GskRoundedRect *    gtk_css_boxes_get_outline_box           (GtkCssBoxes      *boxes);

G_END_DECLS


/* and finally include the actual code for the functions */
#include "gtkcssboxesimplprivate.h"

