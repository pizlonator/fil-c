#!/usr/bin/env python3
#
# Copyright © 2018, 2020 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.


import argparse
import enum
import inspect
import pathlib
import sys
import typing

from dataclasses import dataclass, field

# Types

class Type(enum.IntEnum):
    ECMA = enum.auto() # CSI n [h|l]
    WHAT = enum.auto() # CSI ? n [h|l]
    GT   = enum.auto() # CSI > n [h|l]

class Flags(enum.Flag):
    NONE = 0
    WRITABLE = enum.auto()

class Source(enum.Enum):
    CONTOUR  = enum.auto()
    DEC      = enum.auto(),
    DRCSTERM = enum.auto(),
    ECMA48   = enum.auto() # eq ISO 6429
    HP       = enum.auto()
    KITTY    = enum.auto()
    ITERM2   = enum.auto()
    RLOGIN   = enum.auto()
    SCO      = enum.auto()
    VTE      = enum.auto()
    WYSE     = enum.auto()
    XDG      = enum.auto()
    XTERM    = enum.auto()
    UNDET    = enum.auto()

    @classmethod
    def from_name(cls, name):
        if name.startswith('CONTOUR'):
            return cls.CONTOUR
        elif name.startswith('DEC') or name.endswith('_DEC'):
            return cls.DEC
        elif name.startswith('DRCS'):
            return cls.DRCSTERM
        elif name.endswith('_ECMA'):
            return cls.ECMA35
        elif name.startswith('HP') or name.endswith('_HP'):
             return cls.HP
        elif name.startswith('KITTY'):
            return cls.KITTY
        elif name.startswith('ITERM'):
            return cls.ITERM2
        elif name.startswith('RLOGIN'):
            return cls.RLOGIN
        elif name.startswith('SCO'):
            return cls.SCO
        elif name.startswith('VTE'):
            return cls.VTE
        elif name.startswith('WY'):
            return cls.WYSE
        elif name.startswith('XDG'):
            return cls.XDG
        elif name.startswith('XTERM'):
            return cls.XTERM
        else:
            return cls.UNDET
            #raise ValueError(f'Could not determine source for mode {name}')

# Control Sequence

@dataclass(eq=True, order=True)
class NamedMode:
    ''' A named mode '''
    mtype: Type
    number: int
    name: str
    default: bool
    preserve_decstr: bool=False
    flags: Flags=Flags.NONE
    source: typing.Optional[Source]=None
    alias: typing.Optional[typing.List[str]]=None
    comment: str=None
    sloc_file: str=None
    sloc_line: int=-1

    def __post_init__(self):

        if self.source is None:
            self.source = Source.from_name(self.name)

        if self.sloc_file is None or self.sloc_line == -1:
            fname = f'mode_{self.mtype.name}'
            stack = inspect.stack()
            depth = -1
            for _frame in stack:
                depth += 1
                if _frame.function == fname:
                    depth += 1
                    break

            if depth == -1 or depth >= len(stack):
                raise ValueError('{self.name} source location not found')
            else:
                frame = stack[depth]
                self.sloc_file = frame.filename
                self.sloc_line = frame.lineno

            del stack


def mode_ECMA(name, number, **kwargs):
    return NamedMode(mtype=Type.ECMA,
                     name=name,
                     number=number,
                     **kwargs)

def mode_WHAT(name, number, **kwargs):
    return NamedMode(mtype=Type.WHAT,
                     name=name,
                     number=number,
                     **kwargs)

def mode_GT(name, number, **kwargs):
    return NamedMode(mtype=Type.GT,
                     name=name,
                     number=number,
                     **kwargs)

# All known modes, ordered by type, source, and number

