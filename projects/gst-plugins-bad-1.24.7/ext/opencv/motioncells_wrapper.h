/*
 * GStreamer
 * Copyright (C) 2011 Robert Jobbagy <jobbagy.robert@gmail.com>
 * Copyright (C) 2011 - 2018 Nicola Murino <nicola.murino@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MOTIONCELLS_WRAPPER_H
#define MOTIONCELLS_WRAPPER_H

#include <stdbool.h>

#ifdef __cplusplus
#include "MotionCells.h"
struct instanceOfMC
{
  int id;
  MotionCells *mc;
};

int searchIdx (int p_id);
extern "C"
{
#endif

  int motion_cells_init ();
  int perform_detection_motion_cells (cv::Mat p_image, double p_sensitivity,
      double p_framerate, int p_gridx, int p_gridy,
      long int p_timestamp_millisec, bool p_isVisible, bool p_useAlpha,
      int motionmaskcoord_count, motionmaskcoordrect * motionmaskcoords,
      int motionmaskcells_count, motioncellidx * motionmaskcellsidx,
      cellscolor motioncellscolor, int motioncells_count,
      motioncellidx * motioncellsidx, gint64 starttime, char *datafile,
      bool p_changed_datafile, int p_thickness, int p_id);
  void setPrevFrame (cv::Mat p_prevFrame, int p_id);
  void motion_cells_free (int p_id);
  void motion_cells_free_resources (int p_id);
  char *getMotionCellsIdx (int p_id);
  int getMotionCellsIdxCnt (int p_id);
  bool getChangedDataFile (int p_id);
  char *getInitDataFileFailed (int p_id);
  char *getSaveDataFileFailed (int p_id);
  int getInitErrorCode (int p_id);
  int getSaveErrorCode (int p_id);

#ifdef __cplusplus
}
#endif

#endif  /* MOTIONCELLS_WRAPPER_H */
