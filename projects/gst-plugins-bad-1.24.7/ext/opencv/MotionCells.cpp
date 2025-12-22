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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include "MotionCells.h"
#include <opencv2/imgproc.hpp>

MotionCells::MotionCells ()
{
  m_framecnt = 0;
  m_motioncells_idx_count = 0;
  m_motioncellsidxcstr = NULL;
  m_saveInDatafile = false;
  mc_savefile = NULL;
  m_initdatafilefailed = new char[BUSMSGLEN];
  m_savedatafilefailed = new char[BUSMSGLEN];
  m_initerrorcode = 0;
  m_saveerrorcode = 0;
  m_alpha = 0.5;
  m_beta = 0.5;
  m_useAlpha = false;
  m_isVisible = false;
  m_pCells = NULL;
  m_gridx = 0;
  m_gridy = 0;
  m_cellwidth = 0;
  m_cellheight = 0;
  m_sensitivity = 0;
  m_changed_datafile = false;

  memset (&m_header, 0, sizeof (MotionCellHeader));
  m_header.headersize = GINT32_TO_BE (MC_HEADER);
  m_header.type = GINT32_TO_BE (MC_TYPE);
  m_header.version = GINT32_TO_BE (MC_VERSION);
  m_header.itemsize = 0;
  m_header.gridx = 0;
  m_header.gridy = 0;
  m_header.starttime = 0;
}

MotionCells::~MotionCells ()
{
  if (mc_savefile) {
    fclose (mc_savefile);
    mc_savefile = NULL;
  }
  delete[]m_initdatafilefailed;
  delete[]m_savedatafilefailed;
  if (m_motioncellsidxcstr)
    delete[]m_motioncellsidxcstr;

  m_pcurFrame.release ();
  m_pprevFrame.release ();
  transparencyimg.release ();
  m_pdifferenceImage.release ();
  m_pbwImage.release ();
}

