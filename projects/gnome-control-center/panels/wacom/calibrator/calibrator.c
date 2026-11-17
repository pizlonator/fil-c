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

#include <stdlib.h>

#include "calibrator.h"

#define SWAP(valtype,x,y)		\
    G_STMT_START {			\
    valtype t; t = (x); x = (y); y = t;	\
    } G_STMT_END

/* reset clicks */
void
reset (struct Calib *c)
{
    c->num_clicks = 0;
}

/* check whether the coordinates are along the respective axis */
static gboolean
along_axis (struct Calib *c,
            int           xy,
            int           x0,
            int           y0)
{
    return ((abs(xy - x0) <= c->threshold_misclick) ||
            (abs(xy - y0) <= c->threshold_misclick));
}

/* add a click with the given coordinates */
gboolean
add_click (struct Calib *c,
           int           x,
           int           y)
{
    g_debug ("Trying to add click (%d, %d)", x, y);

    /* Double-click detection */
    if (c->threshold_doubleclick > 0 && c->num_clicks > 0)
    {
        int i = c->num_clicks-1;
        while (i >= 0)
        {
            if (abs(x - c->clicked_x[i]) <= c->threshold_doubleclick &&
                abs(y - c->clicked_y[i]) <= c->threshold_doubleclick)
            {
                g_debug ("Detected double-click, ignoring");
                return FALSE;
            }
            i--;
        }
    }

    /* Mis-click detection */
    if (c->threshold_misclick > 0 && c->num_clicks > 0)
    {
        gboolean misclick = TRUE;

        if (c->num_clicks == 1)
        {
            /* check that along one axis of first point */
            if (along_axis(c, x,c->clicked_x[0],c->clicked_y[0]) ||
                along_axis(c, y,c->clicked_x[0],c->clicked_y[0]))
            {
                misclick = FALSE;
            }
        }
        else if (c->num_clicks == 2)
        {
            /* check that along other axis of first point than second point */
            if ((along_axis(c, y,c->clicked_x[0],c->clicked_y[0]) &&
                 along_axis(c, c->clicked_x[1],c->clicked_x[0],c->clicked_y[0])) ||
                (along_axis(c, x,c->clicked_x[0],c->clicked_y[0]) &&
                 along_axis(c, c->clicked_y[1],c->clicked_x[0],c->clicked_y[0])))
            {
                misclick = FALSE;
            }
        }
        else if (c->num_clicks == 3)
        {
            /* check that along both axis of second and third point */
            if ((along_axis(c, x,c->clicked_x[1],c->clicked_y[1]) &&
                 along_axis(c, y,c->clicked_x[2],c->clicked_y[2])) ||
                (along_axis(c, y,c->clicked_x[1],c->clicked_y[1]) &&
                 along_axis(c, x,c->clicked_x[2],c->clicked_y[2])))
            {
                misclick = FALSE;
            }
        }

        if (misclick)
        {
            g_debug ("Detected misclick, resetting");
            reset(c);
            return FALSE;
        }
    }

    g_debug ("Click (%d, %d) added", x, y);
    c->clicked_x[c->num_clicks] = x;
    c->clicked_y[c->num_clicks] = y;
    c->num_clicks++;

    return TRUE;
}

/* calculate and apply the calibration */
gboolean
finish (struct Calib *c,
        XYinfo       *new_axis,
        gboolean         *swap)
{
    gboolean swap_xy;
    float scale_x;
    float scale_y;
    float delta_x;
    float delta_y;
    XYinfo axis = {-1, -1, -1, -1};

    if (c->num_clicks != 4)
        return FALSE;

    /* Should x and y be swapped? If the device and output are wider
     * towards different axes, swapping must be performed
     *
     * FIXME: Would be even better to know the actual output orientation,
     * not just the direction.
     */
    swap_xy = (c->geometry.width < c->geometry.height);

    /* Compute the scale to transform from pixel positions to [0..1]. */
    scale_x = 1 / (float)c->geometry.width;
    scale_y = 1 / (float)c->geometry.height;

    axis.x_min = ((((c->clicked_x[UL] + c->clicked_x[LL]) / 2)) * scale_x);
    axis.x_max = ((((c->clicked_x[UR] + c->clicked_x[LR]) / 2)) * scale_x);
    axis.y_min = ((((c->clicked_y[UL] + c->clicked_y[UR]) / 2)) * scale_y);
    axis.y_max = ((((c->clicked_y[LL] + c->clicked_y[LR]) / 2)) * scale_y);

    /* Add/subtract the offset that comes from not having the points in the
     * corners (using the same coordinate system they are currently in)
     */
    delta_x = (axis.x_max - axis.x_min) / (float)(NUM_BLOCKS - 2);
    axis.x_min -= delta_x;
    axis.x_max += delta_x;
    delta_y = (axis.y_max - axis.y_min) / (float)(NUM_BLOCKS - 2);
    axis.y_min -= delta_y;
    axis.y_max += delta_y;

    /* If x and y has to be swapped we also have to swap the parameters */
    if (swap_xy)
    {
        SWAP (gdouble, axis.x_min, axis.y_min);
        SWAP (gdouble, axis.x_max, axis.y_max);
    }

    *new_axis = axis;
    *swap = swap_xy;

    return TRUE;
}

