/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

/**
 * plugin-avtp:
 *
 * ## Audio Video Transport Protocol (AVTP) Plugin
 *
 * The AVTP plugin implements typical Talker and Listener functionalities that
 * can be leveraged by GStreamer-based applications in order to implement TSN
 * audio/video applications.
 *
 * ### Dependencies
 *
 * The plugin uses libavtp to handle AVTP packetization. Libavtp source code can
 * be found in https://github.com/AVnu/libavtp as well as instructions to build
 * and install it.
 *
 * If libavtp isn't detected by configure, the plugin isn't built.
 *
 * ### The application/x-avtp mime type
 *
 * For valid AVTPDUs encapsulated in GstBuffers, we use the caps with mime type
 * application/x-avtp.
 *
 * AVTP mime type is pretty simple and has no fields.
 *
 * ### gPTP Setup
 *
 * The Linuxptp project provides the ptp4l daemon, which synchronizes the PTP
 * clock from NIC, and the pmc tool which communicates with ptp4l to get/set
 * some runtime settings. The project also provides the phc2sys daemon which
 * synchronizes the PTP clock and system clock.
 *
 * The AVTP plugin requires system clock is synchronized with PTP clock and
 * TAI offset is properly set in the kernel. ptp4l and phc2sys can be set up
 * in many different ways, below we provide an example that fullfils the plugin
 * requirements. For further information check ptp4l(8) and phc2sys(8).
 *
 * In the following instructions, replace $IFNAME by your PTP capable NIC
 * interface. The gPTP.cfg file mentioned below can be found in /usr/share/
 * doc/linuxptp/ (depending on your distro).
 *
 * Synchronize PTP clock with PTP time:
 *
 *	$ ptp4l -f gPTP.cfg -i $IFNAME
 *
 * Enable TAI offset to be automatically set by phc2sys:
 *
 *	$ pmc -u -t 1 -b 0 'SET GRANDMASTER_SETTINGS_NP \
 * 		clockClass 248 clockAccuracy 0xfe \
 * 		offsetScaledLogVariance 0xffff \
 * 		currentUtcOffset 37 leap61 0 leap59 0 \
 * 		currentUtcOffsetValid 1 ptpTimescale 1 \
 * 		timeTraceable 1 frequencyTraceable 0 timeSource 0xa0'
 *
 * Synchronize system clock with PTP clock:
 *
 * 	$ phc2sys -f gPTP.cfg -s $IFNAME -c CLOCK_REALTIME -w
 *
 * The commands above should be run on both AVTP Talker and Listener hosts.
 *
 * With clocks properly synchronized, applications using the AVTP plugin
 * should use GstSytemClock with GST_CLOCK_TYPE_REALTIME as the pipeline
 * clock.
 *
 * ### Clock Reference Format (CRF)
 *
 * Even though the systems are synchronized by PTP, it is possible that
 * different talkers can send media streams which are out of phase or the
 * frequencies do not exactly match. This is partcularly important when there
 * is a single listener processing data from multiple talkers. The systems in
 * this scenario can benefit if a common clock is distributed among the
 * systems.
 *
 * This can be achieved by using the avtpcrfsync element which implements CRF
 * as described in Chapter 10 of IEEE 1722-2016. avtpcrfcheck can also be used
 * to validate that the adjustment conforms to the criteria specified in the
 * spec. For further details, look at the documentation for the respective
 * elements.
 *
 * ### Traffic Control Setup
 *
 * FQTSS (Forwarding and Queuing Enhancements for Time-Sensitive Streams) can be
 * enabled on Linux with the help of the mqprio and cbs qdiscs provided by the
 * Linux Traffic Control. Below we provide an example to configure those qdiscs
 * in order to transmit a CVF H.264 stream 1280x720@30fps. For further
 * information on how to configure these qdiscs check tc-mqprio(8) and
 * tc-cbs(8) man pages.
 *
 * On the host that will run as AVTP Talker (pipeline that generates the video
 * stream), run the following commands:
 *
 * Configure mpqrio qdisc (replace $MQPRIO_HANDLE_ID by an unused handle ID):
 *
 *     $ tc qdisc add dev $IFNAME parent root handle $MQPRIO_HANDLE_ID mqprio \
 *         num_tc 3 map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
 *         queues 1@0 1@1 2@2 hw 0
 *
 * Configure cbs qdisc (replace $CBS_HANDLE_ID by an unused handle ID):
 *
 *     $ tc qdisc replace dev $IFNAME parent $MQPRIO_HANDLE_ID:1 \
 *         handle $CBS_HANDLE_ID cbs idleslope 27756 sendslope -972244 \
 *         hicredit 42 locredit -1499 offload 1
 *
 * Also, the plugin implements a transmission scheduling mechanism that relies
 * on ETF qdisc so make sure it is properly configured in your system. It could
 * be configured in many ways, below follows an example.
 *
 *     $ tc qdisc add dev $IFNAME parent $CBS_HANDLE_ID:1 etf \
 *         clockid CLOCK_TAI delta 500000 offload
 *
 * No Traffic Control configuration is required at the host running as AVTP
 * Listener.
 *
 * ### Capabilities
 *
 * The `avtpsink` and `avtpsrc` elements open `AF_PACKET` sockets, which require
 * `CAP_NET_RAW` capability. Also, `avtpsink` needs `CAP_NET_ADMIN` to use ETF.
 * Therefore, applications must have those capabilities in order to successfully
 * use these elements. For instance, one can use:
 *
 *     $ sudo setcap cap_net_raw,cap_net_admin+ep <application>
 *
 * Applications can drop these capabilities after the sockets are open, after
 * `avtpsrc` or `avtpsink` elements transition to PAUSED state. See setcap(8)
 * man page for more information.
 *
 * ### Elements configuration
 *
 * Each element has its own configuration properties, with some being common
 * to several elements. Basic properties are:
 *
 *   * streamid (avtpaafpay, avtprvfpay, avtpcvfpay, avtprvfdepay, avtpcvfdepay,
 *     avtprvfdepay, avtpcrfsync, avtpcrfcheck): Stream ID associated with the
 *     stream.
 *
 *   * ifname (avtpsink, avtpsrc, avtpcrfsync, avtpcrfcheck): Network interface
 *     used to send/receive AVTP packets.
 *
 *   * dst-macaddr (avtpsink, avtpsrc): Destination MAC address for the stream.
 *
 *   * priority (avtpsink): Priority used by the plugin to transmit AVTP
 *     traffic.
 *
 *   * mtt (avtpaafpay, avtprvfpay, avtpcvfpay): Maximum Transit Time, in
 *     nanoseconds, as defined in AVTP spec.
 *
 *   * tu (avtpaafpay, avtprvfpay, avtpcvfpay): Maximum Time Uncertainty, in
 *     nanoseconds, as defined in AVTP spec.
 *
 *   * processing-deadline (avtpaafpay, avtprvfpay, avtpcvfpay, avtpsink):
 *     Maximum amount of time, in nanoseconds, that the pipeline is expected to
 *     process any buffer. This value should be in sync between the one used on
 *     the payloader and the sink, as this time is also taken into consideration
 *     to define the correct presentation time of the packets on the AVTP listener
 *     side. It should be as low as possible (zero if possible).
 *
 *   * timestamp-mode (avtpaafpay): AAF timestamping mode, as defined in AVTP spec.
 *
 *   * mtu (avtprvfpay, avtpcvfpay): Maximum Transmit Unit of the underlying network,
 *     used to determine when to fragment a RVF/CVF packet and how big it should be.
 *
 * Check each element documentation for more details.
 *
 *
 * ### Running a sample pipeline
 *
 * The following pipelines uses debugutilsbad "clockselect" element to force
 * the pipeline clock to be GstPtpClock. A real application would
 * programmatically define GstPtpClock as the pipeline clock (see next section).
 * It is also assumes that `gst-launch-1.0` has CAP_NET_RAW and CAP_NET_ADMIN
 * capabilities.
 *
 * On the AVTP talker, the following pipeline can be used to generate an H.264
 * stream to be sent via network using AVTP:
 *
 *     $ gst-launch-1.0 clockselect. \( clockid=ptp \
 *         videotestsrc is-live=true ! clockoverlay ! x264enc ! \
 *         avtpcvfpay processing-deadline=20000000 ! \
 *         avtpcrfsync ifname=$IFNAME ! avtpsink ifname=$IFNAME \)
 *
 * On the AVTP listener host, the following pipeline can be used to get the
 * AVTP stream, depacketize it and show it on the screen:
 *
 *     $ gst-launch-1.0 clockselect. \( clockid=ptp avtpsrc ifname=$IFNAME ! \
 *         avtpcrfcheck ifname=$IFNAME ! avtpcvfdepay ! \
 *         vaapih264dec ! videoconvert ! clockoverlay halignment=right ! \
 *         queue ! autovideosink \)
 *
 * ### Pipeline clock
 *
 * The AVTP plugin elements require that the pipeline clock is in sync with
 * the network PTP clock. As GStreamer has a GstPtpClock, using it should be
 * the simplest way of achieving that.
 *
 * However, as there's no way of forcing a clock to a pipeline using
 * gst-launch-1.0 application, even for quick tests, it's necessary to have
 * an application. One can refer to GStreamer "hello world" application,
 * remembering to set the pipeline clock to GstPtpClock before putting the
 * pipeline on "PLAYING" state. Some code like:
 *
 *     GstClock *clk = gst_ptp_clock_new("ptp-clock", 0);
 *     gst_clock_wait_for_sync(clk, GST_CLOCK_TIME_NONE);
 *     gst_pipeline_use_clock (GST_PIPELINE (pipeline), clk);
 *
 * Would do the trick.
 *
 * ### Disclaimer
 *
 * It's out of scope for the AVTP plugin to verify how it is invoked, should
 * a malicious software do it for Denial of Service attempts, or other
 * compromises attempts.
 *
 * Since: 1.18
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "gstavtpaafdepay.h"
#include "gstavtpaafpay.h"
#include "gstavtpcvfdepay.h"
#include "gstavtprvfdepay.h"
#include "gstavtpcvfpay.h"
#include "gstavtprvfpay.h"
#include "gstavtpsink.h"
#include "gstavtpsrc.h"
#include "gstavtpcrfsync.h"
#include "gstavtpcrfcheck.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (avtpaafpay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpaafdepay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpsink, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (avtprvfpay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpcvfpay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtprvfdepay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpcvfdepay, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpcrfsync, plugin);
  ret |= GST_ELEMENT_REGISTER (avtpcrfcheck, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    avtp, "Audio/Video Transport Protocol (AVTP) plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
