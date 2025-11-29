import sys
import os
import subprocess
import gi

gi.require_version('Gdk', '4.0')
gi.require_version('Gtk', '4.0')

from gi.repository import GLib, GObject, Gdk, Gtk
from pydbus import SessionBus

verbose = True

remote_desktop = None
screen_cast = None
session = None
stream_path = None
done = False

def terminate():
    sys.exit(1)

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

display = None
window = None
entry = None
expected_change = None

def key_pressed_cb (controller, keyval, keycode, state):
    global expected_change
    global loop

    if verbose:
        print(f'got key press: {keyval}, state {state}')
    assert expected_change != None, 'Unexpected key press'
    assert expected_change['type'] == 'press', 'Key press event expected'
    assert keyval == expected_change['keyval'], 'Unexpected keyval in key press event'
    assert state == expected_change['state'], 'Unexpected state in key press event'

    expected_change = None
    loop.quit()

def key_released_cb (controller, keyval, keycode, state):
    global expected_change
    global loop

    if verbose:
        print(f'got key release: {keyval}, state {state}')
    assert expected_change != None, 'Unexpected key release'
    assert expected_change['type'] == 'release', 'Key release event expected'
    assert keyval == expected_change['keyval'], 'Unexpected keyval in key release event'
    assert state == expected_change['state'], 'Unexpected state in key release event'

    expected_change = None
    loop.quit()

