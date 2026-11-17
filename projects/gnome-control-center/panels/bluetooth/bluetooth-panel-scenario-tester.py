#!/usr/bin/env python3

# Copyright (C) 2021 Red Hat Inc.
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import dbus
import sys
import os
import fcntl
import gi
import subprocess
import time
from collections import OrderedDict
from dbusmock import DBusTestCase, mockobject
from dbus.mainloop.glib import DBusGMainLoop
from consolemenu import *
from consolemenu.items import *

from gi.repository import Gio
from gi.repository import GLib

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

        # Start session bus
        klass.session_test_bus = Gio.TestDBus.new(Gio.TestDBusFlags.NONE)
        klass.session_test_bus.up()
        os.environ['DBUS_SESSION_BUS_ADDRESS'] = klass.session_test_bus.get_bus_address()

        # process = subprocess.Popen(['gdbus', 'monitor', '--session', '--dest', 'org.gnome.SettingsDaemon.Rfkill'])
        # process = subprocess.Popen(['gdbus', 'monitor', '--system', '--dest', 'org.bluez'])

        # Start bluez and gsd-rfkill
        klass.start_from_template('bluez5')
        klass.start_from_local_template(
            'gsd_rfkill', {'templates-dir': get_templates_dir()})

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

    @classmethod
    def start_from_local_template(klass, template_file_name, params={}):
        template = get_template_path(template_file_name)
        ret = klass.start_from_template(template, params)
        klass.mocks.setdefault(template_file_name, ret)
        return ret

    def __init__(self):
        self.devices = {}
        self.rfkill = self.mocks['gsd_rfkill'][1]

        self.bluez_mock = self.mocks['bluez5'][1]
        self.hci0_powered = True
        self.hci0_plugged_in = True
        self.add_adapter()
        bus = dbus.SystemBus()
        self.hci0_props = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'), 'org.freedesktop.DBus.Properties')

    def adapter_exists(self):
        try:
            self.get_dbus(True).get_object('org.bluez', '/org/bluez/hci0').Get('org.bluez.Adapter1', 'Name')
        except:
            return False
        return True

    def add_adapter(self):
        if self.adapter_exists():
            return
        self.bluez_mock.AddAdapter('hci0', 'hci0')
        adapter = self.get_dbus(True).get_object('org.bluez', '/org/bluez/hci0')
        adapter.AddProperties('org.bluez.Adapter1',
                {'Blocked': dbus.Boolean(not self.hci0_powered, variant_level=1)})
        adapter.UpdateProperties('org.bluez.Adapter1',
                {'Powered': dbus.Boolean(self.hci0_powered, variant_level=1)})
        self.devices = []
        self.add_device('hci0', '22:33:44:55:66:77', "Bastienʼs mouse", True, 0x580, 'input-mouse')
        self.add_device('hci0', '22:33:44:55:66:78', 'Bloutouf keyboard & keys', True, 0x540, 'input-keyboard')
        self.add_device('hci0', '60:8B:0E:55:66:79', 'iPhoone 19S', True, 0x20C, 'phone')
        # Uncategorised audio device
        self.add_device('hci0', '22:33:44:55:66:79', 'MEGA Speakers', True, 0x200400, 'audio-card')
        self.add_device('hci0', '22:33:44:55:66:80', 'Ski-bi dibby dib yo da dub dub Yo da dub dub Ski-bi dibby dib yo da dub dub Yo da dub dub (I\'m the Scatman) Ski-bi dibby dib yo da dub dub Yo da dub dub Ski-bi dibby dib yo da dub dub Yo da dub dub Ba-da-ba-da-ba-be bop bop bodda bope Bop ba bodda bope Be bop ba bodda bope Bop ba bodda Ba-da-ba-da-ba-be bop ba bodda bope Bop ba bodda bope Be bop ba bodda bope Bop ba bodda bope', True, 0x80C, '')
        self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHasAirplaneMode', dbus.Boolean(True))

    def remove_adapter(self):
        if not self.adapter_exists():
            return
        for dev in self.devices:
            adapter = self.get_dbus(True).get_object('org.bluez', '/org/bluez/hci0')
            adapter.RemoveDevice(dev)
        self.devices = []
        self.bluez_mock.RemoveAdapter('hci0')
        if self.rfkill.Get('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHardwareAirplaneMode') == 0:
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHasAirplaneMode', dbus.Boolean(False))

    def add_device(self, adapter, address, name, paired, klass, icon):
        dev_path = self.bluez_mock.AddDevice(adapter, address, name)
        dev = self.get_dbus(True).get_object('org.bluez', str(dev_path))
        dev.UpdateProperties('org.bluez.Device1',
                {'Paired': dbus.Boolean(paired, variant_level=1),
                 'Class': dbus.UInt32(klass, variant_level=1),
                 'Icon': dbus.String(icon, variant_level=1)})
        self.devices.append(dev)

    def get_rfkill_prop(self, prop_name):
        return self.rfkill.Get('org.gnome.SettingsDaemon.Rfkill', prop_name)

    def toggle_hw_rfkill(self):
        if self.rfkill.Get('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHardwareAirplaneMode') == 0:
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHardwareAirplaneMode', dbus.Boolean(True))
            if self.adapter_exists():
                self.remove_adapter()
        else:
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothHardwareAirplaneMode', dbus.Boolean(False))
            if not self.adapter_exists():
                self.add_adapter()

    def set_unpowered(self):
        if self.hci0_powered:
            print('hci0 will now default to unpowered')
            self.hci0_powered = False
        else:
            print('hci0 will now default to powered')
            self.hci0_powered = True

    def unplug_default_adapter(self):
        if self.hci0_plugged_in:
            print('default adapter is unplugged')
            self.hci0_plugged_in = False
            self.remove_adapter()
        else:
            print('default adapter is plugged in')
            self.hci0_plugged_in = True
            self.add_adapter()

    def toggle_airplane_mode(self):
        if self.rfkill.Get('org.gnome.SettingsDaemon.Rfkill', 'AirplaneMode') == 0:
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'AirplaneMode', dbus.Boolean(True))
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', dbus.Boolean(True))
        else:
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'AirplaneMode', dbus.Boolean(False))
            self.rfkill.Set('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', dbus.Boolean(False))

    def start_menu(self):
        menu = ConsoleMenu("Bluetooth Panel", "Scenario Tester", clear_screen = False)
        function_item = FunctionItem("Toggle Bluetooth hardware rfkill", self.toggle_hw_rfkill)
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle default adapter unpowered", self.set_unpowered)
        menu.append_item(function_item)

        function_item = FunctionItem("Unplug/plug default adapter", self.unplug_default_adapter)
        menu.append_item(function_item)

        function_item = FunctionItem("Toggle airplane mode", self.toggle_airplane_mode)
        menu.append_item(function_item)

        menu.start(show_exit_option=False)

    def wrap_call(self):
        os.environ['GSETTINGS_BACKEND'] = 'memory'

        wrapper = os.environ.get('META_DBUS_RUNNER_WRAPPER')
        args = ['gnome-control-center', '-v', 'bluetooth']
        if wrapper == 'gdb':
            args = ['gdb', '-ex', 'r', '-ex', 'bt full', '--args'] + args
        elif wrapper:
            args = wrapper.split(' ') + args

        p = subprocess.Popen(args, env=os.environ)
        p.wait()

if __name__ == '__main__':
    #if 'umockdev' not in os.environ.get('LD_PRELOAD', ''):
    #    os.execvp('umockdev-wrapper', ['umockdev-wrapper'] + sys.argv)

    GccDBusTestCase.setUpClass()
    test_case = GccDBusTestCase()
    test_case.start_menu()
    try:
        test_case.wrap_call()
    finally:
        GccDBusTestCase.tearDownClass()
