#!/usr/bin/env python3
# -*- coding: utf-8
# vim: set expandtab shiftwidth=4:
# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil -*- */
#
# Copyright © 2018 Red Hat, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the 'Software'),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
#
# Measures the relative motion between touch events (based on slots)
#
# Input is a libinput record yaml file

from dataclasses import dataclass, field, replace
from enum import Enum

import argparse
import math
import sys
import yaml
import libevdev


COLOR_RESET = "\x1b[0m"
COLOR_RED = "\x1b[6;31m"
COLOR_BLUE = "\x1b[6;34m"
COLOR_GREEN = "\x1b[6;32m"


class SlotFormatter:
    def __init__(
        self,
        is_absolute=False,
        resolution=None,
        threshold=None,
        ignore_below=None,
        show_distance=False,
        pressure_thresholds=(0, 0),
    ):
        self.threshold = threshold
        self.ignore_below = ignore_below
        self.resolution = resolution
        self.is_absolute = is_absolute
        self.show_distance = show_distance
        self.pressure_thresholds = pressure_thresholds
        self.slots = []
        self.have_data = False
        self.filtered = False

        self.width = 35 if show_distance else 16

    def __str__(self):
        return " | ".join(self.slots)

    def format_slot(self, slot):
        if slot.state == SlotState.BEGIN:
            self.slots.append("+++++++".center(self.width))
            self.have_data = True
        elif slot.state == SlotState.END:
            self.slots.append("-------".center(self.width))
            self.have_data = True
        elif slot.state == SlotState.NONE:
            self.slots.append(("*" * (self.width - 2)).center(self.width))
        elif not slot.dirty:
            self.slots.append(" ".center(self.width))
        else:
            if self.resolution is not None:
                delta = Point(
                    slot.delta.x / self.resolution[0],
                    slot.delta.y / self.resolution[1],
                )
                distx = abs(slot.position.x - slot.origin.x)
                disty = abs(slot.position.y - slot.origin.y)
                distance = Point(
                    distx / self.resolution[0],
                    disty / self.resolution[1],
                )
            else:
                delta = Point(slot.delta.x, slot.delta.y)
                distance = Point(
                    abs(slot.position.x - slot.origin.x),
                    abs(slot.position.y - slot.origin.y),
                )

            if delta.x != 0 and delta.y != 0:
                t = math.atan2(delta.x, delta.y)
                t += math.pi  # in [0, 2pi] range now

                if t == 0:
                    t = 0.01
                else:
                    t = t * 180.0 / math.pi

                directions = ["↖↑", "↖←", "↙←", "↙↓", "↓↘", "→↘", "→↗", "↑↗"]
                direction = directions[int(t / 45)]
            elif delta.y == 0:
                if delta.x < 0:
                    direction = "←←"
                else:
                    direction = "→→"
            else:
                if delta.y < 0:
                    direction = "↑↑"
                else:
                    direction = "↓↓"

            color = COLOR_RESET
            reset = COLOR_RESET
            if not self.is_absolute:
                if (
                    self.pressure_thresholds[1] > 0
                    and slot.pressure > self.pressure_thresholds[1]
                ):
                    color = COLOR_GREEN
                    reset = COLOR_RESET
                elif (
                    self.pressure_thresholds[0] > 0
                    and slot.pressure > self.pressure_thresholds[0]
                ):
                    color = COLOR_BLUE
                    reset = COLOR_RESET

                if self.ignore_below is not None or self.threshold is not None:
                    dist = math.hypot(delta.x, delta.y)
                    if self.ignore_below is not None and dist < self.ignore_below:
                        self.slots.append(" ".center(self.width))
                        self.filtered = True
                        return
                    if self.threshold is not None and dist >= self.threshold:
                        color = COLOR_RED
                        reset = COLOR_RESET

                if isinstance(delta.x, int) and isinstance(delta.y, int):
                    coords = f"{delta.x:+4d}/{delta.y:+4d}"
                else:
                    coords = f"{delta.x:+3.2f}/{delta.y:+03.2f}"

                if self.show_distance:
                    hypot = math.hypot(distance.x, distance.y)
                    distance = (
                        f"dist: ({distance.x:3.1f}/{distance.y:3.1f}, {hypot:3.1f})"
                    )
                else:
                    distance = ""

                components = [
                    f"{direction}",
                    f"{color}",
                    coords,
                    distance,
                    f"{reset}",
                ]

                string = " ".join(c for c in components if c)
            else:
                x, y = slot.position.x, slot.position.y
                string = "{} {}{:4d}/{:4d}{}".format(direction, color, x, y, reset)
            self.have_data = True
            self.slots.append(string.ljust(self.width + len(color) + len(reset)))