int
MotionCells::performDetectionMotionCells (cv::Mat p_frame,
    double p_sensitivity, double p_framerate, int p_gridx, int p_gridy,
    gint64 timestamp_millisec, bool p_isVisible, bool p_useAlpha,
    int motionmaskcoord_count, motionmaskcoordrect * motionmaskcoords,
    int motionmaskcells_count, motioncellidx * motionmaskcellsidx,
    cellscolor motioncellscolor, int motioncells_count,
    motioncellidx * motioncellsidx, gint64 starttime, char *p_datafile,
    bool p_changed_datafile, int p_thickness)
{

  int sumframecnt = 0;
  int ret = 0;
  cv::Size frameSize;

  p_framerate >= 1 ? p_framerate <= 5 ? sumframecnt = 1
      : p_framerate <= 10 ? sumframecnt = 2
      : p_framerate <= 15 ? sumframecnt = 3
      : p_framerate <= 20 ? sumframecnt = 4
      : p_framerate <= 25 ? sumframecnt = 5
      : p_framerate <= 30 ? sumframecnt = 6 : sumframecnt = 0 : sumframecnt = 0;

  m_framecnt++;
  m_changed_datafile = p_changed_datafile;
  if (m_framecnt >= sumframecnt) {
    m_useAlpha = p_useAlpha;
    m_gridx = p_gridx;
    m_gridy = p_gridy;
    if (m_changed_datafile) {
      ret = initDataFile (p_datafile, starttime);
      if (ret != 0)
        return ret;
    }

    frameSize = p_frame.size ();
    frameSize.width /= 2;
    frameSize.height /= 2;
    setMotionCells (frameSize.width, frameSize.height);
    m_sensitivity = 1 - p_sensitivity;
    m_isVisible = p_isVisible;
    m_pcurFrame = p_frame.clone ();
    cv::Mat m_pcurgreyImage = cv::Mat (frameSize, CV_8UC1);
    cv::Mat m_pprevgreyImage = cv::Mat (frameSize, CV_8UC1);
    cv::Mat m_pgreyImage = cv::Mat (frameSize, CV_8UC1);
    cv::Mat m_pcurDown = cv::Mat (frameSize, m_pcurFrame.type ());
    cv::Mat m_pprevDown = cv::Mat (frameSize, m_pprevFrame.type ());
    m_pbwImage.create (frameSize, CV_8UC1);
    pyrDown (m_pprevFrame, m_pprevDown);
    cvtColor (m_pprevDown, m_pprevgreyImage, cv::COLOR_RGB2GRAY);
    pyrDown (m_pcurFrame, m_pcurDown);
    cvtColor (m_pcurDown, m_pcurgreyImage, cv::COLOR_RGB2GRAY);
    m_pdifferenceImage = m_pcurgreyImage.clone ();
    //cvSmooth(m_pcurgreyImage, m_pcurgreyImage, CV_GAUSSIAN, 3, 0);//TODO camera noise reduce,something smoothing, and rethink runningavg weights

    //Minus the current gray frame from the 8U moving average.
    cv::absdiff (m_pprevgreyImage, m_pcurgreyImage, m_pdifferenceImage);

    //Convert the image to black and white.
    cv::adaptiveThreshold (m_pdifferenceImage, m_pbwImage, 255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 7, 5);

    // Dilate and erode to get object blobs
    cv::dilate (m_pbwImage, m_pbwImage, cv::Mat (), cv::Point (-1, -1), 2);
    cv::erode (m_pbwImage, m_pbwImage, cv::Mat (), cv::Point (-1, -1), 2);

    //mask-out the overlay on difference image
    if (motionmaskcoord_count > 0)
      performMotionMaskCoords (motionmaskcoords, motionmaskcoord_count);
    if (motionmaskcells_count > 0)
      performMotionMask (motionmaskcellsidx, motionmaskcells_count);
    if (getIsNonZero (m_pbwImage)) {    //detect Motion
      if (m_MotionCells.size () > 0)    //it contains previous motioncells what we used when frames dropped
        m_MotionCells.clear ();
      (motioncells_count > 0) ?
          calculateMotionPercentInMotionCells (motioncellsidx,
          motioncells_count)
          : calculateMotionPercentInMotionCells (motionmaskcellsidx, 0);

      transparencyimg = cv::Mat::zeros (p_frame.size (), p_frame.type ());
      if (m_motioncellsidxcstr)
        delete[]m_motioncellsidxcstr;
      m_motioncells_idx_count = m_MotionCells.size () * MSGLEN; //one motion cell idx: (lin idx : col idx,) it's up to 6 character except last motion cell idx
      m_motioncellsidxcstr = new char[m_motioncells_idx_count];
      char *tmpstr = new char[MSGLEN + 1];
      tmpstr[0] = 0;
      for (unsigned int i = 0; i < m_MotionCells.size (); i++) {
        cv::Point pt1, pt2;
        pt1.x = m_MotionCells.at (i).cell_pt1.x * 2;
        pt1.y = m_MotionCells.at (i).cell_pt1.y * 2;
        pt2.x = m_MotionCells.at (i).cell_pt2.x * 2;
        pt2.y = m_MotionCells.at (i).cell_pt2.y * 2;
        if (m_useAlpha && m_isVisible) {
          cv::rectangle (transparencyimg,
              pt1, pt2,
              CV_RGB (motioncellscolor.B_channel_value,
                  motioncellscolor.G_channel_value,
                  motioncellscolor.R_channel_value), cv::FILLED);
        } else if (m_isVisible) {
          cv::rectangle (p_frame,
              pt1, pt2,
              CV_RGB (motioncellscolor.B_channel_value,
                  motioncellscolor.G_channel_value,
                  motioncellscolor.R_channel_value), p_thickness);
        }

        if (i < m_MotionCells.size () - 1) {
          snprintf (tmpstr, MSGLEN + 1, "%d:%d,", m_MotionCells.at (i).lineidx,
              m_MotionCells.at (i).colidx);
        } else {
          snprintf (tmpstr, MSGLEN + 1, "%d:%d", m_MotionCells.at (i).lineidx,
              m_MotionCells.at (i).colidx);
        }
        if (i == 0)
          strncpy (m_motioncellsidxcstr, tmpstr, m_motioncells_idx_count);
        else
          strcat (m_motioncellsidxcstr, tmpstr);
      }
      if (m_MotionCells.size () == 0)
        strncpy (m_motioncellsidxcstr, " ", m_motioncells_idx_count);

      if (m_useAlpha && m_isVisible) {
        if (m_MotionCells.size () > 0)
          blendImages (p_frame, transparencyimg, m_alpha, m_beta);
      }

      delete[]tmpstr;

      if (mc_savefile && m_saveInDatafile) {
        ret = saveMotionCells (timestamp_millisec);
        if (ret != 0)
          return ret;
      }
    } else {
      m_motioncells_idx_count = 0;
      if (m_MotionCells.size () > 0)
        m_MotionCells.clear ();
    }

    m_pprevFrame = m_pcurFrame.clone ();
    m_framecnt = 0;
    if (m_pCells) {
      for (int i = 0; i < m_gridy; ++i) {
        delete[]m_pCells[i];
      }
      delete[]m_pCells;
    }

    if (p_framerate <= 5) {
      if (m_MotionCells.size () > 0)
        m_MotionCells.clear ();
    }
  } else {                      //we do frame drop
    m_motioncells_idx_count = 0;
    ret = -2;
    for (unsigned int i = 0; i < m_MotionCells.size (); i++) {
      cv::Point pt1, pt2;
      pt1.x = m_MotionCells.at (i).cell_pt1.x * 2;
      pt1.y = m_MotionCells.at (i).cell_pt1.y * 2;
      pt2.x = m_MotionCells.at (i).cell_pt2.x * 2;
      pt2.y = m_MotionCells.at (i).cell_pt2.y * 2;
      if (m_useAlpha && m_isVisible) {
        cv::rectangle (transparencyimg,
            pt1,
            pt2,
            CV_RGB (motioncellscolor.B_channel_value,
                motioncellscolor.G_channel_value,
                motioncellscolor.R_channel_value), cv::FILLED);
      } else if (m_isVisible) {
        cv::rectangle (p_frame,
            pt1,
            pt2,
            CV_RGB (motioncellscolor.B_channel_value,
                motioncellscolor.G_channel_value,
                motioncellscolor.R_channel_value), p_thickness);
      }

    }
    if (m_useAlpha && m_isVisible) {
      if (m_MotionCells.size () > 0)
        blendImages (p_frame, transparencyimg, m_alpha, m_beta);
    }
  }
  return ret;
}

