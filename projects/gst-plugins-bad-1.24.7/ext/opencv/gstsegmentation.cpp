/*
 * GStreamer
 * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
 * Except: Parts of code inside the preprocessor define CODE_FROM_OREILLY_BOOK,
 *  which are downloaded from O'Reilly website
 *  [http://examples.oreilly.com/9780596516130/]
 *  and adapted. Its license reads:
 *  "Oct. 3, 2008
 *   Right to use this code in any way you want without warrenty, support or
 *   any guarantee of it working. "
 *
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
#define CODE_FROM_OREILLY_BOOK

/**
 * SECTION:element-segmentation
 *
 * This element creates and updates a fg/bg model using one of several approaches.
 * The one called "codebook" refers to the codebook approach following the opencv
 * O'Reilly book [1] implementation of the algorithm described in K. Kim,
 * T. H. Chalidabhongse, D. Harwood and L. Davis [2]. BackgroundSubtractorMOG [3],
 * or MOG for shorts, refers to a Gaussian Mixture-based Background/Foreground
 * Segmentation Algorithm. OpenCV MOG implements the algorithm described in [4].
 * BackgroundSubtractorMOG2 [5], refers to another Gaussian Mixture-based
 * Background/Foreground segmentation algorithm. OpenCV MOG2 implements the
 * algorithm described in [6] and [7].
 *
 * [1] Learning OpenCV: Computer Vision with the OpenCV Library by Gary Bradski
 * and Adrian Kaehler, Published by O'Reilly Media, October 3, 2008
 * [2] "Real-time Foreground-Background Segmentation using Codebook Model",
 * Real-time Imaging, Volume 11, Issue 3, Pages 167-256, June 2005.
 * [3] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog
 * [4] P. KadewTraKuPong and R. Bowden, "An improved adaptive background
 * mixture model for real-time tracking with shadow detection", Proc. 2nd
 * European Workshop on Advanced Video-Based Surveillance Systems, 2001
 * [5] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog2
 * [6] Z.Zivkovic, "Improved adaptive Gaussian mixture model for background
 * subtraction", International Conference Pattern Recognition, UK, August, 2004.
 * [7] Z.Zivkovic, F. van der Heijden, "Efficient Adaptive Density Estimation
 * per Image Pixel for the Task of Background Subtraction", Pattern Recognition
 * Letters, vol. 27, no. 7, pages 773-780, 2006.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0  v4l2src device=/dev/video0 ! videoconvert ! segmentation test-mode=true method=2 ! videoconvert ! ximagesink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstsegmentation.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_segmentation_debug);
#define GST_CAT_DEFAULT gst_segmentation_debug

using namespace cv;

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_TEST_MODE,
  PROP_METHOD,
  PROP_LEARNING_RATE
};
typedef enum
{
  METHOD_BOOK,
  METHOD_MOG,
  METHOD_MOG2
} GstSegmentationMethod;

#define DEFAULT_TEST_MODE FALSE
#define DEFAULT_METHOD  METHOD_MOG2
#define DEFAULT_LEARNING_RATE  0.01

#define GST_TYPE_SEGMENTATION_METHOD (gst_segmentation_method_get_type ())
static GType
gst_segmentation_method_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      {METHOD_BOOK, "Codebook-based segmentation (Bradski2008)", "codebook"},
      {METHOD_MOG, "Mixture-of-Gaussians segmentation (Bowden2001)", "mog"},
      {METHOD_MOG2, "Mixture-of-Gaussians segmentation (Zivkovic2004)", "mog2"},
      {0, NULL, NULL},
    };
    etype = g_enum_register_static ("GstSegmentationMethod", values);
  }
  return etype;
}

G_DEFINE_TYPE_WITH_CODE (GstSegmentation, gst_segmentation,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_segmentation_debug, "segmentation", 0,
        "Performs Foreground/Background segmentation in video sequences");
    );
GST_ELEMENT_REGISTER_DEFINE (segmentation, "segmentation", GST_RANK_NONE,
    GST_TYPE_SEGMENTATION);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA")));


static void
gst_segmentation_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_segmentation_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_segmentation_transform_ip (GstOpencvVideoFilter *
    filter, GstBuffer * buffer, Mat img);

static void gst_segmentation_finalize (GObject * object);
static gboolean gst_segmentation_set_caps (GstOpencvVideoFilter * filter,
    gint in_width, gint in_height, int in_cv_type, gint out_width,
    gint out_height, int out_cv_type);

/* Codebook algorithm + connected components functions*/
static int update_codebook (unsigned char *p, codeBook * c,
    unsigned *cbBounds, int numChannels);
