import sys
import os
import subprocess
import gi

gi.require_version('Gdk', '4.0')

from gi.repository import GLib, Gdk
from pydbus import SessionBus

verbose = True

screen_cast = None
monitors = {}
waiting = False
done = False
monitor_model = None
display = None

def terminate():
    for key in monitors:
        monitor = monitors[key];
        pipeline = monitor['pipeline'];
        pipeline.terminate()
    sys.exit(1)

def stream_added_closure(name):
    def stream_added(node_id):
        if verbose:
            print('pipewire stream added')

        monitor = monitors[name];

        freq = monitor['freq'];
        width = monitor['width'];
        height = monitor['height'];
        # FIXME scale = monitor['scale'];

        # Use gstreamer out-of-process, since the gst gl support gets
        # itself into a twist with its wayland connection when monitors
        # disappear
        pipeline_desc = f'gst-launch-1.0 pipewiresrc path={node_id} ! video/x-raw,max-framerate={freq}/1,width={width},height={height} ! videoconvert ! glimagesink' # >& gstreamer-monitor.log'
        if verbose:
            print(f'launching {pipeline_desc}')
        monitor['pipeline'] = subprocess.Popen([pipeline_desc], shell=True)

    return stream_added

def add_monitor(name, width, height, scale, freq):
    if verbose:
        print(f'add monitor {name}: {width}x{height}, scale {scale}, frequency {freq}')
    session_path = screen_cast.CreateSession({})
    session = bus.get('org.gnome.Mutter.ScreenCast', session_path)
    monitors[name] = {
        'session': session,
        'width': width,
        'height': height,
        'scale': scale,
        'freq': freq
    }
    stream_path = session.RecordVirtual({})
    stream = bus.get('org.gnome.Mutter.ScreenCast', stream_path)
    stream.onPipeWireStreamAdded = stream_added_closure(name)
    session.Start()

def remove_monitor(name):
    if verbose:
        print(f'remove monitor {name}')
    try:
        monitor = monitors[name];
        pipeline = monitor['pipeline']
        pipeline.kill()
        session = monitor['session']
        session.Stop()
    except KeyError:
        print('failed to remove monitor')
    monitors[name] = None

expected_change = None
loop = None

def quit_cb(loop):
    loop.quit()
    print('timed out while waiting')

def wait(millis):
    global loop
    before = GLib.get_monotonic_time()
    loop = GLib.MainLoop()
    GLib.timeout_add(millis, quit_cb, loop)
    loop.run()
    if verbose:
        time = (GLib.get_monotonic_time() - before) / 1000
        print(f'waited for {time} milliseconds')

def monitors_changed(monitors, position, removed, added):
    global expected_change

    assert expected_change != None, 'No change expected'
    assert position == expected_change['position'], 'Unexpected position in monitors-changed'
    assert removed == expected_change['removed'], 'Unexpected removed in monitors-changed'
    assert added == expected_change['added'], 'Unexpected added in monitors-changed'

    if verbose:
        print('got expected monitors-changed signal')

    expected_change = None
    loop.quit()

def launch_observer():
    global monitor_model
    global display

    if display == None:
        display = Gdk.Display.open(os.getenv('WAYLAND_DISPLAY'))

    if verbose:
        print('launch observer')

    monitor_model = display.get_monitors()
    assert monitor_model.get_n_items() == 0, 'Unexpected initial monitors'
    monitor_model.connect('items-changed', monitors_changed)

def expect_monitors_changed(position, removed, added, timeout):
    global expected_change
    expected_change = {
        'position' : position,
        'removed' : removed,
        'added' : added
    }
    wait(timeout)
    assert expected_change == None, 'Expected change did not happen'

def got_connector(monitor, pspec):
    loop.quit()

def expect_monitor(position, width, height, scale, freq):
    assert monitor_model.get_n_items() > position, f'Monitor {position} not present'
    monitor = monitor_model.get_item(position)
    if monitor.get_connector() == None:
        if verbose:
            print('waiting for connector')
        handler = monitor.connect('notify::connector', got_connector)
        wait(500)
        monitor.disconnect(handler)
    assert monitor.get_connector() != None, 'Monitor has no connector'
    assert monitor.is_valid(), 'Monitor is not valid'
    geometry = monitor.get_geometry()
    assert geometry.width == width, 'Unexpected monitor width'
    assert geometry.height == height, 'Unexpected monitor height'
    assert monitor.get_scale_factor() == scale, 'Unexpected scale factor'
    assert monitor.get_refresh_rate() == freq, 'Unexpected monitor frequency'
    if verbose:
        print(f'monitor {position}: {geometry.width}x{geometry.height} frequency {monitor.get_refresh_rate()} scale {monitor.get_scale_factor()} model \'{monitor.get_model()}\' connector \'{monitor.get_connector()}\'')

def run_commands():
    try:
        launch_observer()

        add_monitor('0', width=100, height=100, scale=1, freq=60)
        expect_monitors_changed(0, 0, 1, 10000)
        expect_monitor (position=0, width=100, height=100, scale=1, freq=60000)

        add_monitor('1', width=1024, height=768, scale=1, freq=144)
        expect_monitors_changed(1, 0, 1, 10000)
        expect_monitor (position=1, width=1024, height=768, scale=1, freq=144000)

        remove_monitor('0')
        expect_monitors_changed(0, 1, 0, 11000) # mutter takes 10 seconds to remove it

        remove_monitor('1')
        expect_monitors_changed(0, 1, 0, 11000)
    except AssertionError as e:
        print(f'Error: {e}')
        terminate()

def mutter_appeared(name):
    global screen_cast
    global done
    if verbose:
        print('mutter appeared on the bus')
    screen_cast = bus.get('org.gnome.Mutter.ScreenCast',
                          '/org/gnome/Mutter/ScreenCast')
    run_commands()

    if verbose:
        print ('Done running commands, exiting...')
    done = True

def mutter_vanished():
    global done
    if screen_cast != None:
        if verbose:
            print('mutter left the bus')
        done = True

bus = SessionBus()
bus.watch_name('org.gnome.Mutter.ScreenCast', 0, mutter_appeared, mutter_vanished)

try:
    while not done:
      GLib.MainContext.default().iteration(True)
except KeyboardInterrupt:
    print('Interrupted')