def motion_cb (controller, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got motion: {x}, {y}')
    if expected_change != None:
        assert expected_change['type'] == 'motion', 'Motion event expected'
        assert x == expected_change['x'], 'Unexpected x coord in motion event'
        assert y == expected_change['y'], 'Unexpected y coord in motion event'
        expected_change = None
    loop.quit()

def enter_cb (controller, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got enter: {x}, {y}')
    assert expected_change != None, 'Unexpected enter'
    assert expected_change['type'] == 'enter', 'Enter event expected'
    assert x == expected_change['x'], 'Unexpected x coord in enter event'
    assert y == expected_change['y'], 'Unexpected y coord in enter event'

    expected_change = None
    loop.quit()

def pressed_cb(controller, n, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got pressed')
    assert expected_change != None, 'Unexpected event'
    assert expected_change['type'] == 'press', 'Button press expected'
    assert expected_change['button'] == controller.get_current_button(), 'Unexpected button pressed'
    assert x == expected_change['x'], 'Unexpected x coord in motion event'
    assert y == expected_change['y'], 'Unexpected y coord in motion event'

    expected_change = None
    loop.quit()

def released_cb(controller, n, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got released')
    assert expected_change != None, 'Unexpected event'
    assert expected_change['type'] == 'release', 'Button release expected'

    expected_change = None
    loop.quit()

def expect_key_press(keyval, state, timeout):
    global expected_change
    expected_change = {
      'type' : 'press',
      'keyval' : keyval,
      'state' : state
    }
    wait(timeout)
    assert expected_change == None, 'Expected event did not happen'

def expect_key_release(keyval, state, timeout):
    global expected_change
    expected_change = {
      'type' : 'release',
      'keyval' : keyval,
      'state' : state
    }
    wait(timeout)
    assert expected_change == None, 'Expected event did not happen'

def expect_motion(x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'motion',
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, 'Expected event did not happen'

def expect_enter(x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'enter',
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, 'Expected event did not happen'

def expect_button_press(button, x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'press',
      'button' : button,
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, 'Button press did not arrive'

def expect_button_release(button, x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'release',
      'button' : button,
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, 'Button release did not arrive'

def got_active(object, pspec):
    global loop
    object.disconnect_by_func(got_active)
    loop.quit()

def launch_observer():
    global display
    global window

    if verbose:
        print('launch observer')

    if display == None:
        display = Gdk.Display.open(os.getenv('WAYLAND_DISPLAY'))

    window = Gtk.Window.new()

    controller = Gtk.EventControllerKey.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('key-pressed', key_pressed_cb)
    controller.connect('key-released', key_released_cb)
    window.add_controller(controller)

    controller = Gtk.EventControllerMotion.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('enter', enter_cb)
    controller.connect('motion', motion_cb)
    window.add_controller(controller)

    controller = Gtk.GestureClick.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('pressed', pressed_cb)
    controller.connect('released', released_cb)
    window.add_controller(controller)

    window.connect('notify::is-active', got_active)
    window.maximize()
    window.present()

    wait(2000)

    assert window.is_active(), 'Observer not active'
    assert window.get_width() == 1024, 'Window not maximized'
    assert window.get_height() == 768, 'Window not maximized'

    # we need to wait out the map animation, or pointer coords will be off
    wait(1000)

def launch_entry():
    global display
    global window
    global entry

    if verbose:
        print('launch entry')

    if display == None:
        display = Gdk.Display.open(os.getenv('WAYLAND_DISPLAY'))

    window = Gtk.Window.new()

    entry = Gtk.Entry.new()

    window.set_child(entry)

    window.connect('notify::is-active', got_active)
    window.maximize()
    window.present()

    wait(500)

    assert window.is_active(), "Observer not active"
    assert window.get_width() == 1024, "Window not maximized"
    assert window.get_height() == 768, "Window not maximized"

    # we need to wait out the map animation, or pointer coords will be off
    wait(1000)

def stop_observer():
    global window
    window.destroy()
    window = None

def expect_entry_text(text):
    assert text == entry.get_text(), 'Unexpected entry text: ' + entry.get_text()

def key_press(keyval):
    if verbose:
        print(f'press key {keyval}')
    session.NotifyKeyboardKeysym(keyval, True)

def key_release(keyval):
    if verbose:
        print(f'release key {keyval}')
    session.NotifyKeyboardKeysym(keyval, False)

buttons = {
    1 : 0x110,
    2 : 0x111,
    3 : 0x112
}

def button_press(button):
    if verbose:
        print(f'press button {button}')
    session.NotifyPointerButton(buttons[button], True)

def button_release(button):
    if verbose:
        print(f'release button {button}')
    session.NotifyPointerButton(buttons[button], False)

def pointer_move(x, y):
    if verbose:
        print(f'pointer move {x} {y}')
    session.NotifyPointerMotionAbsolute(stream_path, x, y)

def basic_keyboard_tests():
    try:
        if verbose:
            print('Starting basic keyboard tests')

        launch_observer()

        key_press(Gdk.KEY_a)
        expect_key_press(keyval=Gdk.KEY_a, state=0, timeout=100)

        key_release(Gdk.KEY_a)
        expect_key_release(keyval=Gdk.KEY_a, state=0, timeout=100)

        key_press(Gdk.KEY_Control_L)
        expect_key_press(keyval=Gdk.KEY_Control_L, state=0, timeout=100)

        key_press(Gdk.KEY_x)
        expect_key_press(keyval=Gdk.KEY_x, state=Gdk.ModifierType.CONTROL_MASK, timeout=100)

        key_release(Gdk.KEY_Control_L)
        expect_key_release(keyval=Gdk.KEY_Control_L, state=Gdk.ModifierType.CONTROL_MASK, timeout=100)

        key_release(Gdk.KEY_x)
        expect_key_release(keyval=Gdk.KEY_x, state=0, timeout=100)

        stop_observer()
    except AssertionError as e:
        print(f'Error in basic_keyboard_tests: {e}')
        terminate()

def quick_typing_test():
    try:
        launch_entry()

        key_press(Gdk.KEY_T)
        key_release(Gdk.KEY_T)
        key_press(Gdk.KEY_e)
        key_release(Gdk.KEY_e)
        key_press(Gdk.KEY_s)
        key_release(Gdk.KEY_s)
        key_press(Gdk.KEY_t)
        key_release(Gdk.KEY_t)

        wait(100)
        expect_entry_text('Test')

        stop_observer()
    except AssertionError as e:
        print(f'Error in quick_typing_test: {e}')
        terminate()

def basic_pointer_tests():
    try:
        if verbose:
            print('Starting basic pointer tests')

        pointer_move(-100.0, -100.0)
        launch_observer()

        # observer window is maximized, so window coords == global coords
        pointer_move(500.0, 300.0)
        expect_enter(x=500, y=300, timeout=200)

        pointer_move(400.0, 200.0)
        expect_motion(x=400, y=200, timeout=200)

        button_press(1)
        expect_button_press(button=1, x=400, y=200, timeout=200)

        pointer_move(220.0, 200.0)
        expect_motion(x=220, y=200, timeout=200)

        button_release(1)
        expect_button_release(button=1, x=220, y=200, timeout=200)

        stop_observer()
    except AssertionError as e:
        print(f'Error in basic_pointer_tests: {e}')
        terminate()

ds_window = None
ds = None

def drag_begin(controller, drag):
    global expected_change
    global loop

    if verbose:
        print('got drag begin')
    assert expected_change != None, 'Unexpected drag begin'
    assert expected_change['type'] == 'drag', 'Drag begin expected'

    expected_change = None
    loop.quit()

def launch_drag_source(value):
    global display
    global ds_window
    global ds

    if verbose:
        print('launch drag source')

    if display == None:
        display = Gdk.Display.open(os.getenv('WAYLAND_DISPLAY'))

    ds_window = Gtk.Window.new()
    ds_window.set_title('Drag Source')

    ds = Gtk.DragSource.new()
    ds.set_content(Gdk.ContentProvider.new_for_value(value))
    ds_window.add_controller(ds)
    ds.connect('drag-begin', drag_begin)

    controller = Gtk.GestureClick.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('pressed', pressed_cb)
    controller.connect('released', released_cb)
    ds_window.add_controller(controller)

    ds_window.connect('notify::is-active', got_active)
    ds_window.maximize()
    ds_window.present()

    wait(2000)

    assert ds_window.is_active(), 'drag source not active'
    assert ds_window.get_width() == 1024, 'Window not maximized'
    assert ds_window.get_height() == 768, 'Window not maximized'

    # we need to wait out the map animation, or pointer coords will be off
    wait(1000)

def stop_drag_source():
    global ds_window
    ds_window.destroy()
    ds_window = None

dt_window = None

def do_drop(controller, value, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got drop {value}')
    assert expected_change != None, 'Unexpected drop begin'
    assert expected_change['type'] == 'drop', 'Drop expected'
    assert expected_change['value'] == value, 'Unexpected value dropped'

    expected_change = None
    loop.quit()

def launch_drop_target():
    global display
    global dt_window

    if verbose:
        print('launch drop target')

    if display == None:
        display = Gdk.Display.open(os.getenv('WAYLAND_DISPLAY'))

    dt_window = Gtk.Window.new()
    dt_window.set_title('Drop Target')

    controller = Gtk.DropTarget.new(GObject.TYPE_STRING, Gdk.DragAction.COPY)
    dt_window.add_controller(controller)
    controller.connect('drop', do_drop)

    dt_window.connect('notify::is-active', got_active)
    dt_window.maximize()
    dt_window.present()

    wait(2000)

    assert dt_window.is_active(), 'drop target not active'
    assert dt_window.get_width() == 1024, 'Window not maximized'
    assert dt_window.get_height() == 768, 'Window not maximized'

    # we need to wait out the map animation, or pointer coords will be off
    wait(1000)

def stop_drop_target():
    global dt_window
    dt_window.destroy()
    dt_window = None

def expect_drag(timeout):
    global expected_change
    expected_change = {
      'type' : 'drag',
    }
    wait(timeout)
    assert expected_change == None, 'DND operation not started'

def expect_drop(value, timeout):
    global expected_change
    expected_change = {
      'type' : 'drop',
      'value' : value
    }
    wait(timeout)
    assert expected_change == None, 'Drop has not happened'

def dnd_tests():
    try:
        if verbose:
            print('Starting dnd tests')

        pointer_move(-100, -100)

        launch_drag_source('abc')
        wait(100);

        pointer_move(100, 100)
        wait(100);
        button_press(1)
        expect_button_press(button=1, x=100, y=100, timeout=300)
        # need to wait out the MIN_TIME_TO_DND
        wait(150)

        pointer_move(120, 150)
        expect_drag(timeout=2000)

        launch_drop_target()
        wait(100);
        button_release(1)
        expect_drop('abc', timeout=2000)

        stop_drop_target()
        stop_drag_source()
    except AssertionError as e:
        print(f'Error in dnd_tests: {e}')
        terminate()

def session_closed_cb():
    print('Session closed')

def run_commands():
    basic_keyboard_tests()
    basic_pointer_tests()
    dnd_tests()
    quick_typing_test()

def mutter_appeared(name):
    global remote_desktop
    global session
    global stream_path
    global done

    if verbose:
        print('mutter appeared on the bus')

    remote_desktop = bus.get('org.gnome.Mutter.RemoteDesktop',
                             '/org/gnome/Mutter/RemoteDesktop')
    device_types = remote_desktop.Get('org.gnome.Mutter.RemoteDesktop', 'SupportedDeviceTypes')
    assert device_types & 1 == 1, 'No keyboard'
    assert device_types & 2 == 2, 'No pointer'

    screen_cast = bus.get('org.gnome.Mutter.ScreenCast',
                          '/org/gnome/Mutter/ScreenCast')

    session_path = remote_desktop.CreateSession()
    session = bus.get('org.gnome.Mutter.RemoteDesktop', session_path)
    session.onClosed = session_closed_cb

    screen_cast_session_path = screen_cast.CreateSession({ 'remote-desktop-session-id' : GLib.Variant('s', session.SessionId)})
    screen_cast_session = bus.get('org.gnome.Mutter.ScreenCast', screen_cast_session_path)

    stream_path = screen_cast_session.RecordMonitor('Meta-0', {})
    session.Start()

    # work around lack of initial devices
    key_press(Gdk.KEY_Control_L)
    key_release(Gdk.KEY_Control_L)
    pointer_move(-100, -100)

    run_commands()

    session.Stop()

    if verbose:
        print('Done running commands, exiting...')
    done = True

def mutter_vanished():
    global done
    if remote_desktop != None:
        if verbose:
            print('mutter left the bus')
        done = True

bus = SessionBus()
bus.watch_name('org.gnome.Mutter.RemoteDesktop', 0, mutter_appeared, mutter_vanished)

try:
    while not done:
      GLib.MainContext.default().iteration(True)
except KeyboardInterrupt:
    print('Interrupted')