static int clear_stale_entries (codeBook * c);
static unsigned char background_diff (unsigned char *p, codeBook * c,
    int numChannels, int *minMod, int *maxMod);
static void find_connected_components (Mat mask, int poly1_hull0,
    float perimScale);

/* MOG (Mixture-of-Gaussians functions */
static int run_mog_iteration (GstSegmentation * filter);
static int run_mog2_iteration (GstSegmentation * filter);

/* initialize the segmentation's class */
static void
gst_segmentation_class_init (GstSegmentationClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOpencvVideoFilterClass *cvfilter_class =
      (GstOpencvVideoFilterClass *) klass;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_segmentation_finalize;
  gobject_class->set_property = gst_segmentation_set_property;
  gobject_class->get_property = gst_segmentation_get_property;


  cvfilter_class->cv_trans_ip_func = gst_segmentation_transform_ip;
  cvfilter_class->cv_set_caps = gst_segmentation_set_caps;

  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method",
          "Segmentation method to use",
          "Segmentation method to use",
          GST_TYPE_SEGMENTATION_METHOD, DEFAULT_METHOD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TEST_MODE,
      g_param_spec_boolean ("test-mode", "test-mode",
          "If true, the output RGB is overwritten with the calculated foreground (white color)",
          DEFAULT_TEST_MODE, (GParamFlags)
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_LEARNING_RATE,
      g_param_spec_float ("learning-rate", "learning-rate",
          "Speed with which a motionless foreground pixel would become background (inverse of number of frames)",
          0, 1, DEFAULT_LEARNING_RATE, (GParamFlags) (G_PARAM_READWRITE)));

  gst_element_class_set_static_metadata (element_class,
      "Foreground/background video sequence segmentation",
      "Filter/Effect/Video",
      "Create a Foregound/Background mask applying a particular algorithm",
      "Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_SEGMENTATION_METHOD, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_segmentation_init (GstSegmentation * filter)
{
  filter->method = DEFAULT_METHOD;
  filter->test_mode = DEFAULT_TEST_MODE;
  filter->framecount = 0;
  filter->learning_rate = DEFAULT_LEARNING_RATE;
  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER (filter), TRUE);
}

static void
gst_segmentation_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSegmentation *filter = GST_SEGMENTATION (object);

  switch (prop_id) {
    case PROP_METHOD:
      filter->method = g_value_get_enum (value);
      break;
    case PROP_TEST_MODE:
      filter->test_mode = g_value_get_boolean (value);
      break;
    case PROP_LEARNING_RATE:
      filter->learning_rate = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_segmentation_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSegmentation *filter = GST_SEGMENTATION (object);

  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, filter->method);
      break;
    case PROP_TEST_MODE:
      g_value_set_boolean (value, filter->test_mode);
      break;
    case PROP_LEARNING_RATE:
      g_value_set_float (value, filter->learning_rate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_segmentation_set_caps (GstOpencvVideoFilter * filter, gint in_width,
    gint in_height, int in_cv_type,
    gint out_width, gint out_height, int out_cv_type)
{
  GstSegmentation *segmentation = GST_SEGMENTATION (filter);
  Size size;

  size = Size (in_width, in_height);
  segmentation->width = in_width;
  segmentation->height = in_height;

  segmentation->cvRGB.create (size, CV_8UC3);
  segmentation->cvYUV.create (size, CV_8UC3);

  segmentation->cvFG = Mat::zeros (size, CV_8UC1);

  segmentation->ch1.create (size, CV_8UC1);
  segmentation->ch2.create (size, CV_8UC1);
  segmentation->ch3.create (size, CV_8UC1);

  /* Codebook method */
  segmentation->TcodeBook = (codeBook *)
      g_malloc (sizeof (codeBook) *
      (segmentation->width * segmentation->height + 1));
  for (int j = 0; j < segmentation->width * segmentation->height; j++) {
    segmentation->TcodeBook[j].numEntries = 0;
    segmentation->TcodeBook[j].t = 0;
  }
  segmentation->learning_interval = (int) (1.0 / segmentation->learning_rate);

  /* Mixture-of-Gaussians (mog) methods */
  segmentation->mog = bgsegm::createBackgroundSubtractorMOG ();
  segmentation->mog2 = createBackgroundSubtractorMOG2 ();

  return TRUE;
}

/* Clean up */
static void
gst_segmentation_finalize (GObject * object)
{
  GstSegmentation *filter = GST_SEGMENTATION (object);

  filter->cvRGB.release ();
  filter->cvYUV.release ();
  filter->cvFG.release ();
  filter->ch1.release ();
  filter->ch2.release ();
  filter->ch3.release ();
  filter->mog.release ();
  filter->mog2.release ();
  g_free (filter->TcodeBook);

  G_OBJECT_CLASS (gst_segmentation_parent_class)->finalize (object);
}

static GstFlowReturn
gst_segmentation_transform_ip (GstOpencvVideoFilter * cvfilter,
    GstBuffer * buffer, Mat img)
{
  GstSegmentation *filter = GST_SEGMENTATION (cvfilter);
  int j;

  filter->framecount++;

  /*  Image preprocessing: color space conversion etc */
  cvtColor (img, filter->cvRGB, COLOR_RGBA2RGB);
  cvtColor (filter->cvRGB, filter->cvYUV, COLOR_RGB2YCrCb);

  /* Create and update a fg/bg model using a codebook approach following the
   * opencv O'Reilly book [1] implementation of the algo described in [2].
   *
   * [1] Learning OpenCV: Computer Vision with the OpenCV Library by Gary
   * Bradski and Adrian Kaehler, Published by O'Reilly Media, October 3, 2008
   * [2] "Real-time Foreground-Background Segmentation using Codebook Model",
   * Real-time Imaging, Volume 11, Issue 3, Pages 167-256, June 2005. */
  if (METHOD_BOOK == filter->method) {
    unsigned cbBounds[3] = { 10, 5, 5 };
    int minMod[3] = { 20, 20, 20 }, maxMod[3] = {
      20, 20, 20
    };

    if (filter->framecount < 30) {
      /* Learning background phase: update_codebook on every frame */
      for (j = 0; j < filter->width * filter->height; j++) {
        update_codebook (filter->cvYUV.data + j * 3,
            (codeBook *) & (filter->TcodeBook[j]), cbBounds, 3);
      }
    } else {
      /*  this updating is responsible for FG becoming BG again */
      if (filter->framecount % filter->learning_interval == 0) {
        for (j = 0; j < filter->width * filter->height; j++) {
          update_codebook (filter->cvYUV.data + j * 3,
              (codeBook *) & (filter->TcodeBook[j]), cbBounds, 3);
        }
      }
      if (filter->framecount % 60 == 0) {
        for (j = 0; j < filter->width * filter->height; j++)
          clear_stale_entries ((codeBook *) & (filter->TcodeBook[j]));
      }

      for (j = 0; j < filter->width * filter->height; j++) {
        if (background_diff
            (filter->cvYUV.data + j * 3,
                (codeBook *) & (filter->TcodeBook[j]), 3, minMod, maxMod)) {
          filter->cvFG.data[j] = (char) 255;
        } else {
          filter->cvFG.data[j] = 0;
        }
      }
    }

    /* 3rd param is the smallest area to show: (w+h)/param , in pixels */
    find_connected_components (filter->cvFG, 1, 10000);

  }
  /* Create the foreground and background masks using BackgroundSubtractorMOG [1],
   *  Gaussian Mixture-based Background/Foreground segmentation algorithm. OpenCV
   * MOG implements the algorithm described in [2].
   *
   * [1] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog
   * [2] P. KadewTraKuPong and R. Bowden, "An improved adaptive background
   * mixture model for real-time tracking with shadow detection", Proc. 2nd
   * European Workshop on Advanced Video-Based Surveillance Systems, 2001
   */
  else if (METHOD_MOG == filter->method) {
    run_mog_iteration (filter);
  }
  /* Create the foreground and background masks using BackgroundSubtractorMOG2
   * [1], Gaussian Mixture-based Background/Foreground segmentation algorithm.
   * OpenCV MOG2 implements the algorithm described in [2] and [3].
   *
   * [1] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog2
   * [2] Z.Zivkovic, "Improved adaptive Gaussian mixture model for background
   * subtraction", International Conference Pattern Recognition, UK, Aug 2004.
   * [3] Z.Zivkovic, F. van der Heijden, "Efficient Adaptive Density Estimation
   * per Image Pixel for the Task of Background Subtraction", Pattern
   * Recognition Letters, vol. 27, no. 7, pages 773-780, 2006.   */
  else if (METHOD_MOG2 == filter->method) {
    run_mog2_iteration (filter);
  }

  /*  if we want to test_mode, just overwrite the output */
  std::vector < cv::Mat > channels (3);

  if (filter->test_mode) {
    cvtColor (filter->cvFG, filter->cvRGB, COLOR_GRAY2RGB);

    split (filter->cvRGB, channels);
  } else
    split (img, channels);

  channels.push_back (filter->cvFG);

  /*  copy anyhow the fg/bg to the alpha channel in the output image */
  merge (channels, img);


  return GST_FLOW_OK;
}


#ifdef CODE_FROM_OREILLY_BOOK   /* See license at the beginning of the page */
/*
  int update_codebook(uchar *p, codeBook &c, unsigned cbBounds)
  Updates the codebook entry with a new data point

  p Pointer to a YUV or HSI pixel
  c Codebook for this pixel
  cbBounds Learning bounds for codebook (Rule of thumb: 10)
  numChannels Number of color channels we¡¯re learning

  NOTES:
  cvBounds must be of length equal to numChannels

  RETURN
  codebook index
*/
int
update_codebook (unsigned char *p, codeBook * c, unsigned *cbBounds,
    int numChannels)
{
/* c->t+=1; */
  unsigned int high[3], low[3];
  int n, i;
  int matchChannel;

  for (n = 0; n < numChannels; n++) {
    high[n] = p[n] + cbBounds[n];
    if (high[n] > 255)
      high[n] = 255;

    if (p[n] > cbBounds[n])
      low[n] = p[n] - cbBounds[n];
    else
      low[n] = 0;
  }

/*  SEE IF THIS FITS AN EXISTING CODEWORD */
  for (i = 0; i < c->numEntries; i++) {
    matchChannel = 0;
    for (n = 0; n < numChannels; n++) {
      if ((c->cb[i]->learnLow[n] <= *(p + n)) &&
/* Found an entry for this channel */
          (*(p + n) <= c->cb[i]->learnHigh[n])) {
        matchChannel++;
      }
    }
    if (matchChannel == numChannels) {  /* If an entry was found */
      c->cb[i]->t_last_update = c->t;
/* adjust this codeword for the first channel */
      for (n = 0; n < numChannels; n++) {
        if (c->cb[i]->max[n] < *(p + n)) {
          c->cb[i]->max[n] = *(p + n);
        } else if (c->cb[i]->min[n] > *(p + n)) {
          c->cb[i]->min[n] = *(p + n);
        }
      }
      break;
    }
  }
/*  OVERHEAD TO TRACK POTENTIAL STALE ENTRIES */
  for (int s = 0; s < c->numEntries; s++) {
/*  Track which codebook entries are going stale: */
    int negRun = c->t - c->cb[s]->t_last_update;
    if (c->cb[s]->stale < negRun)
      c->cb[s]->stale = negRun;
  }
/*  ENTER A NEW CODEWORD IF NEEDED */
  if (i == c->numEntries) {     /* if no existing codeword found, make one */
    code_element **foo =
        (code_element **) g_malloc (sizeof (code_element *) *
        (c->numEntries + 1));
    for (int ii = 0; ii < c->numEntries; ii++) {
      foo[ii] = c->cb[ii];      /* copy all pointers */
    }
    foo[c->numEntries] = (code_element *) g_malloc (sizeof (code_element));
    if (c->numEntries)
      g_free (c->cb);
    c->cb = foo;
    for (n = 0; n < numChannels; n++) {
      c->cb[c->numEntries]->learnHigh[n] = high[n];
      c->cb[c->numEntries]->learnLow[n] = low[n];
      c->cb[c->numEntries]->max[n] = *(p + n);
      c->cb[c->numEntries]->min[n] = *(p + n);
    }
    c->cb[c->numEntries]->t_last_update = c->t;
    c->cb[c->numEntries]->stale = 0;
    c->numEntries += 1;
  }
/*  SLOWLY ADJUST LEARNING BOUNDS */
  for (n = 0; n < numChannels; n++) {
    if (c->cb[i]->learnHigh[n] < high[n])
      c->cb[i]->learnHigh[n] += 1;
    if (c->cb[i]->learnLow[n] > low[n])
      c->cb[i]->learnLow[n] -= 1;
  }
  return (i);
}





/*
 int clear_stale_entries(codeBook &c)
  During learning, after you've learned for some period of time,
  periodically call this to clear out stale codebook entries

  c Codebook to clean up

  Return
  number of entries cleared
*/
int
clear_stale_entries (codeBook * c)
{
  int staleThresh = c->t >> 1;
  int *keep = (int *) g_malloc (sizeof (int) * (c->numEntries));
  int keepCnt = 0;
  code_element **foo;
  int k;
  int numCleared;
/*  SEE WHICH CODEBOOK ENTRIES ARE TOO STALE */
  for (int i = 0; i < c->numEntries; i++) {
    if (c->cb[i]->stale > staleThresh)
      keep[i] = 0;              /* Mark for destruction */
    else {
      keep[i] = 1;              /* Mark to keep */
      keepCnt += 1;
    }
  }
  /*  KEEP ONLY THE GOOD */
  c->t = 0;                     /* Full reset on stale tracking */
  foo = (code_element **) g_malloc (sizeof (code_element *) * keepCnt);
  k = 0;
  for (int ii = 0; ii < c->numEntries; ii++) {
    if (keep[ii]) {
      foo[k] = c->cb[ii];
      /* We have to refresh these entries for next clearStale */
      foo[k]->t_last_update = 0;
      k++;
    }
  }
  /*  CLEAN UP */
  g_free (keep);
  g_free (c->cb);
  c->cb = foo;
  numCleared = c->numEntries - keepCnt;
  c->numEntries = keepCnt;
  return (numCleared);
}



/*
  uchar background_diff( uchar *p, codeBook &c,
  int minMod, int maxMod)
  Given a pixel and a codebook, determine if the pixel is
  covered by the codebook

  p Pixel pointer (YUV interleaved)
  c Codebook reference
  numChannels Number of channels we are testing
  maxMod Add this (possibly negative) number onto

  max level when determining if new pixel is foreground
  minMod Subtract this (possibly negative) number from
  min level when determining if new pixel is foreground

  NOTES:
  minMod and maxMod must have length numChannels,
  e.g. 3 channels => minMod[3], maxMod[3]. There is one min and
  one max threshold per channel.

  Return
  0 => background, 255 => foreground
*/
unsigned char
background_diff (unsigned char *p, codeBook * c, int numChannels,
    int *minMod, int *maxMod)
{
  int matchChannel;
/*  SEE IF THIS FITS AN EXISTING CODEWORD */
  int i;
  for (i = 0; i < c->numEntries; i++) {
    matchChannel = 0;
    for (int n = 0; n < numChannels; n++) {
      if ((c->cb[i]->min[n] - minMod[n] <= *(p + n)) &&
          (*(p + n) <= c->cb[i]->max[n] + maxMod[n])) {
        matchChannel++;         /* Found an entry for this channel */
      } else {
        break;
      }
    }
    if (matchChannel == numChannels) {
      break;                    /* Found an entry that matched all channels */
    }
  }
  if (i >= c->numEntries)
    return (255);
  return (0);
}




/*
 void find_connected_components(IplImage *mask, int poly1_hull0,
 float perimScale, int *num,
 CvRect *bbs, CvPoint *centers)
 This cleans up the foreground segmentation mask derived from calls
 to backgroundDiff

 mask Is a grayscale (8-bit depth) â€œrawâ€ mask image that
 will be cleaned up

 OPTIONAL PARAMETERS:
 poly1_hull0 If set, approximate connected component by
 (DEFAULT) polygon, or else convex hull (0)
 perimScale Len = image (width+height)/perimScale. If contour
 len < this, delete that contour (DEFAULT: 4)
 num Maximum number of rectangles and/or centers to
 return; on return, will contain number filled
 (DEFAULT: NULL)
 bbs Pointer to bounding box rectangle vector of
 length num. (DEFAULT SETTING: NULL)
 centers Pointer to contour centers vector of length
 num (DEFAULT: NULL)
*/

/* Approx.threshold - the bigger it is, the simpler is the boundary */
#define CVCONTOUR_APPROX_LEVEL 1
/* How many iterations of erosion and/or dilation there should be */
#define CVCLOSE_ITR 1
static void
find_connected_components (Mat mask, int poly1_hull0, float perimScale)
{
  /* Just some convenience variables */
  const Scalar CVX_WHITE = CV_RGB (0xff, 0xff, 0xff);
  //const Scalar CVX_BLACK = CV_RGB (0x00, 0x00, 0x00);
  int idx = 0;

  /* CLEAN UP RAW MASK */
  morphologyEx (mask, mask, MORPH_OPEN, Mat (), Point (-1, -1), CVCLOSE_ITR);
  morphologyEx (mask, mask, MORPH_CLOSE, Mat (), Point (-1, -1), CVCLOSE_ITR);
  /* FIND CONTOURS AROUND ONLY BIGGER REGIONS */

  std::vector < std::vector < Point > >contours;
  std::vector < std::vector < Point > >to_draw;
  std::vector < Vec4i > hierarchy;
  findContours (mask, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE,
      Point (0, 0));
  if (contours.size () == 0)
    return;

  for (; idx >= 0; idx = hierarchy[idx][0]) {
    const std::vector < Point > &c = contours[idx];
    double len = fabs (contourArea (Mat (c)));
    double q = (mask.size ().height + mask.size ().width) / perimScale;
    if (len >= q) {
      std::vector < Point > c_new;
      if (poly1_hull0) {
        approxPolyDP (c, c_new, CVCONTOUR_APPROX_LEVEL, (hierarchy[idx][2] < 0
                && hierarchy[idx][3] < 0));
      } else {
        convexHull (c, c_new, true, true);
      }
      to_draw.push_back (c_new);
    }
  }

  mask.setTo (Scalar::all (0));
  if (to_draw.size () > 0) {
    drawContours (mask, to_draw, -1, CVX_WHITE, FILLED);
  }

}
#endif /*ifdef CODE_FROM_OREILLY_BOOK */

int
run_mog_iteration (GstSegmentation * filter)
{
  /*
     BackgroundSubtractorMOG [1], Gaussian Mixture-based Background/Foreground
     Segmentation Algorithm. OpenCV MOG implements the algorithm described in [2].

     [1] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog
     [2] P. KadewTraKuPong and R. Bowden, "An improved adaptive background
     mixture model for real-time tracking with shadow detection", Proc. 2nd
     European Workshop on Advanced Video-Based Surveillance Systems, 2001
   */

  filter->mog->apply (filter->cvYUV, filter->cvFG, filter->learning_rate);

  return (0);
}

int
run_mog2_iteration (GstSegmentation * filter)
{
  /*
     BackgroundSubtractorMOG2 [1], Gaussian Mixture-based Background/Foreground
     segmentation algorithm. OpenCV MOG2 implements the algorithm described in
     [2] and [3].

     [1] http://opencv.itseez.com/modules/video/doc/motion_analysis_and_object_tracking.html#backgroundsubtractormog2
     [2] Z.Zivkovic, "Improved adaptive Gaussian mixture model for background
     subtraction", International Conference Pattern Recognition, UK, August, 2004.
     [3] Z.Zivkovic, F. van der Heijden, "Efficient Adaptive Density Estimation per
     Image Pixel for the Task of Background Subtraction", Pattern Recognition
     Letters, vol. 27, no. 7, pages 773-780, 2006.
   */

  filter->mog2->apply (filter->cvYUV, filter->cvFG, filter->learning_rate);

  return (0);
}
