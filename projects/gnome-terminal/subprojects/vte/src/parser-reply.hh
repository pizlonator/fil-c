/*
 * Copyright © 2018 Christian Persch
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

#if !defined(_VTE_REPLY)
#error "Must define _VTE_REPLY before including this file"
#endif

_VTE_REPLY(NONE, NONE, 0, NONE, NONE,) /* placeholder */


_VTE_REPLY(APC,         APC, 0,   NONE, NONE,  ) /* application program command */
_VTE_REPLY(DECEKBD,     APC, 0,   NONE, NONE,  ) /* extended keyboard report */

_VTE_REPLY(XTERM_FOCUS_IN,                            CSI, 'I', NONE, NONE,  ) /* XTERM focus in report */
_VTE_REPLY(XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_PRESS,   CSI, 'M', LT,   NONE,  ) /* XTERM SGR mouse mode button press report */
_VTE_REPLY(XTERM_FOCUS_OUT,                           CSI, 'O', NONE, NONE,  ) /* XTERM focus out report */
_VTE_REPLY(DECXCPR,                                   CSI, 'R', WHAT, NONE,  ) /* extended cursor position report */
_VTE_REPLY(CPR,                                       CSI, 'R', NONE, NONE,  ) /* cursor position report */
_VTE_REPLY(XTERM_SMGRAPHICS_REPORT,                   CSI, 'S', WHAT, NONE,  ) /* xterm graphics attribute report */
_VTE_REPLY(DECDA1R,                                   CSI, 'c', WHAT, NONE,  ) /* DA1 report */
_VTE_REPLY(DECDA2R,                                   CSI, 'c', GT,   NONE,  ) /* DA2 report */
_VTE_REPLY(SGR,                                       CSI, 'm', NONE, NONE,  ) /* SGR */
_VTE_REPLY(DECSGR,                                    CSI, 'm', WHAT, NONE,  ) /* DECSGR */
_VTE_REPLY(XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_RELEASE, CSI, 'm', LT,   NONE,  ) /* XTERM SGR mouse mode button release report */
_VTE_REPLY(DSR,                                       CSI, 'n', NONE, NONE,  ) /* device status report */
_VTE_REPLY(DECDSR,                                    CSI, 'n', WHAT, NONE,  ) /* device status report */
_VTE_REPLY(DECSCUSR,                                  CSI, 'q', NONE, SPACE, ) /* set-cursor-style */
_VTE_REPLY(DECSRC,                                    CSI, 'q', NONE, MULT,  ) /* secure reset confirmation */
_VTE_REPLY(DECSTBM,                                   CSI, 'r', NONE, NONE,  ) /* set top and bottom margins */
_VTE_REPLY(DECSLRM,                                   CSI, 's', NONE, NONE,  ) /* set left and right margins */
_VTE_REPLY(DECSLPP,                                   CSI, 't', NONE, NONE,  ) /* set lines per page */
_VTE_REPLY(XTERM_WM,                                  CSI, 't', NONE, NONE,  ) /* XTERM WM report */
_VTE_REPLY(DECRPKT,                                   CSI, 'v', NONE, COMMA, ) /* report key type */
_VTE_REPLY(DECRPDE,                                   CSI, 'w', NONE, DQUOTE,) /* report displayed extent */
_VTE_REPLY(DECREPTPARM,                               CSI, 'x', NONE, NONE,  ) /* report terminal parameters */
_VTE_REPLY(DECSACE,                                   CSI, 'x', NONE, MULT,  ) /* report DECSACE  */
_VTE_REPLY(DECPKMFR,                                  CSI, 'y', NONE, PLUS,  ) /* program key free memory report */
_VTE_REPLY(DECRPM_ECMA,                               CSI, 'y', NONE, CASH,  ) /* report ECMA mode */
_VTE_REPLY(DECRPM_DEC,                                CSI, 'y', WHAT, CASH,  ) /* report private mode */
_VTE_REPLY(DECMSR,                                    CSI, '{', NONE, MULT,  ) /* macro space report */
_VTE_REPLY(DECFNK,                                    CSI, '~', NONE, NONE,  ) /* dec function key / XTERM bracketed paste */

_VTE_REPLY(DECTABSR,    DCS, '@', NONE, CASH,  ) /* tabulation stop report */
_VTE_REPLY(DECRPSS,     DCS, 'r', NONE, CASH,  ) /* report state or setting */
_VTE_REPLY(DECTSR,      DCS, 's', NONE, CASH,  ) /* terminal state report */
_VTE_REPLY(DECCTR,      DCS, 's', NONE, CASH,  _VTE_REPLY_PARAMS({2})) /* color table report */
_VTE_REPLY(DECAUPSS,    DCS, 'u', NONE, BANG,  ) /* assign user preferred supplemental set */
_VTE_REPLY(DECPSR,      DCS, 'u', NONE, CASH,  ) /* presentation state report */
_VTE_REPLY(DECRPTUI,    DCS, '|', NONE, BANG,  _VTE_REPLY_STRING("7E565445") /* "~VTE" */) /* report terminal unit ID */
_VTE_REPLY(XTERM_DSR,   DCS, '|', GT,   NONE,  ) /* xterm terminal version report */
_VTE_REPLY(DECRPFK,     DCS, '}', NONE, DQUOTE,) /* report function key */
_VTE_REPLY(DECCKSR,     DCS, '~', NONE, BANG,  ) /* memory checksum report */
_VTE_REPLY(DECRPAK,     DCS, '~', NONE, DQUOTE,) /* report all modifiers/alphanumeric key */

_VTE_REPLY(OSC,         OSC, 0,   NONE, NONE,  ) /* operating system command */

_VTE_REPLY(PM,          PM,  0,   NONE, NONE,  ) /* privacy message */

_VTE_REPLY(SOS,         SOS, 0,   NONE, NONE,  ) /* start of string */
