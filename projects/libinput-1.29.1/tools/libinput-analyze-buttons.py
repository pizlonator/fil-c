#!/usr/bin/env python3
# -*- coding: utf-8
# vim: set expandtab shiftwidth=4:
# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil -*- */
#
# Copyright © 2024 Red Hat, Inc.
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
# Prints the data from a libinput recording in a table format to ease
# debugging.
#
# Input is a libinput record yaml file

from dataclasses import dataclass
import argparse
import os
import sys
import yaml
import libevdev

COLOR_RESET = "\x1b[0m"
COLOR_RED = "\x1b[6;31m"


def micros(e: libevdev.InputEvent):
    return e.usec + e.sec * 1_000_000


@dataclass
class Timestamp:
    sec: int
    usec: int

    @property
    def micros(self) -> int:
        return self.usec + self.sec * 1_000_000


@dataclass
class ButtonFrame:
    delta_ms: int  # delta time to last button (not evdev!) frame
    evdev_delta_ms: int  # delta time to last evdev frame
    events: list[libevdev.InputEvent]  # BTN_ events only

    @property
    def timestamp(self) -> Timestamp:
        e = self.events[0]
        return Timestamp(e.sec, e.usec)

    def value(self, code: libevdev.EventCode) -> bool | None:
        for e in self.events:
            if e.matches(code):
                return e.value
        return None

    def values(self, codes: list[libevdev.EventCode]) -> list[bool | None]:
        return [self.value(code) for code in codes]


def frames(events):
    last_timestamp = None
    current_frame = None
    last_frame = None
    for e in events:
        if last_timestamp is None:
            last_timestamp = micros(e)
        if e.type == libevdev.EV_SYN:
            last_timestamp = micros(e)
            if current_frame is not None:
                yield current_frame
            last_frame = current_frame
            current_frame = None
        elif e.type == libevdev.EV_KEY:
            if e.code.name.startswith("BTN_") and not e.code.name.startswith(
                "BTN_TOOL_"
            ):
                timestamp = micros(e)
                evdev_delta = (timestamp - last_timestamp) // 1000

                if last_frame is not None:
                    delta = (timestamp - last_frame.timestamp.micros) // 1000
                else:
                    delta = 0

                if current_frame is None:
                    current_frame = ButtonFrame(
                        delta_ms=delta, evdev_delta_ms=evdev_delta, events=[e]
                    )
                else:
                    current_frame.events.append(e)


def main(argv):
    parser = argparse.ArgumentParser(description="Display button events in a recording")
    parser.add_argument(
        "--threshold",
        type=int,
        default=25,
        help="Mark any time delta above this threshold (in ms)",
    )
    parser.add_argument(
        "path", metavar="recording", nargs=1, help="Path to libinput-record YAML file"
    )
    args = parser.parse_args()
    isatty = os.isatty(sys.stdout.fileno())
    if not isatty:
        global COLOR_RESET
        global COLOR_RED
        COLOR_RESET = ""
        COLOR_RED = ""

    yml = yaml.safe_load(open(args.path[0]))
    if yml["ndevices"] > 1:
        print(f"WARNING: Using only first {yml['ndevices']} devices in recording")
    device = yml["devices"][0]
    if not device["events"]:
        print("No events found in recording")
        sys.exit(1)

    def events():
        """
        Yields the next event in the recording
        """
        for event in device["events"]:
            for evdev in event.get("evdev", []):
                yield libevdev.InputEvent(
                    code=libevdev.evbit(evdev[2], evdev[3]),
                    value=evdev[4],
                    sec=evdev[0],
                    usec=evdev[1],
                )

    # These are the buttons we possibly care about, but we filter to the ones
    # found on this device anyway
    buttons = [
        libevdev.EV_KEY.BTN_LEFT,
        libevdev.EV_KEY.BTN_MIDDLE,
        libevdev.EV_KEY.BTN_RIGHT,
        libevdev.EV_KEY.BTN_SIDE,
        libevdev.EV_KEY.BTN_EXTRA,
        libevdev.EV_KEY.BTN_FORWARD,
        libevdev.EV_KEY.BTN_BACK,
        libevdev.EV_KEY.BTN_TASK,
        libevdev.EV_KEY.BTN_TOUCH,
        libevdev.EV_KEY.BTN_STYLUS,
        libevdev.EV_KEY.BTN_STYLUS2,
        libevdev.EV_KEY.BTN_STYLUS3,
        libevdev.EV_KEY.BTN_0,
        libevdev.EV_KEY.BTN_1,
        libevdev.EV_KEY.BTN_2,
        libevdev.EV_KEY.BTN_3,
        libevdev.EV_KEY.BTN_4,
        libevdev.EV_KEY.BTN_5,
        libevdev.EV_KEY.BTN_6,
        libevdev.EV_KEY.BTN_7,
        libevdev.EV_KEY.BTN_8,
        libevdev.EV_KEY.BTN_9,
    ]

    def filter_buttons(buttons):
        return filter(
            lambda c: c in buttons,
            map(lambda c: libevdev.evbit("EV_KEY", c), device["evdev"]["codes"][1]),
        )

    buttons = list(filter_buttons(buttons))

    # all BTN_STYLUS will have a header of S - meh
    btn_headers = " │ ".join(b.name[4] for b in buttons)
    print(f"{'Timestamp':^13s} │ {'Delta':^8s} │ {btn_headers}")
    last_btn_vals = [None] * len(buttons)

    def btnchar(b, last):
        if b == 1:
            return "┬"
        if b == 0:
            return "┴"
        return "│" if last else " "

    for frame in frames(events()):
        ts = frame.timestamp
        if frame.timestamp.micros > 0 and frame.delta_ms < args.threshold:
            color = COLOR_RED
        else:
            color = ""
        btn_vals = frame.values(buttons)
        btn_strs = " │ ".join(
            [btnchar(b, last) for b, last in zip(btn_vals, last_btn_vals)]
        )

        last_btn_vals = [
            b if b is not None else last for b, last in zip(btn_vals, last_btn_vals)
        ]

        print(
            f"{color}{ts.sec:6d}.{ts.usec:06d} │ {frame.delta_ms:6d}ms │ {btn_strs}{COLOR_RESET}"
        )


if __name__ == "__main__":
    try:
        main(sys.argv)
    except BrokenPipeError:
        pass
