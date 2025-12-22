# GStreamer AJA source/sink plugin

[GStreamer](https://gstreamer.freedesktop.org/) plugin for
[AJA](https://www.aja.com) capture and output cards.

This plugin requires the AJA NTV2 SDK version 16 or newer.

The location of the SDK can be configured via the `aja-sdk-dir` meson option.
If no location is given then the NTV2 SDK from
[GitHub](https://github.com/aja-video/ntv2.git) is compiled as a meson
subproject as part of the plugin.

## Example usage

Capture 1080p30 audio/video and display it locally

```sh
gst-launch-1.0 ajasrc video-format=1080p-3000 ! ajasrcdemux name=d \
    d.video ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! videoconvert ! autovideosink \
    d.audio ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! audioconvert ! audioresample ! autoaudiosink
```

Output a 1080p2997 test audio/video stream

```sh
gst-launch-1.0 videotestsrc pattern=ball ! video/x-raw,format=v210,width=1920,height=1080,framerate=30000/1001,interlace-mode=progressive ! timeoverlay ! timecodestamper ! combiner.video \
    audiotestsrc freq=440 ! audio/x-raw,format=S32LE,rate=48000,channels=16 ! audiobuffersplit output-buffer-duration=1/30 ! combiner.audio \
    ajasinkcombiner name=combiner ! ajasink channel=0
```

Capture 1080p30 audio/video and directly output it again on the same card

```sh
gst-launch-1.0 ajasrc video-format=1080p-3000 channel=1 input-source=sdi-1 audio-system=2 ! ajasrcdemux name=d \
    d.video ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! c.video \
    d.audio ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! c.audio \
    ajasinkcombiner name=c ! ajasink channel=0 reference-source=input-1
```