modes = [

    # Modes for SM_ECMA/RM_ECMA (CSI n [h|l])
    #
    # Most of these are not implemented in VTE.
    #
    # References: ECMA-48 § 7
    #             WY370

    mode_ECMA('GATM', 1, default=False),
    mode_ECMA('KAM', 2, default=False),
    mode_ECMA('CRM', 3, default=False),

    # IRM - insertion replacement mode
    #
    # Default: reset
    #
    # References: ECMA-48 § 7.2.10
    #             VT525
    #
    mode_ECMA('IRM', 4, default=False, flags=Flags.WRITABLE),

    mode_ECMA('SRTM', 5, default=False),
    mode_ECMA('ERM', 6, default=False),
    mode_ECMA('VEM', 7, default=False),

    # BDSM - Bi-Directional Support Mode
    #
    # Reset state is explicit mode, set state is implicit mode
    #
    # References: ECMA-48
    #             ECMA TR/53
    #             Terminal-wg/bidi
    #
    # Default in ECMA: reset
    # Default in Terminal-wg/bidi and VTE: set
    #
    mode_ECMA('BDSM', 8, default=True, flags=Flags.WRITABLE),

    # DCSM defaults to RESET in ECMA, forced to SET in Terminal-wg/bidi#
    mode_ECMA('DCSM', 9, default=True),

    mode_ECMA('HEM', 10, default=False),

    # ECMA-48 § F.4.1 Deprecated
    mode_ECMA('PUM', 11, default=False),

    # SRM - local echo send/receive mode
    # If reset, characters entered by the keyboard are shown on the
    # screen as well as being sent to the host; if set, the
    # keyboard input is only sent to the host.
    #
    # Default: set
    #
    # References: ECMA-48 § 7.2.15
    #             VT525
    #
    # Removed in VTE 0.60: issue #69
    #
    mode_ECMA('SRM', 12, default=True),

    mode_ECMA('FEAM', 13, default=False),
    mode_ECMA('FETM', 14, default=False),
    mode_ECMA('MATM', 15, default=False),
    mode_ECMA('TTM', 16, default=False),
    mode_ECMA('SATM', 17, default=False),
    mode_ECMA('TSM', 18, default=False),

    # ECMA-48 § F.5.1 Removed
    mode_ECMA('EBM', 19, default=False),

    # LNM - line feed/newline mode
    # If set, the cursor moves to the first column on LF, FF, VT,
    # and a Return key press sends CRLF.
    # If reset, the cursor column is unchanged by LF, FF, VT,
    # and a Return key press sends CR only.
    #
    # Default: reset
    #
    # References: ECMA-48 § F.5.2 Removed!
    #             VT525
    #
    mode_ECMA('LNM', 20, default=False, preserve_decstr=True),

    mode_ECMA('GRCM', 21, default=True),

    # ECMA-48 § F.4.2 Deprecated
    mode_ECMA('ZDM', 22, default=False),

    # WYDSCM - display disable mode
    # If set, blanks the screen; if reset, shows the data.
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_ECMA('WYDSCM', 30, default=False),

    # WHYSTLINM - status line display mode
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYSTLINM', 31, default=False),

    # WYCRTSAVM - screen saver mode
    # Like DECCRTSM.
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYCRTSAVM', 32, default=False),

    # WYSTCURM - steady cursor mode
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYSTCURM', 33, default=False),

    # WYULCURM - underline cursor mode
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYULCURM', 34, default=False),

    # WYCLRM - width change clear disable mode
    # If set, the screen is not cleared when the column mode changes
    # by DECCOLM or WY161.
    # Note that this does not affect DECSCPP.
    # This is the same as DECNCSM mode.
    #
    # Default: set (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYCLRM', 35, default=True),

    # WYDELKM - delete key definition
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    # Note: Same as DECBKM
    mode_ECMA('WYDELKM', 36, default=False),

    # WYGATM - send characters mode
    # If set, sends all characters; if reset, only erasable characters.
    # Like GATM above.
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYGATM', 37, default=False),

    # WYTEXM - send full screen/scrolling region to printer
    # Like DECPEX mode.
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYTEXM', 38, default=False),

    # WYEXTDM - extra data line
    # If set, the last line of the screen is used as data line and not
    # a status line; if reset, the last line of the screen is used
    # as a status line.
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_ECMA('WYEXTDM', 40, default=True),

    # WYASCII - WY350 personality mode
    # If set, switches to WY350 personality.
    #
    # Default: reset (set-up)
    #
    # References: WY370
    #
    mode_ECMA('WYASCII', 42, default=True),

    # ************************************************************************

    # Modes for SM_DEC/RM_DEC (CSI ? n [h|l])
    #
    # Most of these are not implemented in VTE.
    #
    # References: VT525
    #             XTERM
    #             KITTY
    #             MINTTY
    #             MLTERM
    #             RLogin
    #             URXVT
    #             WY370
    #

    # DEC:

    # DECCKM - cursor keys mode
    #
    # Controls whether the cursor keys send cursor sequences, or application
    # sequences.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_APPLICATION_CURSOR_KEYS', 1, default=False, flags=Flags.WRITABLE),

    # DECCOLM: 132 column mode
    #
    # Sets page width to 132 (set) or 80 (reset) columns.
    #
    # Changing this mode resets the top, bottom, left, right margins;
    # clears the screen (unless DECNCSM is set); resets DECLRMM; and clearsb
    # the status line if host-writable.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_132_COLUMN', 3, default=False, preserve_decstr=True, flags=Flags.WRITABLE),

    # DECANM - ansi-mode
    # Resetting this puts the terminal into VT52 compatibility mode.
    # To return to ECMA-48 mode, use ESC < (1/11 3/12).
    #
    # Default: set
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECANM', 2, default=True),

    # DECSCLM - scrolling mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECSCLM', 4, default=False),

    # DECSCNM - screen mode
    # If set, displays reverse; if reset, normal.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_REVERSE_IMAGE', 5, default=False, preserve_decstr=True, flags=Flags.WRITABLE),

    # DECOM - origin mode
    # If set, the cursor is restricted to within the page margins.
    #
    # On terminal reset, DECOM is reset.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_ORIGIN', 6, default=False, flags=Flags.WRITABLE),

    # DECAWM - auto wrap mode
    #
    # Controls whether text wraps to the next line when the
    # cursor reaches the right margin.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_AUTOWRAP', 7, default=False, flags=Flags.WRITABLE),

    # DECARM - autorepeat mode
    # Controls whether keys auytomatically repeat while held pressed
    # for more than 0.5s.
    # Note that /some/ keys do not repeat regardless of this setting.
    #
    # Default: set
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECARM', 8, default=True, preserve_decstr=True),

    mode_WHAT('XTERM_MOUSE_X10', 9, default=False, flags=Flags.WRITABLE),
    mode_WHAT('DECLTM', 11, default=False),
    mode_WHAT('DECEKEM', 16, default=False),

    # DECPFF - print FF mode
    # Controls whether the terminal terminates a print command by
    # sending a FF to the printer.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECCPFF', 18, default=False),

    # DECPEX - print extent mode
    # If set, print page prints only the scrolling region;
    # if reset, the complete page.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECPEX', 19, default=False),

    # DECTCEM - text cursor enable
    # If set, the text cursor is visible; if reset, invisible.
    #
    # Default: set
    #
    # References: VT525
    #
    mode_WHAT('DEC_TEXT_CURSOR', 25, default=True, flags=Flags.WRITABLE),

    # DECLRM - RTL mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECRLM', 34, default=False),

    # DECHEBM - hebrew/north-american keyboard mapping mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECHEBM', 35, default=False),

    # DECHEM - hebrew encoding mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECHEM', 36, default=False),

    mode_WHAT('XTERM_DECCOLM', 40, default=False, flags=Flags.WRITABLE),

    # DECNRCM - NRCS mode
    # Operates in 7-bit (set) or 8-bit (reset) mode.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECNRCM', 42, default=False),

    # DECGEPM - Graphics Expanded Print Mode
    #
    # Default: reset
    #
    # References: VT330
    #
    mode_WHAT('DECGEPM', 43, default=False),

    # DECGPCM - Graphics Print Colour Mode
    #
    # Default: reset
    #
    # References: VT330
    #
    # Note: Conflicts with XTERM_MARGIN_BELL
    #
    #mode_WHAT('DECGPCM', 44, default=False),

    # DECGCPS - Graphics Print Colour Syntax
    # If set, uses RGB colour format; if reset, uses HLS colour format.
    #
    # Default: reset
    #
    # References: VT330
    #
    # Note: conflicts with XTERM_REVERSE_WRAP
    #
    #mode_WHAT('DECGPCS', 45, default=False),

    # DECGPBM - Graphics Print Background Mode
    #
    # Default: reset
    #
    # References: VT330
    #
    # Note: conflicts with XTERM_LOGGING (which VTE does not implement)
    #
    #mode_WHAT('DECGPBM', 46, default=False),

    # DECGRPM - Graphics Rotated Print Mode
    #
    # Default: reset
    #
    # References: VT330
    #
    # Note: conflicts with XTERM_ALTBUF
    #
    #mode_WHAT('DECGRPM', 47, default=False),

    mode_WHAT('XTERM_ALTBUF', 47, default=False, flags=Flags.WRITABLE),

    mode_WHAT('DEC131TM', 53, default=False),

    # DECNAKB - greek/north-american keyboard mapping mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECNAKB', 57, default=False),

    # DECIPEM - enter/return to/from pro-printer emulation mode
    # Switches the terminal to (set)/from (reset) the ibm pro
    # printer protocol.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECIPEM', 58, default=False),

    # Kanji/Katakana Display Mode, from VT382-Kanji
    mode_WHAT('DECKKDM', 59, default=True),

    # DECHCCM - horizontal cursor coupling mode
    # Controls what happens when the cursor moves out of the left or
    # right margins of the window.
    # If set, the window pans to keep the cursor in view; if reset,
    # the cursor disappears.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECHCCM', 60, default=False),

    # DECVCCM - vertical cursor coupling mode
    # Controls what happens when the cursor moves out of the top or
    # bottom of the window, When the height of the window is smaller
    # than the page.
    # If set, the window pans to keep the cursor in view; if reset,
    # the cursor disappears.
    #
    # Default: set
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECVCCM', 61, default=True),

    # DECPCCM - page cursor coupling mode
    #
    # Default: set
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECPCCM', 64, default=True),

    # DECNKM - numeric/application keypad mode
    # Controls whether the numeric keypad sends application (set)
    # or keypad (reset) sequences.
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DEC_APPLICATION_KEYPAD', 66, default=False, flags=Flags.WRITABLE),

    # DECBKM - backarrow key mode
    # WYDELKM
    #
    # If set, the Backspace key works as a backspace key
    # sending the BS control; if reset, it works as a Delete
    # key sending the DEL control.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECBKM', 67, default=False, alias=['WYDELKM']),

    # DECKBUM - typewriter/data rpocessing keys mode
    #
    # If set, the keyboard keys act as data processing keys;
    # if reset, as typewriter keys.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECKBUM', 68, default=False),

    # DECLRMM - vertical split-screen mode
    # Controls whether a DECSLRM is executed.
    # On set, resets line attributes to single width and single height,
    # and while set, the terminal ignores any changes to line attributes.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Needs to be implemented if DECSLRM is implemented, to resolve a
    # conflict between DECSLRM and SCOSC.
    #
    # aka DECVSSM
    #
    mode_WHAT('DECLRMM', 69, default=False, flags=Flags.WRITABLE),

    # DECXRLM - transmit rate limit
    # If set, limits the transmit rate; if reset, the rate is
    # unlimited.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECXRLM', 73, default=False),

    # DECSDM - sixel display mode (scrolling)
    # If set, SIXEL scrolling is disabled; when reset, SIXEL scrolling
    # is enabled.
    #
    # Default: reset
    #
    # References: ?
    #
    # Note: Conflicts with WY161
    #
    mode_WHAT('DECSDM', 80, default=False, flags=Flags.WRITABLE),

    # DECKPM - key position mode
    # If set, the keyboard sends extended reports (DECEKBD) that include
    # the key position and modifier state; if reset, it sends character codes.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECKPM', 81, default=False),

    # Thai Space Compensating Mode, from VT382-Thai
    mode_WHAT('DECTHAISCM', 90, default=False),

    # DECNCSM - no clear screen on DECOLM
    # If set, the screen is not cleared when the column mode changes
    # by DECCOLM.
    # Note that this does not affect DECSCPP.
    #
    # Default: set
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECNCSM', 95, default=False),

    # DECRLCM - RTL copy mode
    # If set, copy/paste from RTL; if reset, from LTR.
    # Only enabled when the keyboard language is set to hebrew.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECRLCM', 96, default=False),

    # DECCRTSM - CRT save mode
    # When set, blanks the terminal after the inactivity timeout
    # (set with DECCRTST).
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECRCRTSM', 97, default=False),

    # DECARSM - auto resize mode
    # Sets whether changing page arrangements automatically
    # changes the lines per screen.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECARSM', 98, default=False),

    # DECMCM - modem control mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECMCM', 99, default=False),

    # DECAAM - auto answerback mode
    #
    # Default: reset
    #
    # References: VT525
    #
    mode_WHAT('DECAAM', 100, default=False),

    # DECCANSM - conceal answerback message mode
    #
    # Default: reset
    #
    # References: VT525
    #
    # Unimplemented, since we don't support answerback at all.
    #
    mode_WHAT('DECANSM', 101, default=False),

    # DECNULM - null mode
    # If set, pass NUL to the printer; if reset, discard NUL.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECNULM', 102, default=False),

    # DECHDPXM - half-duplex mode
    # Whether to use half-duplex (set) or full-duplex (reset) mode.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECHDPXM', 103, default=False),

    # DECESKM - enable secondary keyboard language mode
    # If set, use the secondary keyboard mapping (group 2); if reset,
    # use the primary (group 1).
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECESKM', 104, default=False),

    # DECOSCNM - overscan mode
    # (monochrome terminal only)
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECOSCNM', 106, default=False),

    # DECNUMLK - num lock mode
    #
    # Set the num lock state as if by acting the NumLock key.
    # Set means NumLock on; reset means off.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECNUMLK', 108, default=False),

    # DECCAPSLK - caps lock mode
    #
    # Set the caps lock state as if by acting the CapsLock key.
    # Set means CapsLock on; reset means off.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECCAPSLK', 109, default=False),

    # DECKLHIM - keyboard LED host indicator mode
    # If set, the keyboard LEDs show the state from the host
    # (see DECLL); if reset, the local state.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECKLHIM', 110, default=False),

    # DECFWM - framed window mode
    # If set, session window frames are drawn with frame border and icon.
    #
    # Default: reset
    #
    # References: VT525
    #
    # VTE does not support sessions.
    #
    mode_WHAT('DECFWM', 111, default=False),

    # DECRPL - review previous lines mode
    # If set, allows to view the scrollback.
    #
    # Default: set (VTE)
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECRPL', 112, default=True),

    # DECHWUM - host wake-up mode
    # If set, the terminal exits CRT save and energy save mode
    # when a character is received from the host.
    #
    # Default: ?
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECHWUM', 113, default=False),

    # DECTCUM - alternate text color underline mode
    #
    # If set, text with the undeerline attribute is underlined as
    # well as being displayed in the alternate coolor (if
    # specified); if reset, it is only displayed in the
    # alternate color.
    #
    # Default: ?
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECATCUM', 114, default=False),

    # DECTCBM - alternate text color blink mode
    #
    # If set, text with the blink attribute blinks as well
    # as being displayed in the alternate color (if
    # specified); if reset, it is only displayed in the
    # alternate color.
    #
    # Default: ?
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECATCBM', 115, default=False),

    # DECBBSM - bold and blink style mode
    #
    # If set, the bold or blink attributes affect both foreground
    # and background color; if reset, those affect only the foreground
    # color.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECBBSM', 116, default=False),

    # DECECM - erase color mode
    #
    # If set, erased text or new cells appearing on the screen by scrolling
    # are assigned the screen background color; if reset, they are assigned
    # the text background color.
    #
    # Default: reset
    #
    # References: VT525
    #
    # Probably not worth implementing.
    #
    mode_WHAT('DECECM', 117, default=False),

    # Contour:

    mode_WHAT('CONTOUR_BATCHED_RENDERING', 2026, default=False),

    # DRCSTerm:

    # DRCSMM_V1
    # Whether to enable DRCSMMv1 unicode mapping
    #
    # Default: reset
    #
    # References: DRCSTerm
    #
    mode_WHAT('DRCSMM_V1', 8800, default=False),

    # KiTTY:

    mode_WHAT('KITTY_STYLED_UNDERLINES', 2016, default=True),
    mode_WHAT('KITTY_EXTENDED_KEYBOARD', 2017, default=False),

    # MinTTY:

    mode_WHAT('MINTTY_REPORT_CJK_AMBIGUOUS_WIDTH', 7700, default=False),
    mode_WHAT('MINTTY_REPORT_SCROLL_MARKER_IN_CURRENT_LINE', 7711, default=False),
    mode_WHAT('MINTTY_APPLICATION_ESCAPE', 7727, default=False),
    mode_WHAT('MINTTY_ESCAPE_SENDS_FS',7728, default=False),

    # MINTTY_SIXEL_SCROLL_END_POSITION:
    # If set, sixel scrolling moves the cursor to the left margin on the
    # next line; if reset, moves the cursor to the right of the inserted
    # graphic.
    #
    # Default: reset
    #
    # References: MinTTY
    mode_WHAT('MINTTY_SIXEL_SCROLL_END_POSITION',7730, default=False),

    mode_WHAT('MINTTY_SCROLLBAR', 7766, default=False),
    mode_WHAT('MINTTY_REPORT_FONT_CHANGES', 7767, default=False),
    mode_WHAT('MINTTY_SHORTCUT_OVERRIDE', 7783, default=False),
    mode_WHAT('MINTTY_ALTBUF_MOUSEWHEEL_TO_CURSORKEYS', 7786, default=False),
    mode_WHAT('MINTTY_MOUSEWHEEL_APPLICATION_KEYS', 7787, default=False),
    mode_WHAT('MINTTY_BIDI_DISABLE_IN_CURRENT_LINE', 7796, default=False),

    # MINTTY_SIXEL_SCROLL_CURSOR_RIGHT:
    # If set, sixel scrolling moves the cursor to the right of the
    # inserted graphic; if reset, MINTTY_SIXEL_SCROLL_END_POSITION
    # takes effect.
    #
    # Default: reset
    #
    # References: MinTTY
    mode_WHAT('MINTTY_SIXEL_SCROLL_CURSOR_RIGHT', 8452, default=False,
              alias=['RLOGIN_SIXEL_SCROLL_CURSOR_RIGHT']),

    # MinTTY also knows mode 77096 "BIDI disable", and 77000..77031
    # "Application control key", all of  which are outside of the supported
    # range CSI parameters in VTE, so we don't list them here and VTE will
    # never support them.

    # RLogin:

    # RLogin has many private modes
    # [https://github.com/kmiya-culti/RLogin/blob/master/RLogin/TextRam.h#L131]:
    # 1406..1415, 1420..1425, 1430..1434, 1436, 1452..1481,
    # 8400..8406, 8416..8417, 8428..8429, 8435, 8437..8443,
    # 8446..8458,
    # and modes 7727, 7786, 8200 (home cursor on [ED 2]),
    # 8800 (DRCSMM_V1), 8840 (same as 8428).
    #
    # We're not going to implement them, but avoid these ranges
    # when assigning new mode numbers.
    #
    # The following are the ones from RLogin that MLTerm knows about:

    #mode_WHAT('RLOGIN_APPLICATION_ESCAPE', 7727, default=False)
    #mode_WHAT('RLOGIN_MOUSEWHEEL_TO_CURSORKEYS', 7786, default=False)

    # Ambiguous-width characters are wide (reset) or narrow (set)
    mode_WHAT('RLOGIN_AMBIGUOUS_WIDTH_CHARACTERS_NARROW', 8428, default=False),

    # XTERM also knows this one
    #mode_WHAT('RLOGIN_SIXEL_SCROLL_CURSOR_RIGHT', 8452, default=False),

    # [u]RXVT:

    mode_WHAT('RXVT_TOOLBAR', 10, default=False),
    mode_WHAT('RXVT_SCROLLBAR', 30, default=False),

    # Conflicts with DECHEBM
    #mode_WHAT('RXVT_SHIFT_KEYS', 35, default=False),

    mode_WHAT('RXVT_SCROLL_OUTPUT', 1010, default=False),
    mode_WHAT('RXVT_SCROLL_KEYPRESS', 1011, default=False),
    mode_WHAT('RXVT_MOUSE_EXT', 1015, default=False),

    # Bold/blink uses normal (reset) or high intensity (set) colour
    mode_WHAT('RXVT_INTENSITY_STYLES', 1021, default=True),

    # WYSE:

    # WYTEK - TEK 4010/4014 personality
    # If set, switches to TEK 4010/4014 personality.
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_WHAT('WYTEK', 38, default=False, alias=['DECTEK']),

    # WY161 - 161 column mode
    # If set, switches the terminal to 161 columns; if reset,
    # to 80 columns.
    #
    # Default: reset
    #
    # References: WY370
    #
    # Note: Conflicts with DECSDM
    #mode_WHAT('WY161', 80, default=False),

    # WY52 - 52 lines mode
    # If set, switches the terminal to 52 lines; if reset,
    # to 24 lines.
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_WHAT('WY52', 83, default=False),

    # WYENAT - enable separate attributes
    # If set, SGR attributes may be set separately for eraseable
    # and noneraseable characters. If reset, the same SGR attributes
    # apply to both eraseable and noneraseable characters.
    #
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_WHAT('WYENAT', 84, default=False),

    # WYREPL - replacement character color
    #
    # Default: reset
    #
    # References: WY370
    #
    mode_WHAT('WYREPL', 85, default=False),

    # VTE:

    # Whether to swap the Left and Right arrow keys if the cursor
    # stands over an RTL paragraph.
    #
    # Default:  set
    #
    # Reference: Terminal-wg/bidi
    #
    mode_WHAT('VTE_BIDI_SWAP_ARROW_KEYS', 1243, default=True, flags=Flags.WRITABLE),

    # Whether box drawing characters in the U+2500..U+257F range
    # are to be mirrored in RTL context.
    #
    # Default: reset
    #
    # Reference: Terminal-wg/bidi
    #
    mode_WHAT('VTE_BIDI_BOX_MIRROR', 2500, default=False, flags=Flags.WRITABLE),

    # Whether BiDi paragraph direction is autodetected.
    #
    # Default: reset
    #
    # Reference: Terminal-wg/bidi
    #
    mode_WHAT('VTE_BIDI_AUTO', 2501, default=False, flags=Flags.WRITABLE),

    # XTERM:

    mode_WHAT('XTERM_ATT610_BLINK', 12, default=False),
    mode_WHAT('XTERM_CURSOR_BLINK', 13, default=False),
    mode_WHAT('XTERM_CURSOR_BLINK_XOR', 14, default=False),
    mode_WHAT('XTERM_CURSES_HACK', 41, default=False),
    mode_WHAT('XTERM_MARGIN_BELL', 44, default=False),
    mode_WHAT('XTERM_REVERSE_WRAP', 45, default=False),
    mode_WHAT('XTERM_LOGGING', 46, default=False),

    mode_WHAT('XTERM_MOUSE_VT220', 1000, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_MOUSE_VT220_HIGHLIGHT', 1001, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_MOUSE_BUTTON_EVENT', 1002, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_MOUSE_ANY_EVENT', 1003, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_FOCUS', 1004, default=False, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_MOUSE_EXT', 1005, default=False),

    mode_WHAT('XTERM_MOUSE_EXT_SGR', 1006, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_ALTBUF_SCROLL', 1007, default=True, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_FAST_SCROLL', 1014, default=False),
    mode_WHAT('XTERM_MOUSE_EXT_SGR_PIXEL', 1016, default=False),

    mode_WHAT('XTERM_8BIT_META', 1034, default=False),
    mode_WHAT('XTERM_NUMLOCK', 1035, default=False),

    mode_WHAT('XTERM_META_SENDS_ESCAPE', 1036, default=True, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_DELETE_IS_DEL', 1037, default=False),
    mode_WHAT('XTERM_ALT_SENDS_ESCAPE', 1039, default=False),
    mode_WHAT('XTERM_KEEP_SELECTION', 1040, default=False),
    mode_WHAT('XTERM_SELECT_TO_CLIPBOARD', 1041, default=False),
    mode_WHAT('XTERM_BELL_URGENT', 1042, default=False),
    mode_WHAT('XTERM_PRESENT_ON_BELL', 1043, default=False),
    mode_WHAT('XTERM_KEEP_CLIPBOARD', 1044, default=False),
    mode_WHAT('XTERM_ALLOW_ALTBUF', 1046, default=True),

    mode_WHAT('XTERM_OPT_ALTBUF', 1047, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_SAVE_CURSOR', 1048, default=False, flags=Flags.WRITABLE),
    mode_WHAT('XTERM_OPT_ALTBUF_SAVE_CURSOR', 1049, default=False, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_FKEYS_TERMCAP', 1050, default=False),
    mode_WHAT('XTERM_FKEYS_SUN', 1051, default=False),
    mode_WHAT('XTERM_FKEYS_HP', 1052, default=False),
    mode_WHAT('XTERM_FKEYS_SCO', 1053, default=False),
    mode_WHAT('XTERM_FKEYS_LEGACY', 1060, default=False),
    mode_WHAT('XTERM_FKEYS_VT220', 1061, default=False),

    # XTERM_SIXEL_PRIVATE_COLOR_REGISTERS:
    # When set, each SIXEL graphic uses newly initialised colour registers.
    # When reset, changes to colour registers from one SIXEL image are
    # saved and used for the next SIXEL graphic.
    #
    # Default: set
    #
    # References: XTERM
    #
    mode_WHAT('XTERM_SIXEL_PRIVATE_COLOR_REGISTERS', 1070, default=True, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_READLINE_BUTTON1_MOVE_POINT', 2001, default=False),
    mode_WHAT('XTERM_READLINE_BUTTON2_MOVE_POINT', 2002, default=False),
    mode_WHAT('XTERM_READLINE_DBLBUTTON3_DELETE', 2003, default=False),

    # Whether to surround pasted text with CSI ~ sequences
    #
    # Default: reset
    #
    # References: XTERM
    #
    mode_WHAT('XTERM_READLINE_BRACKETED_PASTE', 2004, default=False, flags=Flags.WRITABLE),

    mode_WHAT('XTERM_READLINE_PASTE_QUOTE', 2005, default=False),
    mode_WHAT('XTERM_READLINE_PASTE_LITERAL_NL', 2006, default=False),

    # In-band resize notifications,
    #
    # References: https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83
    mode_WHAT('UNDET_IRN', 2048, default=False),

    # ************************************************************************

    # Modes for SM_HP/RM_HP (CSI > n [h|l])
    #
    # None of these are not implemented in VTE.
    #
    # References: HP 2397A

    # HP:

    # HP_MULTIPAGE:
    # If set, the terminal has multiple pages of 24 lines of display memory.
    # If reset, the terminal only has one page of 24 lines of display memory
    #
    # Default: reset
    #
    # References: HP 2397A
    mode_GT('HP_MULTIPAGE', 1, default=False),

    # HP_MEMLOCK:
    #
    # Default: reset
    #
    # References: HP 2397A
    mode_GT('HP_MEMLOCK', 2, default=False),

]

# Output generator

''' Write copyright header '''
def write_header(outfile):
    outfile.write('''
/* Generated by modes.py; do not edit! */

/*
 * Copyright © 2018, 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */
''')


''' Write sequences '''
def write_modes(output, mtype):
    outfile = open(output.as_posix(), 'w')
    write_header(outfile)
    outfile.write('''
#if !defined(MODE) || !defined(MODE_FIXED)
#error "Must define MODE and MODE_FIXED before including this file"
#endif

''')

    for m in sorted([m for m in modes if m.mtype == mtype], key=lambda m: m.number):

        if m.flags & Flags.WRITABLE:
            outfile.write(f'MODE('
                          f'{m.name}, '
                          f'{m.number})\n')
        else:
            value = 'ALWAYS_SET' if m.default else 'ALWAYS_RESET'
            outfile.write(f'MODE_FIXED('
                          f'{m.name}, '
                          f'{m.number}, '
                          f'{value})\n')


# main

''' main '''
if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='modes include file generator')
    parser.add_argument('--destdir',
                        type=pathlib.Path,
                        default=pathlib.PosixPath('.'),
                        help='Output directory')

    try:
        args = parser.parse_args()
    except Exception as e:
        print(f'Failed to parse arguments: {e}')
        sys.exit(1)

    write_modes(args.destdir / "modes-ecma.hh", Type.ECMA)
    write_modes(args.destdir / "modes-dec.hh", Type.WHAT)
    # write_modes(args.destdir / "modes-hp.hh", Type.GT)
