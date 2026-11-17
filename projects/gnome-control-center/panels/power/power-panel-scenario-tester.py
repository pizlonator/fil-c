#!/usr/bin/env python3

# Copyright (C) 2021 Red Hat Inc.
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import dbus
import dbusmock
import sys
import os
import fcntl
import gi
import subprocess
import time
from collections import OrderedDict
from dbusmock import DBusTestCase
from dbus.mainloop.glib import DBusGMainLoop
from consolemenu import *
from consolemenu.items import *

gi.require_version('UPowerGlib', '1.0')
gi.require_version('UMockdev', '1.0')

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import UPowerGlib
from gi.repository import UMockdev

DBusGMainLoop(set_as_default=True)


def set_nonblock(fd):
    '''Set a file object to non-blocking'''
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

def get_templates_dir():
    return os.path.join(os.path.dirname(__file__), 'dbusmock-templates')

def get_template_path(template_name):
    return os.path.join(get_templates_dir(), template_name + '.py')

class GccDBusTestCase(DBusTestCase):
    @classmethod
    def setUpClass(klass):
        klass.mocks = OrderedDict()

        # Start system bus
        DBusTestCase.setUpClass()
        klass.test_bus = Gio.TestDBus.new(Gio.TestDBusFlags.NONE)
        klass.test_bus.up()
        os.environ['DBUS_SYSTEM_BUS_ADDRESS'] = klass.test_bus.get_bus_address()

        # Find upower
        if os.environ.get('UNDER_JHBUILD', False):
            jhbuild_prefix = os.environ['JHBUILD_PREFIX']
            klass.upowerd_path = os.path.join(jhbuild_prefix, 'libexec', 'upowerd')
            if not GLib.file_test(klass.upowerd_path, GLib.FileTest.IS_EXECUTABLE):
                klass.upowerd_path = None

        if not os.environ.get('UNDER_JHBUILD', False) or klass.upowerd_path == None:
            klass.upowerd_path = None
            with open('/usr/share/dbus-1/system-services/org.freedesktop.UPower.service') as f:
                for line in f:
                    if line.startswith('Exec='):
                        klass.upowerd_path = line.split('=', 1)[1].strip()
                        break
            assert klass.upowerd_path, 'could not determine daemon path from D-BUS .service file'

        # Start mock udev
        klass.testbed = UMockdev.Testbed.new()

        # Start ppd and logind
        klass.start_from_template('power_profiles_daemon')
        klass.start_from_template('logind')

        klass.system_bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)

    @classmethod
    def tearDownClass(klass):
        for (mock_server, mock_obj) in reversed(klass.mocks.values()):
            mock_server.terminate()
            mock_server.wait()

        DBusTestCase.tearDownClass()

    @classmethod
    def start_from_template(klass, template, params={}):
        mock_server, mock_obj = \
            klass.spawn_server_template(template,
                                        params,
                                        stdout=subprocess.PIPE)
        set_nonblock(mock_server.stdout)

        mocks = (mock_server, mock_obj)
        assert klass.mocks.setdefault(template, mocks) == mocks
        return mocks

    def get_upower_property(self, name):
        '''Get property value from UPower D-Bus interface.'''

        proxy = Gio.DBusProxy.new_sync(
            self.system_bus, Gio.DBusProxyFlags.DO_NOT_AUTO_START, None, 'org.freedesktop.UPower',
            '/org/freedesktop/UPower', 'org.freedesktop.DBus.Properties', None)
        return proxy.Get('(ss)', 'org.freedesktop.UPower', name)

    def __init__(self):
        self.devices = {}
        self.ppd = self.mocks['power_profiles_daemon'][1]

        os.environ['UMOCKDEV_DIR'] = self.testbed.get_root_dir()
        # See https://github.com/systemd/systemd/pull/21761
        # os.environ['SYSTEMD_LOG_LEVEL'] = 'debug'
        self.upowerd = subprocess.Popen([ self.upowerd_path ],
                                        env=os.environ, stdout=None,
                                        stderr=subprocess.STDOUT)

        # wait until the daemon gets online
        timeout = 100
        while timeout > 0:
            time.sleep(0.1)
            timeout -= 1
            try:
                self.get_upower_property('DaemonVersion')
                break
            except GLib.GError:
                pass
        else:
            self.fail('daemon did not start in 10 seconds')

        # self.assertEqual(self.upowerd.poll(), None, 'daemon crashed')

    def toggle_devices(self, device_types):
        for _type in device_types:
            if _type not in self.devices:
                self.devices[_type] = self.add_device(_type)
                # print('added ' + _type)
            else:
                # print('removing ' + _type)
                devs = self.devices[_type]
                devs.reverse()
                for dev in devs:
                    self.testbed.uevent(dev, 'remove')
                    self.testbed.remove_device(dev)
                del self.devices[_type]

        # out = subprocess.check_output(['upower', '--dump'],
        #               universal_newlines=True)
        # print(out)

    def add_device(self, device):
        if device == 'battery':
            dev = self.testbed.add_device('power_supply', 'BAT0', None,
                    ['type', 'Battery',
                     'present', '1',
                     'status', 'Discharging',
                     'energy_full', '60000000',
                     'energy_full_design', '80000000',
                     'energy_now', '48000000',
                     'voltage_now', '12000000',
                     'cycle_count', '250'], [])
            return [ dev ]

        elif device == '2nd-battery':
            # Not charging or discharging
            # No cycle count available
            dev = self.testbed.add_device('power_supply', 'BAT1', None,
                    ['type', 'Battery',
                     'present', '1',
                     'status', 'Not charging',
                     'energy_full', '30000000',
                     'energy_full_design', '40000000',
                     'energy_now', '20000000',
                     'voltage_now', '12000000',
                     'cycle_count', '-1'], [])
            return [ dev ]

        elif device == 'ac':
            dev = self.testbed.add_device('power_supply', 'AC', None,
                    ['type', 'Mains', 'online', '0'], [])
            return [ dev ]

        elif device == 'keyboard':
            dev = self.testbed.add_device('bluetooth',
                    'usb2/bluetooth/hci0/hci0:1',
                    None,
                    [], [])
            devs = [ dev ]

            parent = dev
            devs.append(self.testbed.add_device(
                    'input',
                    'input3/event4',
                    parent,
                    [], ['DEVNAME', 'input/event4', 'ID_INPUT_KEYBOARD', '1']))

            devs.append(self.testbed.add_device(
                    'power_supply',
                    'power_supply/hid-00:22:33:44:55:66-battery',
                    parent,
                    ['type', 'Battery',
                        'scope', 'Device',
                        'present', '1',
                        'online', '1',
                        'status', 'Discharging',
                        'capacity', '40',
                        'model_name', 'Monster Typist'],
                    []))
            return devs

        elif device == 'mouse':
            dev = self.testbed.add_device('hid',
                    '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A',
                    None,
                    [], [])
            devs = [ dev ]

            parent = dev
            devs.append(self.testbed.add_device('input',
                    '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A/input/input22',
                    parent,
                    [], ['DEVNAME', 'input/mouse3', 'ID_INPUT_MOUSE', '1']))

            devs.append(self.testbed.add_device('power_supply',
                    '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A/power_supply/hidpp_battery_3',
                    parent,
                    ['type', 'Battery',
                     'scope', 'Device',
                     'present', '1',
                     'online', '1',
                     'status', 'Discharging',
                     'capacity', '30',
                     'serial_number', '123456',
                     'model_name', 'Fancy Mouse'],
                    []))
            return devs

        elif device == 'ups':
            dev = self.testbed.add_device('usb', 'hiddev0', None, [],
                    ['DEVNAME', 'null',
                     'UPOWER_VENDOR', 'APC',
                     'UPOWER_BATTERY_TYPE', 'ups',
                     'UPOWER_FAKE_DEVICE', '1',
                     'UPOWER_FAKE_HID_CHARGING', '0',
                     'UPOWER_FAKE_HID_PERCENTAGE', '70'])
            return [ dev ]

        print('Unhandled device')
        return None

    def cycle_degraded(self):
        perf = self.ppd.Get('net.hadess.PowerProfiles', 'PerformanceDegraded')
        if perf == '':
            perf = 'lap-detected'
        elif perf == 'lap-detected':
            perf = 'high-operating-temperature'
        elif perf == 'high-operating-temperature':
            perf = ''
        mock_iface = dbus.Interface(self.ppd, dbusmock.MOCK_IFACE)
        mock_iface.UpdateProperties('net.hadess.PowerProfiles', {
                'PerformanceDegraded': dbus.String(perf, variant_level=1)
        })

    def start_menu(self):
        menu = ConsoleMenu("Power Panel", "Scenario Tester", clear_screen = False)
        function_item = FunctionItem("Toggle Keyboard", self.toggle_devices, [["keyboard"]])
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle Mouse", self.toggle_devices, [["mouse"]])
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle UPS", self.toggle_devices, [["ups"]])
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle laptop battery", self.toggle_devices, [['ac', 'battery']])
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle 2nd battery", self.toggle_devices, [['2nd-battery']])
        menu.append_item(function_item)

        function_item = FunctionItem("Cycle degraded performance", self.cycle_degraded, [])
        menu.append_item(function_item)

        menu.start(show_exit_option=False)

    def wrap_call(self):
        os.environ['GSETTINGS_BACKEND'] = 'memory'

        wrapper = os.environ.get('META_DBUS_RUNNER_WRAPPER')
        args = ['gnome-control-center', 'power']
        if wrapper == 'gdb':
            args = ['gdb', '-ex', 'r', '-ex', 'bt full', '--args'] + args
        elif wrapper:
            args = wrapper.split(' ') + args

        p = subprocess.Popen(args, env=os.environ)
        p.wait()

if __name__ == '__main__':
    if 'umockdev' not in os.environ.get('LD_PRELOAD', ''):
        os.execvp('umockdev-wrapper', ['umockdev-wrapper'] + sys.argv)

    GccDBusTestCase.setUpClass()
    test_case = GccDBusTestCase()
    test_case.start_menu()
    try:
        test_case.wrap_call()
    finally:
        GccDBusTestCase.tearDownClass()