int
MotionCells::initDataFile (char *p_datafile, gint64 starttime)  //p_date is increased with difference between current and previous buffer ts
{
  MotionCellData mcd;
  if (strncmp (p_datafile, " ", 1)) {
    mc_savefile = fopen (p_datafile, "w");
    if (mc_savefile == NULL) {
      //fprintf(stderr, "%s %d:initDataFile:fopen:%d (%s)\n", __FILE__, __LINE__, errno,
      //strerror(errno));
      strncpy (m_initdatafilefailed, strerror (errno), BUSMSGLEN - 1);
      m_initerrorcode = errno;
      return 1;
    } else {
      m_saveInDatafile = true;
    }
  } else
    mc_savefile = NULL;

  //it needs these bytes
  m_header.itemsize =
      GINT32_TO_BE ((int) ceil (ceil (m_gridx * m_gridy / 8.0) / 4.0) * 4 +
      sizeof (mcd.timestamp));
  m_header.gridx = GINT32_TO_BE (m_gridx);
  m_header.gridy = GINT32_TO_BE (m_gridy);
  m_header.starttime = GINT64_TO_BE (starttime);

  snprintf (m_header.name, sizeof (m_header.name), "%s %dx%d", MC_VERSIONTEXT,
      GINT32_FROM_BE (m_header.gridx), GINT32_FROM_BE (m_header.gridy));
  m_changed_datafile = false;
  return 0;
}

