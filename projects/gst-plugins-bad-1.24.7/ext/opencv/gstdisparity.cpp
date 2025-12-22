 /*
  * GStreamer
  * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
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
/*
 * SECTION:element-disparity
 *
 * This element computes a disparity map from two stereo images, meaning each one coming from a
 * different camera, both looking at the same scene and relatively close to each other - more on
 * this below. The disparity map is a proxy of the depth of a scene as seen from the camera.
 *
 * Assumptions: Input images are stereo, rectified and aligned. If these conditions are not met,
 * results can be poor. Both cameras should be looking parallel to maximize the overlapping
 * stereo area, and should not have objects too close or too far. The algorithms implemented here
 * run prefiltering stages to normalize brightness between the inputs, and to maximize texture.
 *
 * Note that in general is hard to find correspondences between soft textures, for instance a
 * block of gloss blue colour. The output is a gray image with values close to white meaning
 * closer to the cameras and darker far away. Black means that the pixels were not matched
 * correctly (not found). The resulting depth map can be transformed into real world coordinates
 * by means of OpenCV function (reprojectImageTo3D) but for this the camera matrixes need to
 * be fully known.
 *
 * Algorithm 1 is the OpenCV Stereo Block Matching, similar to the one developed by Kurt Konolige
 * [A] and that works by using small Sum-of-absolute-differenc (SAD) windows to find matching
 * points between the left and right rectified images. This algorithm finds only strongly matching
 * points between both images, this means normally strong textures. In soft textures, such as a
 * single coloured wall (as opposed to, f.i. a hairy rug), not all pixels might have correspondence.
 *
 * Algorithm 2 is the Semi Global Matching (SGM) algorithm [B] which models the scene structure
 * with a point-wise matching cost and an associated smoothness term. The energy minimization
 * is then computed in a multitude of 1D lines. For each point, the disparity corresponding to
 * the minimum aggregated cost is selected. In [B] the author proposes to use 8 or 16 different
 * independent paths. The SGM approach works well near depth discontinuities, but produces less
 * accurate results. Despite its relatively large memory footprint, this method is very fast and
 * potentially robust to complicated textured regions.
 *
 * Algorithm 3 is the OpenCV implementation of a modification of the variational stereo
 * correspondence algorithm, described in [C].
 *
 * Algorithm 4 is the Graph Cut stereo vision algorithm (GC) introduced in [D]; it is a global
 * stereo vision method. It calculates depth discontinuities by minimizing an energy function
 * combingin a point-wise matching cost and a smoothness term. The energy function is passed
 * to graph and Graph Cut is used to find a lowest-energy cut. GC is computationally intensive due
 * to its global nature and uses loads of memory, but it can deal with textureless regions and
 * reflections better than other methods.
 * Graphcut based technique is CPU intensive hence smaller framesizes are desired.
 *
 * Some test images can be found here: http://vision.stanford.edu/~birch/p2p/
 *
 * [A] K. Konolige. Small vision system. hardware and implementation. In Proc. International
 * Symposium on Robotics Research, pages 111--116, Hayama, Japan, 1997.
 * [B] H. Hirschmüller, “Accurate and efficient stereo processing by semi-global matching and
 * mutual information,” in Proceedings of the IEEE Conference on Computer Vision and Pattern
 * Recognition, 2005, pp. 807–814.
 * [C] S. Kosov, T. Thormaehlen, H.-P. Seidel "Accurate Real-Time Disparity Estimation with
 * Variational Methods" Proceedings of the 5th International Symposium on Visual Computing,
 * Vegas, USA
 * [D] Scharstein, D. & Szeliski, R. (2001). A taxonomy and evaluation of dense two-frame stereo
 * correspondence algorithms, International Journal of Computer Vision 47: 7–42.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! video/x-raw,width=320,height=240 ! videoconvert ! disp0.sink_right videotestsrc ! video/x-raw,width=320,height=240 ! videoconvert ! disp0.sink_left disparity name=disp0 ! videoconvert ! ximagesink
 * ]|
 * Another example, with two png files representing a classical stereo matching,
 * downloadable from http://vision.middlebury.edu/stereo/submit/tsukuba/im4.png and
 * im3.png. Note here they are downloaded in ~ (home).
 * |[
gst-launch-1.0    multifilesrc  location=~/im3.png ! pngdec ! videoconvert  ! disp0.sink_right     multifilesrc  location=~/im4.png ! pngdec ! videoconvert ! disp0.sink_left disparity   name=disp0 method=sbm     disp0.src ! videoconvert ! ximagesink
 * ]|
 * Yet another example with two cameras, which should be the same model, aligned etc.
 * |[
 gst-launch-1.0    v4l2src device=/dev/video1 ! video/x-raw,width=320,height=240 ! videoconvert  ! disp0.sink_right     v4l2src device=/dev/video0 ! video/x-raw,width=320,height=240 ! videoconvert ! disp0.sink_left disparity   name=disp0 method=sgbm     disp0.src ! videoconvert ! ximagesink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstdisparity.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_disparity_debug);
#define GST_CAT_DEFAULT gst_disparity_debug

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
  PROP_METHOD,
};

typedef enum
{
  METHOD_SBM,
  METHOD_SGBM
} GstDisparityMethod;

#define DEFAULT_METHOD METHOD_SGBM

#define GST_TYPE_DISPARITY_METHOD (gst_disparity_method_get_type ())
static GType
gst_disparity_method_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      {METHOD_SBM, "Global block matching algorithm", "sbm"},
      {METHOD_SGBM, "Semi-global block matching algorithm", "sgbm"},
      {0, NULL, NULL},
    };
    etype = g_enum_register_static ("GstDisparityMethod", values);
  }
  return etype;
}

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

G_DEFINE_TYPE_WITH_CODE (GstDisparity, gst_disparity, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_disparity_debug, "disparity", 0,
        "Stereo image disparity (depth) map calculation");
    );
GST_ELEMENT_REGISTER_DEFINE (disparity, "disparity", GST_RANK_NONE,
    GST_TYPE_DISPARITY);

static void gst_disparity_finalize (GObject * object);
static void gst_disparity_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_disparity_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_disparity_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_disparity_handle_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_disparity_handle_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_disparity_chain_right (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstFlowReturn gst_disparity_chain_left (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static void initialise_disparity (GstDisparity * fs, int width, int height,
    int nchannels);
static int initialise_sbm (GstDisparity * filter);
static int run_sbm_iteration (GstDisparity * filter);
static int run_sgbm_iteration (GstDisparity * filter);

/* initialize the disparity's class */
static void
gst_disparity_class_init (GstDisparityClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_disparity_finalize;
  gobject_class->set_property = gst_disparity_set_property;
  gobject_class->get_property = gst_disparity_get_property;


  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method",
          "Stereo matching method to use",
          "Stereo matching method to use",
          GST_TYPE_DISPARITY_METHOD, DEFAULT_METHOD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->change_state = gst_disparity_change_state;

  gst_element_class_set_static_metadata (element_class,
      "Stereo image disparity (depth) map calculation",
      "Filter/Effect/Video",
      "Calculates the stereo disparity map from two (sequences of) rectified and aligned stereo images",
      "Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_type_mark_as_plugin_api (GST_TYPE_DISPARITY_METHOD, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_disparity_init (GstDisparity * filter)
{
  filter->sinkpad_left =
      gst_pad_new_from_static_template (&sink_factory, "sink_left");
  gst_pad_set_event_function (filter->sinkpad_left,
      GST_DEBUG_FUNCPTR (gst_disparity_handle_sink_event));
  gst_pad_set_query_function (filter->sinkpad_left,
      GST_DEBUG_FUNCPTR (gst_disparity_handle_query));
  gst_pad_set_chain_function (filter->sinkpad_left,
      GST_DEBUG_FUNCPTR (gst_disparity_chain_left));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad_left);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad_left);

  filter->sinkpad_right =
      gst_pad_new_from_static_template (&sink_factory, "sink_right");
  gst_pad_set_event_function (filter->sinkpad_right,
      GST_DEBUG_FUNCPTR (gst_disparity_handle_sink_event));
  gst_pad_set_query_function (filter->sinkpad_right,
      GST_DEBUG_FUNCPTR (gst_disparity_handle_query));
  gst_pad_set_chain_function (filter->sinkpad_right,
      GST_DEBUG_FUNCPTR (gst_disparity_chain_right));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad_right);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad_right);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  g_mutex_init (&filter->lock);
  g_cond_init (&filter->cond);

  filter->method = DEFAULT_METHOD;
}