class SlotState(Enum):
    NONE = 0
    BEGIN = 1
    UPDATE = 2
    END = 3


@dataclass
class Point:
    x: float = 0.0
    y: float = 0.0


@dataclass
class Slot:
    index: int
    state: SlotState = SlotState.NONE
    position: Point = field(default_factory=Point)
    delta: Point = field(default_factory=Point)
    origin: Point = field(default_factory=Point)
    pressure: int = 0
    used: bool = False
    dirty: bool = False


def main(argv):
    global COLOR_RESET
    global COLOR_RED
    global COLOR_BLUE
    global COLOR_GREEN

    slots = []
    xres, yres = 1, 1

    parser = argparse.ArgumentParser(
        description="Measure delta between event frames for each slot"
    )
    parser.add_argument(
        "--use-mm", action="store_true", help="Use mm instead of device deltas"
    )
    parser.add_argument(
        "--show-distance",
        action="store_true",
        help="Show the absolute distance relative to the first position",
    )
    parser.add_argument(
        "--use-st",
        action="store_true",
        help="Use ABS_X/ABS_Y instead of ABS_MT_POSITION_X/Y",
    )
    parser.add_argument(
        "--use-absolute",
        action="store_true",
        help="Use absolute coordinates, not deltas",
    )
    parser.add_argument(
        "path", metavar="recording", nargs=1, help="Path to libinput-record YAML file"
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=None,
        help="Mark any delta above this threshold",
    )
    parser.add_argument(
        "--ignore-below",
        type=float,
        default=None,
        help="Ignore any delta below this threshold",
    )
    parser.add_argument(
        "--pressure-min",
        type=int,
        default=0,
        help="Highlight touches above this pressure minimum",
    )
    parser.add_argument(
        "--pressure-max",
        type=int,
        default=0,
        help="Highlight touches below this pressure maximum",
    )
    args = parser.parse_args()

    if not sys.stdout.isatty():
        COLOR_RESET = ""
        COLOR_RED = ""
        COLOR_GREEN = ""
        COLOR_BLUE = ""

    yml = yaml.safe_load(open(args.path[0]))
    device = yml["devices"][0]
    absinfo = device["evdev"]["absinfo"]
    try:
        nslots = absinfo[libevdev.EV_ABS.ABS_MT_SLOT.value][1] + 1
    except KeyError:
        args.use_st = True

    if args.use_st:
        nslots = 1

    slots = [Slot(i) for i in range(0, nslots)]
    slots[0].used = True

    if args.use_mm:
        xres = 1.0 * absinfo[libevdev.EV_ABS.ABS_X.value][4]
        yres = 1.0 * absinfo[libevdev.EV_ABS.ABS_Y.value][4]
        if not xres or not yres:
            print("Error: device doesn't have a resolution, cannot use mm")
            sys.exit(1)

    if args.use_st:
        print("Warning: slot coordinates on FINGER/DOUBLETAP change may be incorrect")
        slots[0].used = True

    slot = 0
    last_time = None
    tool_bits = {
        libevdev.EV_KEY.BTN_TOUCH: 0,
        libevdev.EV_KEY.BTN_TOOL_DOUBLETAP: 0,
        libevdev.EV_KEY.BTN_TOOL_TRIPLETAP: 0,
        libevdev.EV_KEY.BTN_TOOL_QUADTAP: 0,
        libevdev.EV_KEY.BTN_TOOL_QUINTTAP: 0,
    }
    btn_state = {
        libevdev.EV_KEY.BTN_LEFT: 0,
        libevdev.EV_KEY.BTN_MIDDLE: 0,
        libevdev.EV_KEY.BTN_RIGHT: 0,
    }

    nskipped_lines = 0

    for event in device["events"]:
        for evdev in event["evdev"]:
            s = slots[slot]
            e = libevdev.InputEvent(
                code=libevdev.evbit(evdev[2], evdev[3]),
                value=evdev[4],
                sec=evdev[0],
                usec=evdev[1],
            )

            if e.code in tool_bits:
                tool_bits[e.code] = e.value
            if e.code in btn_state:
                btn_state[e.code] = e.value

            if args.use_st:
                # Note: this relies on the EV_KEY events to come in before the
                # x/y events, otherwise the last/first event in each slot will
                # be wrong.
                if (
                    e.code == libevdev.EV_KEY.BTN_TOOL_FINGER
                    or e.code == libevdev.EV_KEY.BTN_TOOL_PEN
                ):
                    slot = 0
                    s = slots[slot]
                    s.dirty = True
                    if e.value:
                        s.state = SlotState.BEGIN
                    else:
                        s.state = SlotState.END
                elif e.code == libevdev.EV_KEY.BTN_TOOL_DOUBLETAP:
                    if len(slots) > 1:
                        slot = 1
                    s = slots[slot]
                    s.dirty = True
                    if e.value:
                        s.state = SlotState.BEGIN
                    else:
                        s.state = SlotState.END
                elif e.code == libevdev.EV_ABS.ABS_PRESSURE:
                    s.pressure = e.value
            else:
                if e.code == libevdev.EV_ABS.ABS_MT_SLOT:
                    slot = e.value
                    s = slots[slot]
                    s.dirty = True
                    # bcm5974 cycles through slot numbers, so let's say all below
                    # our current slot number was used
                    for sl in slots[: slot + 1]:
                        sl.used = True
                elif e.code == libevdev.EV_ABS.ABS_MT_TRACKING_ID:
                    if e.value == -1:
                        s.state = SlotState.END
                    else:
                        s.state = SlotState.BEGIN
                        s.delta.x, s.delta.y = 0, 0
                    s.dirty = True
                elif e.code == libevdev.EV_ABS.ABS_MT_PRESSURE:
                    s.pressure = e.value

            if args.use_st:
                axes = [libevdev.EV_ABS.ABS_X, libevdev.EV_ABS.ABS_Y]
            else:
                axes = [
                    libevdev.EV_ABS.ABS_MT_POSITION_X,
                    libevdev.EV_ABS.ABS_MT_POSITION_Y,
                ]

            if e.code in axes:
                s.dirty = True

                # If recording started after touch down
                if s.state == SlotState.NONE:
                    s.state = SlotState.BEGIN
                    s.delta = Point(0, 0)

                if e.code in [
                    libevdev.EV_ABS.ABS_X,
                    libevdev.EV_ABS.ABS_MT_POSITION_X,
                ]:
                    if s.state == SlotState.UPDATE:
                        s.delta.x = e.value - s.position.x
                    s.position.x = e.value
                elif e.code in [
                    libevdev.EV_ABS.ABS_Y,
                    libevdev.EV_ABS.ABS_MT_POSITION_Y,
                ]:
                    if s.state == SlotState.UPDATE:
                        s.delta.y = e.value - s.position.y
                    s.position.y = e.value
                else:
                    assert False, f"Invalid axis {e.code}"

            if e.code == libevdev.EV_SYN.SYN_REPORT:
                if last_time is None:
                    last_time = e.sec * 1000000 + e.usec
                    tdelta = 0
                else:
                    t = e.sec * 1000000 + e.usec
                    tdelta = int((t - last_time) / 1000)  # ms
                    last_time = t

                tools = [
                    (libevdev.EV_KEY.BTN_TOOL_QUINTTAP, "QIN"),
                    (libevdev.EV_KEY.BTN_TOOL_QUADTAP, "QAD"),
                    (libevdev.EV_KEY.BTN_TOOL_TRIPLETAP, "TRI"),
                    (libevdev.EV_KEY.BTN_TOOL_DOUBLETAP, "DBL"),
                    (libevdev.EV_KEY.BTN_TOUCH, "TOU"),
                ]

                for bit, string in tools:
                    if tool_bits[bit]:
                        tool_state = string
                        break
                else:
                    tool_state = "   "

                buttons = [
                    (libevdev.EV_KEY.BTN_LEFT, "L"),
                    (libevdev.EV_KEY.BTN_MIDDLE, "M"),
                    (libevdev.EV_KEY.BTN_RIGHT, "R"),
                ]

                button_state = (
                    "".join([string for bit, string in buttons if btn_state[bit]])
                    or "."
                )

                fmt = SlotFormatter(
                    is_absolute=args.use_absolute,
                    resolution=(xres, yres) if args.use_mm else None,
                    threshold=args.threshold,
                    ignore_below=args.ignore_below,
                    show_distance=args.show_distance,
                    pressure_thresholds=(args.pressure_min, args.pressure_max),
                )
                for sl in [s for s in slots if s.used]:
                    fmt.format_slot(sl)

                    sl.dirty = False
                    sl.delta.x, sl.delta.y = 0, 0
                    if sl.state == SlotState.BEGIN:
                        sl.origin = replace(sl.position)
                        sl.state = SlotState.UPDATE
                    elif sl.state == SlotState.END:
                        sl.state = SlotState.NONE

                if fmt.have_data:
                    if nskipped_lines > 0:
                        print("")
                        nskipped_lines = 0
                    print(
                        "{:2d}.{:06d} {:+5d}ms {} {} {}".format(
                            e.sec, e.usec, tdelta, tool_state, button_state, fmt
                        )
                    )
                elif fmt.filtered:
                    nskipped_lines += 1
                    print(
                        "\r",
                        " " * 21,
                        "... {} below threshold".format(nskipped_lines),
                        flush=True,
                        end="",
                    )


if __name__ == "__main__":
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
