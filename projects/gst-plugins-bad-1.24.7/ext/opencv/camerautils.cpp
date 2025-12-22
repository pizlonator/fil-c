/* GStreamer
 * Copyright (C) <2017> Philippe Renon <philippe_renon@yahoo.fr>
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

#include "camerautils.hpp"

#include <opencv2/opencv.hpp>

gchar *
camera_serialize_undistort_settings (cv::Mat & cameraMatrix,
    cv::Mat & distCoeffs)
{
  cv::FileStorage fs (".xml", cv::FileStorage::WRITE + cv::FileStorage::MEMORY);
  fs << "cameraMatrix" << cameraMatrix;
  fs << "distCoeffs" << distCoeffs;
  std::string buf = fs.releaseAndGetString ();

  return g_strdup (buf.c_str ());
}

gboolean
camera_deserialize_undistort_settings (gchar * str, cv::Mat & cameraMatrix,
    cv::Mat & distCoeffs)
{
  cv::FileStorage fs (str, cv::FileStorage::READ + cv::FileStorage::MEMORY);
  fs["cameraMatrix"] >> cameraMatrix;
  fs["distCoeffs"] >> distCoeffs;

  return TRUE;
}