static void
gst_disparity_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDisparity *filter = GST_DISPARITY (object);
  switch (prop_id) {
    case PROP_METHOD:
      filter->method = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_disparity_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDisparity *filter = GST_DISPARITY (object);

  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, filter->method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */
static GstStateChangeReturn
gst_disparity_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstDisparity *fs = GST_DISPARITY (element);
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&fs->lock);
      fs->flushing = true;
      g_cond_signal (&fs->cond);
      g_mutex_unlock (&fs->lock);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock (&fs->lock);
      fs->flushing = false;
      g_mutex_unlock (&fs->lock);
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_disparity_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&fs->lock);
      fs->flushing = true;
      g_cond_signal (&fs->cond);
      g_mutex_unlock (&fs->lock);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock (&fs->lock);
      fs->flushing = false;
      g_mutex_unlock (&fs->lock);
      break;
    default:
      break;
  }
  return ret;
}

static gboolean
gst_disparity_handle_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event)
{
  gboolean ret = TRUE;
  GstDisparity *fs = GST_DISPARITY (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstVideoInfo info;
      gst_event_parse_caps (event, &caps);

      /* Critical section since both pads handle event sinking simultaneously */
      g_mutex_lock (&fs->lock);
      gst_video_info_from_caps (&info, caps);

      GST_INFO_OBJECT (pad, " Negotiating caps via event %" GST_PTR_FORMAT,
          caps);
      if (!gst_pad_has_current_caps (fs->srcpad)) {
        /* Init image info (width, height, etc) and all OpenCV matrices */
        initialise_disparity (fs, info.width, info.height,
            info.finfo->n_components);

        /* Initialise and keep the caps. Force them on src pad */
        fs->caps = gst_video_info_to_caps (&info);
        gst_pad_set_caps (fs->srcpad, fs->caps);

      } else if (!gst_caps_is_equal (fs->caps, caps)) {
        ret = FALSE;
      }
      g_mutex_unlock (&fs->lock);

      GST_INFO_OBJECT (pad,
          " Negotiated caps (result %d) via event: %" GST_PTR_FORMAT, ret,
          caps);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static gboolean
gst_disparity_handle_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstDisparity *fs = GST_DISPARITY (parent);
  gboolean ret = TRUE;
  GstCaps *template_caps;
  GstCaps *current_caps;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      g_mutex_lock (&fs->lock);
      current_caps = gst_pad_get_current_caps (fs->srcpad);
      if (current_caps == NULL) {
        template_caps = gst_pad_get_pad_template_caps (pad);
        gst_query_set_caps_result (query, template_caps);
        gst_caps_unref (template_caps);
      } else {
        gst_query_set_caps_result (query, current_caps);
        gst_caps_unref (current_caps);
      }
      g_mutex_unlock (&fs->lock);
      ret = TRUE;
      break;
    case GST_QUERY_ALLOCATION:
      if (pad == fs->sinkpad_right)
        ret = gst_pad_peer_query (fs->srcpad, query);
      else
        ret = FALSE;
      break;
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }
  return ret;
}

