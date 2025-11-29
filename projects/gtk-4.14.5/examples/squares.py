#!/usr/bin/env -S GI_TYPELIB_PATH=${PWD}/build/gtk:${GI_TYPELIB_PATH} LD_PRELOAD=${LD_PRELOAD}:${PWD}/build/gtk/libgtk-4.so python3

import gi

gi.require_version('Gdk', '4.0')
gi.require_version('Gtk', '4.0')

from gi.repository import Gdk
from gi.repository import Gtk
from gi.repository import Graphene


class DemoWidget(Gtk.Widget):

    __gtype_name__ = "DemoWidget"

    def __init__(self):
        super().__init__()

    def do_measure(self, orientation, for_size: int):
        # We need some space to draw
        return 100, 200, -1, -1

    def do_snapshot(self, snapshot):
        # Draw four color squares
        color = Gdk.RGBA()
        rect = Graphene.Rect.alloc()

        width = self.get_width() / 2
        height = self.get_height() / 2

        Gdk.RGBA.parse(color, "red")
        rect.init(0, 0, width, height)
        snapshot.append_color(color, rect)

        Gdk.RGBA.parse(color, "green")
        rect.init(width, 0, width, height)
        snapshot.append_color(color, rect)

        Gdk.RGBA.parse(color, "yellow")
        rect.init(0, height, width, height)
        snapshot.append_color(color, rect)

        Gdk.RGBA.parse(color, "blue")
        rect.init(width, height, width, height)
        snapshot.append_color(color, rect)

def on_activate(app):
    # Create a new window
    win = Gtk.ApplicationWindow(application=app)
    win.set_title("Squares")
    icon = DemoWidget()
    win.set_child(icon)
    win.present()

# Create a new application
app = Gtk.Application(application_id='org.gtk.exampleapp')
app.connect('activate', on_activate)

# Run the application
app.run(None)