int
MotionCells::saveMotionCells (gint64 timestamp_millisec)
{

  MotionCellData mc_data;
  mc_data.timestamp = GINT32_TO_BE (timestamp_millisec);
  mc_data.data = NULL;
  //There is no datafile
  if (mc_savefile == NULL)
    return 0;

  if (ftello (mc_savefile) == 0) {
    //cerr << "Writing out file header"<< m_header.headersize <<":" << sizeof(MotionCellHeader) << " itemsize:"
    //<< m_header.itemsize << endl;
    if (fwrite (&m_header, sizeof (MotionCellHeader), 1, mc_savefile) != 1) {
      //fprintf(stderr, "%s %d:saveMotionCells:fwrite:%d (%s)\n", __FILE__, __LINE__, errno,
      //strerror(errno));
      strncpy (m_savedatafilefailed, strerror (errno), BUSMSGLEN - 1);
      m_saveerrorcode = errno;
      return -1;
    }
  }

  mc_data.data =
      (char *) calloc (1,
      GINT32_FROM_BE (m_header.itemsize) - sizeof (mc_data.timestamp));
  if (mc_data.data == NULL) {
    //fprintf(stderr, "%s %d:saveMotionCells:calloc:%d (%s)\n", __FILE__, __LINE__, errno,
    //strerror(errno));
    strncpy (m_savedatafilefailed, strerror (errno), BUSMSGLEN - 1);
    m_saveerrorcode = errno;
    return -1;
  }

  for (unsigned int i = 0; i < m_MotionCells.size (); i++) {
    int bitnum =
        m_MotionCells.at (i).lineidx * GINT32_FROM_BE (m_header.gridx) +
        m_MotionCells.at (i).colidx;
    int bytenum = (int) floor (bitnum / 8.0);
    int shift = bitnum - bytenum * 8;
    mc_data.data[bytenum] = mc_data.data[bytenum] | (1 << shift);
    //cerr << "Motion Detected " <<  "line:" << m_MotionCells.at(i).lineidx << " col:" << m_MotionCells.at(i).colidx;
    //cerr << "    bitnum " << bitnum << " bytenum " << bytenum << " shift " << shift << " value " << (int)mc_data.data[bytenum] << endl;
  }

  if (fwrite (&mc_data.timestamp, sizeof (mc_data.timestamp), 1,
          mc_savefile) != 1) {
    //fprintf(stderr, "%s %d:saveMotionCells:fwrite:%d (%s)\n", __FILE__, __LINE__, errno,
    //strerror(errno));
    strncpy (m_savedatafilefailed, strerror (errno), BUSMSGLEN - 1);
    m_saveerrorcode = errno;
    return -1;
  }

  if (fwrite (mc_data.data,
          GINT32_FROM_BE (m_header.itemsize) - sizeof (mc_data.timestamp), 1,
          mc_savefile) != 1) {
    //fprintf(stderr, "%s %d:saveMotionCells:fwrite:%d (%s)\n", __FILE__, __LINE__, errno,
    //strerror(errno));
    strncpy (m_savedatafilefailed, strerror (errno), BUSMSGLEN - 1);
    m_saveerrorcode = errno;
    return -1;
  }

  free (mc_data.data);
  return 0;
}

double
MotionCells::calculateMotionPercentInCell (int p_row, int p_col,
    double *p_cellarea, double *p_motionarea)
{
  double cntpixelsnum = 0;
  double cntmotionpixelnum = 0;

  int ybegin = floor ((double) p_row * m_cellheight);
  int yend = floor ((double) (p_row + 1) * m_cellheight);
  int xbegin = floor ((double) (p_col) * m_cellwidth);
  int xend = floor ((double) (p_col + 1) * m_cellwidth);
  int cellw = xend - xbegin;
  int cellh = yend - ybegin;
  int cellarea = cellw * cellh;
  *p_cellarea = cellarea;
  int thresholdmotionpixelnum = floor ((double) cellarea * m_sensitivity);

  for (int i = ybegin; i < yend; i++) {
    for (int j = xbegin; j < xend; j++) {
      cntpixelsnum++;
      if ((((uchar *) (m_pbwImage.data + m_pbwImage.step[0] * i))[j]) > 0) {
        cntmotionpixelnum++;
        if (cntmotionpixelnum >= thresholdmotionpixelnum) {     //we don't need to calculate anymore
          *p_motionarea = cntmotionpixelnum;
          return (cntmotionpixelnum / cntpixelsnum);
        }
      }
      int remainingpixelsnum = cellarea - cntpixelsnum;
      if ((cntmotionpixelnum + remainingpixelsnum) < thresholdmotionpixelnum) { //moving pixels number will be less than threshold
        *p_motionarea = 0;
        return 0;
      }
    }
  }

  return (cntmotionpixelnum / cntpixelsnum);
}