static void
gst_disparity_finalize (GObject * object)
{
  GstDisparity *filter;

  filter = GST_DISPARITY (object);

  filter->cvRGB_right.release ();
  filter->cvRGB_left.release ();
  filter->cvGray_right.release ();
  filter->cvGray_left.release ();
  filter->cvGray_depth_map1.release ();
  filter->cvGray_depth_map2.release ();
  filter->cvGray_depth_map1_2.release ();
  filter->img_right_as_cvMat_gray.release ();
  filter->img_left_as_cvMat_gray.release ();
  filter->depth_map_as_cvMat.release ();
  filter->sbm.release ();
  filter->sgbm.release ();

  gst_caps_replace (&filter->caps, NULL);

  g_cond_clear (&filter->cond);
  g_mutex_clear (&filter->lock);
  G_OBJECT_CLASS (gst_disparity_parent_class)->finalize (object);
}



static GstFlowReturn
gst_disparity_chain_left (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDisparity *fs;
  GstMapInfo info;

  fs = GST_DISPARITY (parent);
  GST_DEBUG_OBJECT (pad, "processing frame from left");
  g_mutex_lock (&fs->lock);
  if (fs->flushing) {
    g_mutex_unlock (&fs->lock);
    return GST_FLOW_FLUSHING;
  }
  if (fs->buffer_left) {
    GST_DEBUG_OBJECT (pad, " right is busy, wait and hold");
    g_cond_wait (&fs->cond, &fs->lock);
    GST_DEBUG_OBJECT (pad, " right is free, continuing");
    if (fs->flushing) {
      g_mutex_unlock (&fs->lock);
      return GST_FLOW_FLUSHING;
    }
  }
  fs->buffer_left = buffer;

  if (!gst_buffer_map (buffer, &info, (GstMapFlags) GST_MAP_READWRITE)) {
    return GST_FLOW_ERROR;
  }
  fs->cvRGB_left.data = (unsigned char *) info.data;
  fs->cvRGB_left.datastart = (unsigned char *) info.data;

  GST_DEBUG_OBJECT (pad, "signalled right");
  g_cond_signal (&fs->cond);
  g_mutex_unlock (&fs->lock);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_disparity_chain_right (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDisparity *fs;
  GstMapInfo info;
  GstFlowReturn ret;

  fs = GST_DISPARITY (parent);
  GST_DEBUG_OBJECT (pad, "processing frame from right");
  g_mutex_lock (&fs->lock);
  if (fs->flushing) {
    g_mutex_unlock (&fs->lock);
    return GST_FLOW_FLUSHING;
  }
  if (fs->buffer_left == NULL) {
    GST_DEBUG_OBJECT (pad, " left has not provided another frame yet, waiting");
    g_cond_wait (&fs->cond, &fs->lock);
    GST_DEBUG_OBJECT (pad, " left has just provided a frame, continuing");
    if (fs->flushing) {
      g_mutex_unlock (&fs->lock);
      return GST_FLOW_FLUSHING;
    }
  }
  if (!gst_buffer_map (buffer, &info, (GstMapFlags) GST_MAP_READWRITE)) {
    g_mutex_unlock (&fs->lock);
    return GST_FLOW_ERROR;
  }

  fs->cvRGB_right.data = (unsigned char *) info.data;
  fs->cvRGB_right.datastart = (unsigned char *) info.data;

  /* Here do the business */
  GST_INFO_OBJECT (pad,
      "comparing frames, %dB (%dx%d) %d channels", (int) info.size,
      fs->width, fs->height, fs->actualChannels);

  /* Stereo corresponding using semi-global block matching. According to OpenCV:
     "" The class implements modified H. Hirschmuller algorithm HH08 . The main
     differences between the implemented algorithm and the original one are:

     - by default the algorithm is single-pass, i.e. instead of 8 directions we
     only consider 5. Set fullDP=true to run the full variant of the algorithm
     (which could consume a lot of memory)
     - the algorithm matches blocks, not individual pixels (though, by setting
     SADWindowSize=1 the blocks are reduced to single pixels)
     - mutual information cost function is not implemented. Instead, we use a
     simpler Birchfield-Tomasi sub-pixel metric from BT96 , though the color
     images are supported as well.
     - we include some pre- and post- processing steps from K. Konolige
     algorithm FindStereoCorrespondenceBM , such as pre-filtering
     ( CV_STEREO_BM_XSOBEL type) and post-filtering (uniqueness check, quadratic
     interpolation and speckle filtering) ""
   */
  if (METHOD_SGBM == fs->method) {
    cvtColor (fs->cvRGB_left, fs->cvGray_left, COLOR_RGB2GRAY);
    cvtColor (fs->cvRGB_right, fs->cvGray_right, COLOR_RGB2GRAY);
    run_sgbm_iteration (fs);
    normalize (fs->cvGray_depth_map1, fs->cvGray_depth_map2, 0, 255,
        NORM_MINMAX, fs->cvGray_depth_map2.type ());
    cvtColor (fs->cvGray_depth_map2, fs->cvRGB_right, COLOR_GRAY2RGB);
  }
  /* Algorithm 1 is the OpenCV Stereo Block Matching, similar to the one
     developed by Kurt Konolige [A] and that works by using small Sum-of-absolute-
     differences (SAD) window. See the comments on top of the file.
   */
  else if (METHOD_SBM == fs->method) {
    cvtColor (fs->cvRGB_left, fs->cvGray_left, COLOR_RGB2GRAY);
    cvtColor (fs->cvRGB_right, fs->cvGray_right, COLOR_RGB2GRAY);
    run_sbm_iteration (fs);
    normalize (fs->cvGray_depth_map1, fs->cvGray_depth_map2, 0, 255,
        NORM_MINMAX, fs->cvGray_depth_map2.type ());
    cvtColor (fs->cvGray_depth_map2, fs->cvRGB_right, COLOR_GRAY2RGB);
  }


  GST_DEBUG_OBJECT (pad, " right has finished");
  gst_buffer_unmap (fs->buffer_left, &info);
  gst_buffer_unref (fs->buffer_left);
  fs->buffer_left = NULL;
  g_cond_signal (&fs->cond);
  g_mutex_unlock (&fs->lock);

  ret = gst_pad_push (fs->srcpad, buffer);
  return ret;
}

static void
initialise_disparity (GstDisparity * fs, int width, int height, int nchannels)
{
  int cv_type = CV_8UC3;
  fs->width = width;
  fs->height = height;
  fs->actualChannels = nchannels;

  fs->imgSize = Size (fs->width, fs->height);
  if (fs->actualChannels == 1) {
    cv_type = CV_8UC1;
  } else if (fs->actualChannels == 2) {
    cv_type = CV_8UC2;
  }

  fs->cvRGB_right.create (fs->imgSize, cv_type);
  fs->cvRGB_left.create (fs->imgSize, cv_type);
  fs->cvGray_right.create (fs->imgSize, CV_8UC1);
  fs->cvGray_left.create (fs->imgSize, CV_8UC1);

  fs->cvGray_depth_map1.create (fs->imgSize, CV_16SC1);
  fs->cvGray_depth_map2.create (fs->imgSize, CV_8UC1);
  fs->cvGray_depth_map1_2.create (fs->imgSize, CV_16SC1);

  /* Stereo Block Matching methods */
  initialise_sbm (fs);
}

int
initialise_sbm (GstDisparity * filter)
{
  filter->img_right_as_cvMat_gray = Mat (filter->cvGray_right);
  filter->img_left_as_cvMat_gray = Mat (filter->cvGray_left);
  filter->depth_map_as_cvMat = Mat (filter->cvGray_depth_map1);

  filter->sbm = StereoBM::create ();
  filter->sgbm = StereoSGBM::create (1, 64, 3);

  filter->sbm->setBlockSize (9);
  filter->sbm->setNumDisparities (32);
  filter->sbm->setPreFilterSize (9);
  filter->sbm->setPreFilterCap (32);
  filter->sbm->setMinDisparity (0);
  filter->sbm->setTextureThreshold (0);
  filter->sbm->setUniquenessRatio (0);
  filter->sbm->setSpeckleWindowSize (0);
  filter->sbm->setSpeckleRange (0);
  filter->sbm->setDisp12MaxDiff (0);

  filter->sgbm->setMinDisparity (1);
  filter->sgbm->setNumDisparities (64);
  filter->sgbm->setBlockSize (3);
  filter->sgbm->setP1 (200);
  filter->sgbm->setP2 (255);
  filter->sgbm->setDisp12MaxDiff (0);
  filter->sgbm->setPreFilterCap (0);
  filter->sgbm->setUniquenessRatio (0);
  filter->sgbm->setSpeckleWindowSize (0);
  filter->sgbm->setSpeckleRange (0);
  filter->sgbm->setMode (StereoSGBM::MODE_HH);

  return (0);
}

int
run_sbm_iteration (GstDisparity * filter)
{
  ((StereoBM *) filter->sbm)->compute (filter->img_left_as_cvMat_gray,
      filter->img_right_as_cvMat_gray, filter->depth_map_as_cvMat);

  return (0);
}

int
run_sgbm_iteration (GstDisparity * filter)
{
  ((StereoSGBM *) filter->sgbm)->compute (filter->img_left_as_cvMat_gray,
      filter->img_right_as_cvMat_gray, filter->depth_map_as_cvMat);

  return (0);
}
