#!/usr/bin/env python3

# Copyright (C) 2021 Red Hat Inc.
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import gi
import subprocess
import sys

gi.require_version('UMockdev', '1.0')
from gi.repository import UMockdev

def setup_devices(testbed):
    dev = testbed.add_device('hid',
            '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A',
            None,
            [], [])

    parent = dev
    testbed.add_device('input',
            '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A/input/input3',
            parent,
            ['name', 'Wacom Cintiq 24HD Pad'],
            ['DEVNAME', 'input/event3',
             'ID_INPUT', '1',
             'ID_INPUT_TABLET', '1',
             'ID_INPUT_TABLET_PAD', '1',
             'ID_VENDOR_ID', '0x56a',
             'ID_MODEL_ID', '0x0f4',
             'ID_INPUT_WIDTH_MM', '50',
             'ID_INPUT_HEIGHT_MM', '40',
             'PRODUCT', '3/56a/f4/100',
             'LIBINPUT_DEVICE_GROUP', '3/56a/f4:usb-0000:00:14.0-5'])
    testbed.add_device('input',
            '/devices/pci0000:00/0000:00:14.0/usb3/3-10/3-10:1.2/0003:046D:C52B.0009/0003:046D:4101.000A/input/input4',
            parent,
            ['name', 'Wacom Cintiq 24HD Pen'],
            ['DEVNAME', 'input/event4',
             'ID_INPUT', '1',
             'ID_INPUT_TABLET', '1',
             'ID_VENDOR_ID', '0x56a',
             'ID_MODEL_ID', '0x0f4',
             'ID_INPUT_WIDTH_MM', '50',
             'ID_INPUT_HEIGHT_MM', '40',
             'PRODUCT', '3/56a/f4/100',
             'LIBINPUT_DEVICE_GROUP', '3/56a/f4:usb-0000:00:14.0-5'])

def wrap_call(testbed):
    os.environ['GSETTINGS_BACKEND'] = 'memory'
    os.environ['UMOCKDEV_DIR'] = testbed.get_root_dir()

    wrapper = os.environ.get('WRAPPER')
    args = ['gnome-control-center', '-v', 'wacom']
    if wrapper == 'gdb':
        args = ['gdb', '-ex', 'r', '-ex', 'bt full', '--args'] + args
    elif wrapper:
        args = wrapper.split(' ') + args

    print(os.environ)

    p = subprocess.Popen(args, env=os.environ)
    p.wait()

if __name__ == '__main__':
    if 'umockdev' not in os.environ.get('LD_PRELOAD', ''):
        os.execvp('umockdev-wrapper', ['umockdev-wrapper'] + sys.argv)

    # Start mock udev
    testbed = UMockdev.Testbed.new()
    setup_devices(testbed)
    wrap_call(testbed)
