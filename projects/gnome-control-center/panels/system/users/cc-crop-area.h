/*
 * Copyright © 2009 Bastien Nocera <hadess@hadess.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CC_CROP_AREA_H_
#define _CC_CROP_AREA_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_CROP_AREA (cc_crop_area_get_type ())
G_DECLARE_FINAL_TYPE (CcCropArea, cc_crop_area, CC, CROP_AREA, GtkWidget)

GtkWidget *      cc_crop_area_new                  (void);
GdkPaintable *   cc_crop_area_get_paintable        (CcCropArea   *area);
void             cc_crop_area_set_paintable        (CcCropArea   *area,
                                                    GdkPaintable *paintable);
void             cc_crop_area_set_min_size         (CcCropArea   *area,
                                                    int           width,
                                                    int           height);
GdkPixbuf *      cc_crop_area_create_pixbuf        (CcCropArea   *area);

G_END_DECLS

#endif /* _CC_CROP_AREA_H_ */
