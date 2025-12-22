#!/usr/bin/python
import gobject; gobject.threads_init()
import pygst; pygst.require("0.10")
import gst

p = gst.parse_launch ("""
   v4l2src !
   videoconvert ! queue ! video/x-raw,width=320,height=240,framerate=30/1 !  gaussianblur qos=true name=vf ! videoconvert !  timeoverlay ! xvimagesink
   """)

m = p.get_by_name ("vf")
m.set_property ("sigma", 0.5)

control = gst.Controller(m, "sigma")
control.set_interpolation_mode("sigma", gst.INTERPOLATE_LINEAR)
control.set("sigma", 0 * gst.SECOND, 0.5)
control.set("sigma", 5 * gst.SECOND, 10.0)
control.set("sigma", 25 * gst.SECOND, -5.0)

p.set_state (gst.STATE_PLAYING)

gobject.MainLoop().run()