void
MotionCells::calculateMotionPercentInMotionCells (motioncellidx *
    p_motioncellsidx, int p_motioncells_count)
{
  if (p_motioncells_count == 0) {
    for (int i = 0; i < m_gridy; i++) {
      for (int j = 0; j < m_gridx; j++) {
        m_pCells[i][j].MotionPercent = calculateMotionPercentInCell (i, j,
            &m_pCells[i][j].CellArea, &m_pCells[i][j].MotionArea);
        m_pCells[i][j].hasMotion =
            m_sensitivity < m_pCells[i][j].MotionPercent ? true : false;
        if (m_pCells[i][j].hasMotion) {
          MotionCellsIdx mci;
          mci.lineidx = i;
          mci.colidx = j;
          mci.cell_pt1.x = floor ((double) j * m_cellwidth);
          mci.cell_pt1.y = floor ((double) i * m_cellheight);
          mci.cell_pt2.x = floor ((double) (j + 1) * m_cellwidth);
          mci.cell_pt2.y = floor ((double) (i + 1) * m_cellheight);
          int w = mci.cell_pt2.x - mci.cell_pt1.x;
          int h = mci.cell_pt2.y - mci.cell_pt1.y;
          mci.motioncell = cv::Rect (mci.cell_pt1.x, mci.cell_pt1.y, w, h);
          m_MotionCells.push_back (mci);
        }
      }
    }
  } else {
    for (int k = 0; k < p_motioncells_count; ++k) {

      int i = p_motioncellsidx[k].lineidx;
      int j = p_motioncellsidx[k].columnidx;
      m_pCells[i][j].MotionPercent =
          calculateMotionPercentInCell (i, j,
          &m_pCells[i][j].CellArea, &m_pCells[i][j].MotionArea);
      m_pCells[i][j].hasMotion =
          m_pCells[i][j].MotionPercent > m_sensitivity ? true : false;
      if (m_pCells[i][j].hasMotion) {
        MotionCellsIdx mci;
        mci.lineidx = p_motioncellsidx[k].lineidx;
        mci.colidx = p_motioncellsidx[k].columnidx;
        mci.cell_pt1.x = floor ((double) j * m_cellwidth);
        mci.cell_pt1.y = floor ((double) i * m_cellheight);
        mci.cell_pt2.x = floor ((double) (j + 1) * m_cellwidth);
        mci.cell_pt2.y = floor ((double) (i + 1) * m_cellheight);
        int w = mci.cell_pt2.x - mci.cell_pt1.x;
        int h = mci.cell_pt2.y - mci.cell_pt1.y;
        mci.motioncell = cv::Rect (mci.cell_pt1.x, mci.cell_pt1.y, w, h);
        m_MotionCells.push_back (mci);
      }
    }
  }
}

void
MotionCells::performMotionMaskCoords (motionmaskcoordrect * p_motionmaskcoords,
    int p_motionmaskcoords_count)
{
  cv::Point upperleft;
  upperleft.x = 0;
  upperleft.y = 0;
  cv::Point lowerright;
  lowerright.x = 0;
  lowerright.y = 0;
  for (int i = 0; i < p_motionmaskcoords_count; i++) {
    upperleft.x = p_motionmaskcoords[i].upper_left_x;
    upperleft.y = p_motionmaskcoords[i].upper_left_y;
    lowerright.x = p_motionmaskcoords[i].lower_right_x;
    lowerright.y = p_motionmaskcoords[i].lower_right_y;
    cv::rectangle (m_pbwImage, upperleft, lowerright, CV_RGB (0, 0, 0),
        cv::FILLED);
  }
}

void
MotionCells::performMotionMask (motioncellidx * p_motionmaskcellsidx,
    int p_motionmaskcells_count)
{
  for (int k = 0; k < p_motionmaskcells_count; k++) {
    int beginy = p_motionmaskcellsidx[k].lineidx * m_cellheight;
    int beginx = p_motionmaskcellsidx[k].columnidx * m_cellwidth;
    int endx =
        (double) p_motionmaskcellsidx[k].columnidx * m_cellwidth + m_cellwidth;
    int endy =
        (double) p_motionmaskcellsidx[k].lineidx * m_cellheight + m_cellheight;
    for (int i = beginy; i < endy; i++)
      for (int j = beginx; j < endx; j++) {
        ((uchar *) (m_pbwImage.data + m_pbwImage.step[0] * i))[j] = 0;
      }
  }
}

///BGR if we use only OpenCV
//RGB if we use gst+OpenCV
void
MotionCells::blendImages (cv::Mat p_actFrame, cv::Mat p_cellsFrame,
    float p_alpha, float p_beta)
{

  int height = p_actFrame.size ().height;
  int width = p_actFrame.size ().width;
  int step = p_actFrame.step[0] / sizeof (uchar);
  int channels = p_actFrame.channels ();
  int cellstep = p_cellsFrame.step[0] / sizeof (uchar);
  uchar *curImageData = (uchar *) p_actFrame.data;
  uchar *cellImageData = (uchar *) p_cellsFrame.data;

  for (int i = 0; i < height; i++)
    for (int j = 0; j < width; j++)
      for (int k = 0; k < channels; k++)
        if (cellImageData[i * cellstep + j * channels + k] > 0) {
          curImageData[i * step + j * channels + k] =
              round ((double) curImageData[i * step + j * channels +
                  k] * p_alpha + ((double) cellImageData[i * cellstep +
                      j * channels + k] * p_beta));
        }
}
