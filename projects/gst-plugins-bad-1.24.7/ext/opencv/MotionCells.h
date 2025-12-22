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

#ifndef MOTIONCELLS_H_
#define MOTIONCELLS_H_

#include <opencv2/core.hpp>
#include <fstream>
#include <vector>
#include <glib.h>

//MotionCells defines
#define MC_HEADER 64
#define MC_TYPE 1
#define MC_VERSION 1
#define MC_VERSIONTEXT "MotionCells-1"
#define MSGLEN 6
#define BUSMSGLEN 20

using namespace std;

struct MotionCellHeader{
	gint32 headersize;
	gint32 type;
	gint32 version;
	gint32 itemsize;
	gint32 gridx;
	gint32 gridy;
	gint64 starttime;
	char name[MC_HEADER - 32];
};

struct MotionCellData{
	gint32 timestamp;
	char *data;
};

typedef struct {
	int upper_left_x;
	int upper_left_y;
	int lower_right_x;
	int lower_right_y;
} motionmaskcoordrect;

typedef struct {
	int R_channel_value;
	int G_channel_value;
	int B_channel_value;
} cellscolor;

typedef struct {
	int lineidx;
	int columnidx;
} motioncellidx;

struct Cell
{
  double MotionArea;
  double CellArea;
  double MotionPercent;
  bool hasMotion;
};

struct MotionCellsIdx
{
  cv::Rect motioncell;
  //Points for the edges of the rectangle.
  cv::Point cell_pt1;
  cv::Point cell_pt2;
  int lineidx;
  int colidx;
};

struct OverlayRegions
{
  cv::Point upperleft;
  cv::Point lowerright;
};

class MotionCells
{
public:

  MotionCells ();
  virtual ~ MotionCells ();

  int performDetectionMotionCells (cv::Mat p_frame, double p_sensitivity,
      double p_framerate, int p_gridx, int p_gridy, gint64 timestamp_millisec,
      bool p_isVisble, bool p_useAlpha, int motionmaskcoord_count,
      motionmaskcoordrect * motionmaskcoords, int motionmaskcells_count,
      motioncellidx * motionmaskcellsidx, cellscolor motioncellscolor,
      int motioncells_count, motioncellidx * motioncellsidx, gint64 starttime,
      char *datafile, bool p_changed_datafile, int p_thickness);

  void setPrevFrame (cv::Mat p_prevframe)
  {
    m_pprevFrame = p_prevframe.clone();
  }
  char *getMotionCellsIdx ()
  {
    return m_motioncellsidxcstr;
  }

  int getMotionCellsIdxCount ()
  {
    return m_motioncells_idx_count;
  }

  bool getChangedDataFile ()
  {
    return m_changed_datafile;
  }

  char *getDatafileInitFailed ()
  {
    return m_initdatafilefailed;
  }

  char *getDatafileSaveFailed ()
  {
    return m_savedatafilefailed;
  }

  int getInitErrorCode ()
  {
    return m_initerrorcode;
  }

  int getSaveErrorCode ()
  {
    return m_saveerrorcode;
  }

  void freeDataFile ()
  {
    if (mc_savefile) {
      fclose (mc_savefile);
      mc_savefile = NULL;
      m_saveInDatafile = false;
    }
  }

private:

  double calculateMotionPercentInCell (int p_row, int p_col, double *p_cellarea,
      double *p_motionarea);
  void performMotionMaskCoords (motionmaskcoordrect * p_motionmaskcoords,
      int p_motionmaskcoords_count);
  void performMotionMask (motioncellidx * p_motionmaskcellsidx,
      int p_motionmaskcells_count);
  void calculateMotionPercentInMotionCells (motioncellidx *
      p_motionmaskcellsidx, int p_motionmaskcells_count = 0);
  int saveMotionCells (gint64 timestamp_millisec);
  int initDataFile (char *p_datafile, gint64 starttime);
  void blendImages (cv::Mat p_actFrame, cv::Mat p_cellsFrame,
      float p_alpha, float p_beta);

  void setData (cv::Mat img, int lin, int col, uchar valor)
  {
    ((uchar *) (img.data + img.step[0] * lin))[col] = valor;
  }

  uchar getData (cv::Mat img, int lin, int col)
  {
    return ((uchar *) (img.data + img.step[0] * lin))[col];
  }

  bool getIsNonZero (cv::Mat img)
  {
    int lin, col;

    for (lin = 0; lin < img.size().height; lin++)
      for (col = 0; col < img.size().width; col++) {
        if ((((uchar *) (img.data + img.step[0] * lin))[col]) > 0)
          return true;
      }
    return false;
  }

  void setMotionCells (int p_frameWidth, int p_frameHeight)
  {
    int i, j;

    m_cellwidth = (double) p_frameWidth / (double) m_gridx;
    m_cellheight = (double) p_frameHeight / (double) m_gridy;
    m_pCells = new Cell *[m_gridy];
    for (i = 0; i < m_gridy; i++)
      m_pCells[i] = new Cell[m_gridx];

    //init cells
    for (i = 0; i < m_gridy; i++)
      for (j = 0; j < m_gridx; j++) {
        m_pCells[i][j].MotionArea = 0;
        m_pCells[i][j].CellArea = 0;
        m_pCells[i][j].MotionPercent = 0;
        m_pCells[i][j].hasMotion = false;
      }
  }

  cv::Mat m_pprevFrame, m_pdifferenceImage, m_pbwImage, m_pcurFrame,
    transparencyimg;
  bool m_isVisible, m_changed_datafile, m_useAlpha, m_saveInDatafile;
  Cell **m_pCells;
  vector < MotionCellsIdx > m_MotionCells;
  vector < OverlayRegions > m_OverlayRegions;
  int m_gridx, m_gridy;
  double m_cellwidth, m_cellheight;
  double m_alpha, m_beta;
  double m_sensitivity;
  int m_framecnt, m_motioncells_idx_count, m_initerrorcode, m_saveerrorcode;
  char *m_motioncellsidxcstr, *m_initdatafilefailed, *m_savedatafilefailed;
  FILE *mc_savefile;
  MotionCellHeader m_header;

};

#endif /* MOTIONCELLS_H_ */
