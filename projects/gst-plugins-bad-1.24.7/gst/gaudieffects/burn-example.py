#!/usr/bin/python
import gobject; gobject.threads_init()
import pygst; pygst.require("0.10")
import gst

p = gst.parse_launch ("""
   v4l2src !
   videoconvert ! queue ! video/x-raw,width=320,height=240,framerate=30/1 !  burn qos=true name=vf ! videoconvert !  timeoverlay ! xvimagesink
   """)

m = p.get_by_name ("vf")
m.set_property ("adjustment", 128)

control = gst.Controller(m, "adjustment")
control.set_interpolation_mode("adjustment", gst.INTERPOLATE_LINEAR)
control.set("adjustment", 0 * gst.SECOND, 128)
control.set("adjustment", 5 * gst.SECOND, 256)
control.set("adjustment", 25 * gst.SECOND, 0)

p.set_state (gst.STATE_PLAYING)

gobject.MainLoop().run()
