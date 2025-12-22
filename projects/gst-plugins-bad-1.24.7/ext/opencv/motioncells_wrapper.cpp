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

/* This breaks the build for reasons that aren't entirely clear to me yet */
#if 0
//#ifdef HAVE_CONFIG_H
//#include "config.h"
//#endif
#endif

#include <stdio.h>
#include <limits.h>
#include "motioncells_wrapper.h"

static int instanceCounter = 0;
static bool element_id_was_max = false;

vector < instanceOfMC > motioncellsvector;
vector < int >motioncellsfreeids;

MotionCells *mc;
char p_str[] = "idx failed";

int
motion_cells_init ()
{
  mc = new MotionCells ();
  instanceOfMC tmpmc;
  tmpmc.id = instanceCounter;
  tmpmc.mc = mc;
  motioncellsvector.push_back (tmpmc);
  if ((instanceCounter < INT_MAX) && !element_id_was_max) {
    instanceCounter++;
    element_id_was_max = false;
  } else {
    element_id_was_max = true;
    instanceCounter = motioncellsfreeids.back ();
    motioncellsfreeids.pop_back ();
  }
  return tmpmc.id;
}

int
perform_detection_motion_cells (cv::Mat p_image, double p_sensitivity,
    double p_framerate, int p_gridx, int p_gridy, long int p_timestamp_millisec,
    bool p_isVisible, bool p_useAlpha, int motionmaskcoord_count,
    motionmaskcoordrect * motionmaskcoords, int motionmaskcells_count,
    motioncellidx * motionmaskcellsidx, cellscolor motioncellscolor,
    int motioncells_count, motioncellidx * motioncellsidx, gint64 starttime,
    char *p_datafile, bool p_changed_datafile, int p_thickness, int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->performDetectionMotionCells (p_image,
        p_sensitivity, p_framerate, p_gridx, p_gridy, p_timestamp_millisec,
        p_isVisible, p_useAlpha, motionmaskcoord_count, motionmaskcoords,
        motionmaskcells_count, motionmaskcellsidx, motioncellscolor,
        motioncells_count, motioncellsidx, starttime, p_datafile,
        p_changed_datafile, p_thickness);
  else
    return -1;
}


void
setPrevFrame (cv::Mat p_prevFrame, int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    motioncellsvector.at (idx).mc->setPrevFrame (p_prevFrame);
}

char *
getMotionCellsIdx (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getMotionCellsIdx ();
  else
    return p_str;
}

int
getMotionCellsIdxCnt (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getMotionCellsIdxCount ();
  else
    return 0;
}

bool
getChangedDataFile (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getChangedDataFile ();
  else
    return false;
}

int
searchIdx (int p_id)
{
  for (unsigned int i = 0; i < motioncellsvector.size (); i++) {
    instanceOfMC tmpmc;
    tmpmc = motioncellsvector.at (i);
    if (tmpmc.id == p_id) {
      return i;
    }
  }
  return -1;
}

char *
getInitDataFileFailed (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getDatafileInitFailed ();
  else
    return p_str;
}

char *
getSaveDataFileFailed (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getDatafileSaveFailed ();
  else
    return p_str;
}

int
getInitErrorCode (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getInitErrorCode ();
  else
    return -1;
}

int
getSaveErrorCode (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    return motioncellsvector.at (idx).mc->getSaveErrorCode ();
  else
    return -1;
}

void
motion_cells_free (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1) {
    delete motioncellsvector.at (idx).mc;
    motioncellsvector.erase (motioncellsvector.begin () + idx);
    motioncellsfreeids.push_back (p_id);
  }
}

void
motion_cells_free_resources (int p_id)
{
  int idx = 0;
  idx = searchIdx (p_id);
  if (idx > -1)
    motioncellsvector.at (idx).mc->freeDataFile ();
}
