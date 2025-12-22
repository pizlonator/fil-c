/*
 * WebRTC Audio Processing Elements
 *
 *  Copyright 2016 Collabora Ltd
 *    @author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/**
 * SECTION:element-webrtcdsp
 * @short_description: Audio Filter using WebRTC Audio Processing library
 *
 * A voice enhancement filter based on WebRTC Audio Processing library. This
 * library provides a whide variety of enhancement algorithms. This element
 * tries to enable as much as possible. The currently enabled enhancements are
 * High Pass Filter, Echo Canceller, Noise Suppression, Automatic Gain Control,
 * and some extended filters.
 *
 * While webrtcdsp element can be used alone, there is an exception for the
 * echo canceller. The audio canceller need to be aware of the far end streams
 * that are played to loud speakers. For this, you must place a webrtcechoprobe
 * element at that far end. Note that the sample rate must match between
 * webrtcdsp and the webrtechoprobe. Though, the number of channels can differ.
 * The probe is found by the DSP element using it's object name. By default,
 * webrtcdsp looks for webrtcechoprobe0, which means it just work if you have
 * a single probe and DSP.
 *
 * The probe can only be used within the same top level GstPipeline.
 * Additionally, to simplify the code, the probe element must be created
 * before the DSP sink pad is activated. It does not need to be in any
 * particular state and does not even need to be added to the pipeline yet.
 *
 * # Example launch line
 *
 * As a convenience, the echo canceller can be tested using an echo loop. In
 * this configuration, one would expect a single echo to be heard.
 *
 * |[
 * gst-launch-1.0 pulsesrc ! webrtcdsp ! webrtcechoprobe ! pulsesink
 * ]|
 *
 * In real environment, you'll place the probe before the playback, but only
 * process the far end streams. The DSP should be placed as close as possible
 * to the audio capture. The following pipeline is astracted and does not
 * represent a real pipeline.
 *
 * |[
 * gst-launch-1.0 far-end-src ! audio/x-raw,rate=48000 ! webrtcechoprobe ! pulsesink \
 *                pulsesrc ! audio/x-raw,rate=48000 ! webrtcdsp ! far-end-sink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwebrtcdsp.h"
#include "gstwebrtcechoprobe.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (webrtcdsp, plugin);
  ret |= GST_ELEMENT_REGISTER (webrtcechoprobe, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    webrtcdsp,
    "Voice pre-processing using WebRTC Audio Processing Library",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
