/*
 * Copyright (c) 2009 Tias Guns
 * Copyright (c) 2009 Soren Hauberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#include "gsd-device-manager.h"

/* struct to hold min/max info of the X and Y axis */
typedef struct
{
	gdouble x_min;
	gdouble x_max;
	gdouble y_min;
	gdouble y_max;
} XYinfo;

#define CC_TYPE_CALIB_AREA cc_calib_area_get_type ()
G_DECLARE_FINAL_TYPE (CcCalibArea, cc_calib_area, CC, CALIB_AREA, GtkWindow)

typedef void (*FinishCallback) (CcCalibArea *area, gpointer user_data);

CcCalibArea * cc_calib_area_new (GdkDisplay      *display,
                                 GdkMonitor     *monitor,
                                 GsdDevice      *device,
                                 FinishCallback  callback,
                                 gpointer        user_data,
                                 int             threshold_doubleclick,
                                 int             threshold_misclick);

gboolean cc_calib_area_finish (CcCalibArea *area);

void cc_calib_area_free (CcCalibArea *area);

void cc_calib_area_get_axis (CcCalibArea *area,
                             XYinfo      *new_axis,
                             gboolean    *swap_xy);

void cc_calib_area_get_padding (CcCalibArea *area,
                                XYinfo      *padding);

G_END_DECLS
