'''gsd-rfkill mock template

This creates the expected methods and properties of the main
org.gnome.SettingsDaemon.Rfkill object.
'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Bastien Nocera'
__copyright__ = '(c) 2022, Red Hat Inc.'

import dbus
import os
from dbusmock import mockobject

BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill'
MAIN_OBJ = '/org/gnome/SettingsDaemon/Rfkill'
MAIN_IFACE = 'org.gnome.SettingsDaemon.Rfkill'
SYSTEM_BUS = False

ADAPTER_IFACE = 'org.bluez.Adapter1'

def rfkill_changed(*args, **kwargs):
    [iface, changed, _invalidated] = args

    rfkill = mockobject.objects[MAIN_OBJ]
    adapter = dbus.bus.BusConnection(os.environ['DBUS_SYSTEM_BUS_ADDRESS']).get_object('org.bluez', '/org/bluez/hci0')
    try:
        adapter.Get(ADAPTER_IFACE, 'Name')
    except:
        adapter = None

    if 'BluetoothAirplaneMode' in changed:
        if adapter and rfkill.props[MAIN_IFACE]['BluetoothAirplaneMode'] == 1:
            adapter.UpdateProperties(ADAPTER_IFACE,
                    {'Powered': dbus.Boolean(False),
                     'Blocked': dbus.Boolean(True)})
        elif adapter:
            adapter.UpdateProperties(ADAPTER_IFACE,
                    {'Blocked': dbus.Boolean(False)})
    if 'BluetoothHardwareAirplaneMode' in changed:
        if rfkill.props[MAIN_IFACE]['BluetoothAirplaneMode'] == 0:
            rfkill.Set(MAIN_IFACE, 'BluetoothAirplaneMode', dbus.Boolean(False))

def load(mock, parameters):
    # Loaded!
    mock.loaded = True

    props = {
        'AirplaneMode': parameters.get('AirplaneMode', dbus.Boolean(False)),
        'HardwareAirplaneMode': parameters.get('HardwareAirplaneMode', dbus.Boolean(False)),
        'HasAirplaneMode': parameters.get('HasAirplaneMode', dbus.Boolean(True)),
        # True if not desktop, server, vm or container
        'ShouldShowAirplaneMode': parameters.get('ShouldShowAirplaneMode', dbus.Boolean(True)),
        'BluetoothAirplaneMode': parameters.get('BluetoothAirplaneMode', dbus.Boolean(False)),
        'BluetoothHardwareAirplaneMode': parameters.get('BluetoothAirplaneMode', dbus.Boolean(False)),
        'BluetoothHasAirplaneMode': parameters.get('BluetoothHasAirplaneMode', dbus.Boolean(True)),
        'WwanAirplaneMode': parameters.get('WwanAirplaneMode', dbus.Boolean(False)),
        'WwanHardwareAirplaneMode': parameters.get('WwanHardwareAirplaneMode', dbus.Boolean(False)),
        'WwanHasAirplaneMode': parameters.get('WwanHasAirplaneMode', dbus.Boolean(False)),
    }
    mock.AddProperties(MAIN_IFACE, dbus.Dictionary(props, signature='sv'))

    rfkill = mockobject.objects[MAIN_OBJ]
    rfkill.hci0_power = True

    session_bus = dbus.SessionBus()
    session_bus.add_signal_receiver(rfkill_changed,
            signal_name='PropertiesChanged',
            path=MAIN_OBJ,
            dbus_interface='org.freedesktop.DBus.Properties')
