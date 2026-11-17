#!/usr/bin/env python3
#
# Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
# Copyright © 2018, 2019, 2020 Christian Persch
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
    COMMAND = enum.auto() # placeholder / special handling
    GRAPHIC = enum.auto()
    CONTROL = enum.auto()
    ESCAPE  = enum.auto()
    CSI     = enum.auto()
    DCS     = enum.auto()
    OSC     = enum.auto()
    SCI     = enum.auto()
    APC     = enum.auto()
    PM      = enum.auto()
    SOS     = enum.auto()

class Intermediate(enum.IntEnum):
    SPACE   = enum.auto() # SP  02/00
    BANG    = enum.auto() # !   02/01
    DQUOTE  = enum.auto() # "   02/02
    HASH    = enum.auto() # #   02/03
    CASH    = enum.auto() # $   02/04
    PERCENT = enum.auto() # %   02/05
    AND     = enum.auto() # &   02/06
    SQUOTE  = enum.auto() # \'  02/07
    POPEN   = enum.auto() # (   02/08
    PCLOSE  = enum.auto() # )   02/09
    MULT    = enum.auto() # *   02/10
    PLUS    = enum.auto() # +   02/11
    COMMA   = enum.auto() # ,   02/12
    MINUS   = enum.auto() # -   02/13
    DOT     = enum.auto() # .   02/14
    SLASH   = enum.auto() # /   02/15

class ParameterIntro(enum.IntEnum):
        # Numbers; not used         *  03/00..03/09
        # COLON is reserved         = ':'   * 03/10
        # SEMICOLON is reserved     = ';'   * 03/11
        LT    = enum.auto() # '<'  03/12
        EQUAL = enum.auto() # '='  03/13
        GT    = enum.auto() # '>'  03/14
        WHAT  = enum.auto() # '?'  03/15

class Direction(enum.Flag):
    HTT  = enum.auto() # Host to Terminal
    TTH  = enum.auto() # Terminal to Host
    BIDI = HTT | TTH

class Flags(enum.Flag):
    NOP_TTH = enum.auto()    # NOP terminal to host
    NOP_HTT = enum.auto()    # NOP host to terminal
    NOP = NOP_TTH | NOP_HTT  # NOP both directions
    UNRIPE = enum.auto()     # dispatch when unripe
    HANDLER_RV = enum.auto() # handler has return value

class Source(enum.Enum):
    DEC    = enum.auto(),
    ECMA16 = enum.auto() # eq ISO 1745
    ECMA35 = enum.auto() # eq ISO 2022
    ECMA48 = enum.auto() # eq ISO 6429
    HP     = enum.auto()
    ITERM2 = enum.auto()
    MINTTY = enum.auto()
    RLOGIN = enum.auto()
    SCO    = enum.auto()
    VTE    = enum.auto()
    WYSE   = enum.auto()
    XDG    = enum.auto()
    XTERM  = enum.auto()

    @classmethod
    def from_name(cls, name):
        if name.startswith('DEC') or name.endswith('_DEC'):
            return cls.DEC
        elif name.endswith('_ECMA'):
            return cls.ECMA48
        elif name.startswith('HP') or name.endswith('_HP'):
             return cls.HP
        elif name.startswith('ITERM'):
            return cls.ITERM2
        elif name.startswith('MINTTY'):
            return cls.MINTTY
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
            return cls.ECMA48

# Control Sequence

@dataclass(eq=True, order=True, frozen=True)
class Sequence:
    ''' A control sequence '''
    stype: Type
    final: int=0
    pintro: typing.Tuple[ParameterIntro, ...]=()
    intermediates: typing.Tuple[Intermediate, ...]=()

    def __post_init__(self):
        if len(self.pintro) > 1:
            raise ValueError('Can only have none or one ParameterIntro')
        if len(self.intermediates) > 4:
            raise ValueError('Too many intermediates')


@dataclass(order=True)
class NamedSequence:
    ''' A named control sequence '''
    seq: Sequence
    name: str
    direction: Direction=Direction.HTT
    flags: typing.Optional[Flags]=None
    source: typing.Optional[Source]=None
    comment: str=None
    sloc_file: str=None
    sloc_line: int=-1

    def __post_init__(self):

        if self.source is None:
            self.source = Source.from_name(self.name)

        if self.sloc_file is None or self.sloc_line == -1:
            fname = f'seq_{self.seq.stype.name}'
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


def named_sequence(**kwargs):
    sargs={key: value for key, value in kwargs.items() if key in ['stype', 'final', 'pintro', 'intermediates']}
    nargs={key: value for key, value in kwargs.items() if key not in ['stype', 'final', 'pintro', 'intermediates']}

    return NamedSequence(seq=Sequence(**sargs), **nargs)

command_final=0x100
def seq_COMMAND(name, **kwargs):
    # fake final character to disambiguate commands
    global command_final
    command_final += 1
    return named_sequence(name=name,
                          stype=Type.COMMAND,
                          final=command_final,
                          **kwargs)

def seq_CONTROL(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.CONTROL,
                          final=final,
                          **kwargs)

def seq_ESCAPE(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.ESCAPE,
                          final=ord(final[0]),
                          **kwargs)

def seq_CSI(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.CSI,
                          final=ord(final[0]),
                          **kwargs)

def seq_DCS(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.DCS,
                          final=ord(final[0]),
                          **kwargs)

def seq_OSC(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.OSC,
                          final=ord(final[0]),
                          **kwargs)

def seq_SCI(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.SCI,
                          final=ord(final[0]),
                          **kwargs)

def seq_APC(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.APC,
                          final=ord(final[0]),
                          **kwargs)

def seq_PM(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.PM,
                          final=ord(final[0]),
                          **kwargs)

def seq_SOS(name, final, **kwargs):
    return named_sequence(name=name,
                          stype=Type.SOS,
                          final=ord(final[0]),
                          **kwargs)

# All known sequences, ordered by type, final character, pintro, intermediates

sequences = [
    # Commands that are handled specially by the parser than the other sequences
    seq_COMMAND('ACS', source=Source.ECMA35,
                comment='announce code structure'),
    seq_COMMAND('CnD', flags=Flags.NOP, source=Source.ECMA35,
                comment='Cn designate'),
    seq_COMMAND('DOCS', flags=Flags.NOP, source=Source.ECMA35,
                comment='designate other coding system'),
    seq_COMMAND('GnDm', source=Source.ECMA35,
                comment='Gn designate 9m charset'),
    seq_COMMAND('GnDMm', flags=Flags.NOP, source=Source.ECMA35,
                comment='Gn designate multibyte 9m charset'),
    seq_COMMAND('IRR', flags=Flags.NOP, source=Source.ECMA35,
                comment='identify revised registration'),
    seq_COMMAND('OSC',
                comment='operating system command'),

    # Control characters
    seq_CONTROL('NUL', 0x00, flags=Flags.NOP, source=Source.ECMA16,
                comment='nul'),
    seq_CONTROL('SOH', 0x01, flags=Flags.NOP, source=Source.ECMA16,
                comment='start of heading'),
    seq_CONTROL('STX', 0x02, flags=Flags.NOP, source=Source.ECMA16,
                comment='start of text'),
    seq_CONTROL('ETX', 0x03, flags=Flags.NOP, source=Source.ECMA16,
                comment='end of text'),
    seq_CONTROL('EOT', 0x04, flags=Flags.NOP, source=Source.ECMA16,
                comment='end of transmission'),
    seq_CONTROL('ENQ', 0x05, flags=Flags.NOP, source=Source.ECMA16,
                comment='enquire'),
    seq_CONTROL('ACK', 0x06, flags=Flags.NOP, source=Source.ECMA16,
                comment='acknowledge'),
    seq_CONTROL('BEL', 0x07, source=Source.ECMA16,
                comment='bell'),
    seq_CONTROL('BS', 0x08, source=Source.ECMA16,
                comment='backspace'),
    seq_CONTROL('HT', 0x09, source=Source.ECMA16,
                comment='horizontal tab'),
    seq_CONTROL('LF', 0x0a, source=Source.ECMA16,
                comment='line feed'),
    seq_CONTROL('VT', 0x0b, source=Source.ECMA16,
                comment='vertical tag'),
    seq_CONTROL('FF', 0x0c, source=Source.ECMA16,
                comment='form feed'),
    seq_CONTROL('CR', 0x0d, source=Source.ECMA16,
                comment='carriage return'),
    seq_CONTROL('LS1', 0x0e, source=Source.ECMA16,
                comment='locking shift 1'),
    seq_CONTROL('LS0', 0x0f, source=Source.ECMA16,
                comment='locking shift 0'),
    seq_CONTROL('DLE', 0x10, flags=Flags.NOP, source=Source.ECMA16,
                comment='data link escape'),
    seq_CONTROL('DC1', 0x11, flags=Flags.NOP, source=Source.ECMA16,
                comment='device control 1 / XON'),
    seq_CONTROL('DC2', 0x12, flags=Flags.NOP, source=Source.ECMA16,
                comment='device control 2'),
    seq_CONTROL('DC3', 0x13, flags=Flags.NOP, source=Source.ECMA16,
                comment='device control 3 / XOFF'),
    seq_CONTROL('DC4', 0x14, flags=Flags.NOP, source=Source.ECMA16,
                comment='device control 4'),
    seq_CONTROL('NAK', 0x15, flags=Flags.NOP, source=Source.ECMA16,
                comment='negative acknowledge'),
    seq_CONTROL('SYN', 0x16, flags=Flags.NOP, source=Source.ECMA16,
                comment='synchronise'),
    seq_CONTROL('ETB', 0x17, flags=Flags.NOP, source=Source.ECMA16,
                comment='end of transmissionblock'),
    # seq_CONTROL('CAN', 0x18, flags=Flags.NOP, source=Source.ECMA16,
    #             comment='cancel'),
    seq_CONTROL('EM', 0x19, flags=Flags.NOP, source=Source.ECMA16,
                comment='end of medium'),
    seq_CONTROL('SUB', 0x1a, source=Source.ECMA16,
                comment='substitute'),
    # seq_CONTROL('ESC', 0x1b, source=Source.ECMA16,
    #             comment='escape'),
    seq_CONTROL('IS4', 0x1c, flags=Flags.NOP, source=Source.ECMA16,
                comment='information separator 4 / file separator (FS)'),
    seq_CONTROL('IS3', 0x1d, flags=Flags.NOP, source=Source.ECMA16,
                comment='information separator 3 / group separator (GS)'),
    seq_CONTROL('IS2', 0x1e, flags=Flags.NOP, source=Source.ECMA16,
                comment='information separator 2 / record separator (RS)'),
    seq_CONTROL('IS1', 0x1f, flags=Flags.NOP, source=Source.ECMA16,
                comment='information separator 1 / unit separator (US)'),
    seq_CONTROL('BPH', 0x82, flags=Flags.NOP,
                comment='break permitted here'),
    seq_CONTROL('NBH', 0x83, flags=Flags.NOP,
                comment='no break permitted here'),
    seq_CONTROL('IND', 0x84,
                comment='index'),
    seq_CONTROL('NEL', 0x85,
                comment='next line'),
    seq_CONTROL('SSA', 0x86, flags=Flags.NOP,
                comment='start of selected area'),
    seq_CONTROL('ESA', 0x87, flags=Flags.NOP,
                comment='end of selected area'),
    seq_CONTROL('HTS', 0x88,
                comment='horizontal tab set'),
    seq_CONTROL('HTJ', 0x89,
                comment='character tabulation with justification'),
    seq_CONTROL('VTS', 0x8a, flags=Flags.NOP,
                comment='line tabulation set'),
    seq_CONTROL('PLD', 0x8b, flags=Flags.NOP,
                comment='partial line forward'),
    seq_CONTROL('PLU', 0x8c, flags=Flags.NOP,
                comment='partial line backward'),
    seq_CONTROL('RI', 0x8d,
                comment='reverse index'),
    seq_CONTROL('SS2', 0x8e, source=Source.ECMA35,
                comment='single shift 2'),
    seq_CONTROL('SS3', 0x8f, source=Source.ECMA35,
                comment='single shift 3'),
    # seq_CONTROL('DCS', 0x90, flags=Flags.NOP, source=Sources.ECMA35,
    #             comment='device control string'),
    seq_CONTROL('PU1', 0x91, flags=Flags.NOP,
                comment='private use 1'),
    seq_CONTROL('PU2', 0x92, flags=Flags.NOP,
                comment='private use 2'),
    seq_CONTROL('STS', 0x93, flags=Flags.NOP,
                comment='set transmit state'),
    seq_CONTROL('CCH', 0x94, flags=Flags.NOP,
                comment='cancel character'),
    seq_CONTROL('MW', 0x95, flags=Flags.NOP,
                comment='message waiting'),
    seq_CONTROL('SPA', 0x96, flags=Flags.NOP,
                comment='start of protected area'),
    seq_CONTROL('EPA', 0x97, flags=Flags.NOP,
                comment='end of protected area'),
    # seq_CONTROL('SOS', 0x98, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='start of string'),
    # seq_CONTROL('SOS', 0x9a, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='single character introducer'),
    # seq_CONTROL('CSI', 0x9b, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='control sequence introducer'),
    seq_CONTROL('ST', 0x9c, flags=Flags.NOP,
                comment='string terminator'),
    # seq_CONTROL('OSC', 0x9d, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='operating system command'),
    # seq_CONTROL('PM', 0x9e, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='privay message'),
    # seq_CONTROL('APC', 0x9f, flags=Flags.NOP, source=Source.ECMA35,
    #             comment='application program command'),

    # Escape sequences
    seq_ESCAPE('DECDHL_TH', '3', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='double width double height line: top half'),
    seq_ESCAPE('DECDHL_BH', '4', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='double width double height line: bottom half'),
    seq_ESCAPE('DECSWL', '5', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='single width single height line'),
    seq_ESCAPE('DECBI', '6',
               comment='back index'),
    seq_ESCAPE('DECDWL', '6', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='double width single height line'),
    seq_ESCAPE('DECSC', '7',
               comment='save cursor'),
    seq_ESCAPE('DECRC', '8',
               comment='restore cursor'),
    seq_ESCAPE('DECALN', '8', intermediates=(Intermediate.HASH,),
               comment='screen alignment pattern'),
    seq_ESCAPE('DECFI', '9',
               comment='forward index'),
    seq_ESCAPE('WYDHL_TH', ':', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='single width double height line: top half'),
    seq_ESCAPE('WYDHL_BH', ';', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
               comment='single width double height line: bottom half'),
    seq_ESCAPE('DECANM', '<', flags=Flags.NOP,
               comment='ansi mode'),
    seq_ESCAPE('DECKPAM', '=',
               comment='keypad application mode'),
    seq_ESCAPE('DECKPNM', '>',
               comment='keypad numeric mode'),
    seq_ESCAPE('BPH', 'B', flags=Flags.NOP,
               comment='break permitted here'),
    seq_ESCAPE('NBH', 'C', flags=Flags.NOP,
               comment='no break permitted here'),
    seq_ESCAPE('IND', 'D',
               comment='index'),
    seq_ESCAPE('NEL', 'E',
               comment='next line'),
    seq_ESCAPE('SSA', 'F', flags=Flags.NOP,
               comment='start of selected area'),
    seq_ESCAPE('ESA', 'G', flags=Flags.NOP,
               comment='end of selected area'),
    seq_ESCAPE('HTS', 'H',
               comment='horizontal tab set'),
    seq_ESCAPE('HTJ', 'I',
               comment='character tabulation with justification'),
    seq_ESCAPE('VTS', 'J', flags=Flags.NOP,
               comment='line tabulation set'),
    seq_ESCAPE('PLD', 'K', flags=Flags.NOP,
               comment='partial line forward'),
    seq_ESCAPE('PLU', 'L', flags=Flags.NOP,
               comment='partial line backward'),
    seq_ESCAPE('RI', 'M',
               comment='reverse index'),
    seq_ESCAPE('SS2', 'N',
               comment='single shift 2'),
    seq_ESCAPE('SS3', 'O',
               comment='single shift 3'),
    seq_ESCAPE('PU1', 'Q', flags=Flags.NOP,
               comment='private use 1'),
    seq_ESCAPE('PU2', 'R', flags=Flags.NOP,
               comment='private use 2'),
    seq_ESCAPE('STS', 'S', flags=Flags.NOP,
               comment='set transmit state'),
    seq_ESCAPE('CCH', 'T', flags=Flags.NOP,
               comment='cancel character'),
    seq_ESCAPE('MW', 'U', flags=Flags.NOP,
               comment='message waiting'),
    seq_ESCAPE('SPA', 'V', flags=Flags.NOP,
               comment='start of protected area'),
    seq_ESCAPE('EPA', 'W', flags=Flags.NOP,
               comment='end of protected area'),
    seq_ESCAPE('ST', '\\', flags=Flags.NOP,
               comment='string terminator'),
    seq_ESCAPE('DMI', '`', flags=Flags.NOP,
               comment='disable manual input'),
    seq_ESCAPE('INT', 'a', flags=Flags.NOP,
               comment='interrupt'),
    seq_ESCAPE('EMI', 'b', flags=Flags.NOP,
               comment='enable manual input'),
    seq_ESCAPE('RIS', 'c',
               comment='reset to initial state'),
    seq_ESCAPE('CMD', 'd', flags=Flags.NOP,
               comment='coding method delimiter'),
    seq_ESCAPE('XTERM_MLHP', 'l', flags=Flags.NOP,
               comment='xterm memory lock hp bugfix'),
    seq_ESCAPE('XTERM_MUHP', 'm', flags=Flags.NOP,
               comment='xterm memory unlock hp bugfix'),
    seq_ESCAPE('LS2', 'n',
               comment='locking shift 2'),
    seq_ESCAPE('LS3', 'o',
               comment='locking shift 3'),
    seq_ESCAPE('LS3R', '|',
               comment='locking shift 3 right'),
    seq_ESCAPE('LS2R', '}',
               comment='locking shift 2 right'),
    seq_ESCAPE('LS1R', '~',
               comment='locking shift 1 right'),

    # CSI sequences
    seq_CSI('ICH', '@',
            comment='insert character'),
    seq_CSI('SL', '@', intermediates=(Intermediate.SPACE,),
            comment='scroll left'),
    seq_CSI('CUU', 'A',
            comment='cursor up'),
    seq_CSI('SR', 'A', intermediates=(Intermediate.SPACE,),
            comment='scroll right'),
    seq_CSI('CUD', 'B',
            comment='cursor down'),
    seq_CSI('GSM', 'B', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='graphic size modification'),
    seq_CSI('CUF', 'C',
            comment='cursor forward'),
    seq_CSI('GSS', 'C', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='graphic size selection'),
    seq_CSI('CUB', 'D',
            comment='cursor backward'),
    seq_CSI('FNT', 'D', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='font selection'),
    seq_CSI('CNL', 'E',
            comment='cursor next line'),
    seq_CSI('CPL', 'F',
            comment='cursor previous line'),
    seq_CSI('JFY', 'F', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='justify'),
    seq_CSI('TSS', 'E', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='thine space specification'),
    seq_CSI('CHA', 'G',
            comment='cursor horizontal absolute'),
    seq_CSI('SPI', 'G', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='spacing increment'),
    seq_CSI('CUP', 'H',
            comment='cursor position'),
    seq_CSI('QUAD', 'H', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='quad'),
    seq_CSI('CHT', 'I',
            comment='cursor horizontal forward tabulation'),
    seq_CSI('SSU', 'I', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set size unit'),
    seq_CSI('ED', 'J',
            comment='erase in display'),
    seq_CSI('PFS', 'J', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='page format selection'),
    seq_CSI('DECSED', 'J', pintro=(ParameterIntro.WHAT,),
            comment='selective erase in display'),
    seq_CSI('EL', 'K',
            comment='erase in line'),
    seq_CSI('SHS', 'K', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='select character spacing'),
    seq_CSI('DECSEL', 'K', pintro=(ParameterIntro.WHAT,),
            comment='selective erase in line'),
    seq_CSI('IL', 'L',
            comment='insert line'),
    seq_CSI('SVS', 'L', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='select line spacing'),
    seq_CSI('DL', 'M',
            comment='delete line'),
    seq_CSI('IGS', 'M', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='identify graphic subrepertoire'),
    seq_CSI('EF', 'N', flags=Flags.NOP,
            comment='erase in field'),
    seq_CSI('EA', 'O', flags=Flags.NOP,
            comment='erase in area'),
    seq_CSI('IDCS', 'O', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='identify DCS'),
    seq_CSI('DCH', 'P',
            comment='delete character'),
    seq_CSI('PPA', 'P', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='page position absolute'),
    seq_CSI('XTERM_PUSHCOLORS', 'P', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm push color palette stack'),
    seq_CSI('SEE', 'Q', flags=Flags.NOP,
            comment='select editing extent'),
    seq_CSI('PPR', 'Q', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='page position relative'),
    seq_CSI('XTERM_POPCOLORS', 'Q', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm pop color palette stack'),
    seq_CSI('PPB', 'R', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='page position backward'),
    seq_CSI('XTERM_REPORTCOLORS', 'R', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm Report color palette stack'),
    seq_CSI('SU', 'S',
            comment='scroll up'),
    seq_CSI('SPD', 'S', intermediates=(Intermediate.SPACE,),
            comment='select presentation directions'),
    seq_CSI('XTERM_SMGRAPHICS', 'S', pintro=(ParameterIntro.WHAT,),
            comment='xterm graphics attributes'),
    seq_CSI('SD', 'T',
            comment='scroll down'),
    seq_CSI('XTERM_IHMT', 'T', flags=Flags.NOP,
            comment='xterm initiate highlight mouse tracking'),
    seq_CSI('DTA', 'T', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='dimension text area'),
    seq_CSI('MINTTY_SD', 'T', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='mintty unscroll'),
    seq_CSI('XTERM_RTM', 'T', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='xterm reset title mode'),
    seq_CSI('NP', 'U', flags=Flags.NOP,
            comment='next page'),
    seq_CSI('SLH', 'U', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set line home'),
    seq_CSI('PP', 'V', flags=Flags.NOP,
            comment='preceding page'),
    seq_CSI('SLL', 'V', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set line limit'),
    seq_CSI('CTC', 'W',
            comment='cursor tabulation control'),
    seq_CSI('FNK', 'W', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='function key'),
    seq_CSI('DECST8C', 'W', pintro=(ParameterIntro.WHAT,),
            comment='set tab at every 8 columns'),
    seq_CSI('ECH', 'X',
            comment='erase character'),
    seq_CSI('SPQR', 'X', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='select print quality and rapidity'),
    seq_CSI('CVT', 'Y', flags=Flags.NOP,
            comment='cursor line tabulation'),
    seq_CSI('SEF', 'Y', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='sheet eject and feed'),
    seq_CSI('CBT', 'Z',
            comment='cursor backward tabulation'),
    seq_CSI('PEC', 'Z', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='presentation expand or contract'),
    seq_CSI('SRS', '[', flags=Flags.NOP,
            comment='start reversed string'),
    seq_CSI('SSW', '[', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set space width'),
    seq_CSI('PTX', '\\', flags=Flags.NOP,
            comment='parallel texts'),
    seq_CSI('SACS', '\\', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set additional character separation'),
    seq_CSI('SDS', ']', flags=Flags.NOP,
            comment='start directed string'),
    seq_CSI('SAPV', ']', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='select alternative presentation variants'),
    seq_CSI('SIMD', '^', flags=Flags.NOP,
            comment='select implicit movement direction'),
    seq_CSI('STAB', '^', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='selective tabulation'),
    seq_CSI('GCC', '_', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='graphic character combination'),
    seq_CSI('HPA', '`',
            comment='horizontal position absolute'),
    seq_CSI('TATE', '`', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='tabulation aligned trailing edge'),
    seq_CSI('HPR', 'a',
            comment='horizontal position relative'),
    seq_CSI('TALE', 'a', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='tabulation aligned leading edge'),
    seq_CSI('REP', 'b',
            comment='repeat'),
    seq_CSI('TAC', 'b', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='tabulation aligned centre'),
    seq_CSI('DA1', 'c',
            comment='primary device attributes'),
    seq_CSI('TCC', 'c', intermediates=(Intermediate.SPACE,),
            comment='tabulation centred on character'),
    seq_CSI('DA3', 'c', pintro=(ParameterIntro.EQUAL,),
            comment='tertiary device attributes'),
    seq_CSI('DA2', 'c', pintro=(ParameterIntro.GT,),
            comment='secondary device attributes'),
    seq_CSI('VPA', 'd',
            comment='vertical line position absolute'),
    seq_CSI('TSR', 'd', intermediates=(Intermediate.SPACE,),
            comment='tabulation stop remove'),
    seq_CSI('VPR', 'e',
            comment='vertical line position relative'),
    seq_CSI('SCO', 'e', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='select character orientation'),
    seq_CSI('HVP', 'f',
            comment='horizontal and vertical position'),
    seq_CSI('SRCS', 'f', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set reduced character separation'),
    seq_CSI('TBC', 'g',
            comment='tab clear'),
    seq_CSI('SCS', 'g', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set character spacing'),
    seq_CSI('DECLFKC', 'g', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='local function key control'),
    seq_CSI('SM_ECMA', 'h',
            comment='set mode ecma'),
    seq_CSI('SLS', 'h', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set line spacing'),
    seq_CSI('SM_HP', 'h', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='set mode hp'),
    seq_CSI('SM_DEC', 'h', pintro=(ParameterIntro.WHAT,),
            comment='set mode dec'),
    seq_CSI('MC_ECMA', 'i', flags=Flags.NOP,
            comment='media copy ecma'),
    seq_CSI('SPH', 'i', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set page home'),
    seq_CSI('MC_DEC', 'i', pintro=(ParameterIntro.WHAT,), flags=Flags.NOP,
            comment='media copy dec'),
    seq_CSI('HPB', 'j', flags=Flags.NOP,
            comment='horizontal position backward'),
    seq_CSI('SPL', 'j', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set page limit'),
    seq_CSI('VPB', 'k', flags=Flags.NOP,
            comment='line position backward'),
    seq_CSI('SCP', 'k', intermediates=(Intermediate.SPACE,),
            comment='select character path'),
    seq_CSI('RM_ECMA', 'l',
            comment='reset mode ecma'),
    seq_CSI('RM_HP', 'l', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='reset mode hp'),
    seq_CSI('RM_DEC', 'l', pintro=(ParameterIntro.WHAT,),
            comment='reset mode dec'),
    seq_CSI('SGR', 'm',
            comment='select graphics rendition'),
    seq_CSI('DECSGR', 'm', pintro=(ParameterIntro.WHAT,),
            comment='DEC select graphics rendition'),
    seq_CSI('XTERM_MODKEYS', 'm', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='xterm set key modifier options'),
    seq_CSI('DSR_ECMA', 'n',
            comment='device status report ecma'),
    seq_CSI('XTERM_RRV', 'n', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='xterm reset resource value'),
    seq_CSI('DSR_DEC', 'n', pintro=(ParameterIntro.WHAT,),
            comment='device status report dec'),
    seq_CSI('DAQ', 'o', flags=Flags.NOP,
            comment='define area qualification'),
    seq_CSI('DECSSL', 'p', flags=Flags.NOP,
            comment='select setup language'),
    seq_CSI('DECSSCLS', 'p', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set scroll speed'),
    seq_CSI('DECSTR', 'p', intermediates=(Intermediate.BANG,),
            comment='soft terminal reset'),
    seq_CSI('DECSCL', 'p', intermediates=(Intermediate.DQUOTE,),
            comment='select conformance level'),
    # This will remain a NOP even if VTE ever implements XTERM_PUSHSGR, see
    # https://gitlab.gnome.org/GNOME/vte/-/issues/23#note_513969
    seq_CSI('XTERM_PUSHSGR', 'p', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm push SGR stack'),
    seq_CSI('DECRQM_ECMA', 'p', intermediates=(Intermediate.CASH,),
            comment='request mode ecma'),
    seq_CSI('DECSDPT', 'p', intermediates=(Intermediate.PCLOSE,), flags=Flags.NOP,
            comment='select digital printed data type'),
    seq_CSI('DECSPPCS', 'p', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='select pro printer character set'),
    seq_CSI('DECSR', 'p', intermediates=(Intermediate.PLUS,),
            comment='secure reset'),
    seq_CSI('DECLTOD', 'p', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='load time of day'),
    seq_CSI('DECARR', 'p', intermediates=(Intermediate.MINUS,), flags=Flags.NOP,
            comment='auto repeat rate'),
    seq_CSI('XTERM_PTRMODE', 'p', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='xterm set pointer mode'),
    seq_CSI('DECRQM_DEC', 'p', pintro=(ParameterIntro.WHAT,), intermediates=(Intermediate.CASH,),
            comment='request mode dec'),
    seq_CSI('DECLL', 'q', flags=Flags.NOP,
            comment='load leds'),
    seq_CSI('DECSCUSR', 'q', intermediates=(Intermediate.SPACE,),
            comment='set cursor style'),
    seq_CSI('DECSCA', 'q', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='select character protection attribute'),
    # See comment above for XTERM_PUSHSGR
    seq_CSI('XTERM_POPSGR', 'q', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm pop SGR stack'),
    seq_CSI('DECSDDT', 'q', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='select disconnect delay time'),
    seq_CSI('MINTTY_PROGRESS', 'q', intermediates=(Intermediate.PERCENT,), flags=Flags.NOP,
            comment='set progress report'),
    seq_CSI('DECSR', 'q', intermediates=(Intermediate.MULT,),
            comment='secure reset'),
    seq_CSI('DECELF', 'q', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='enable local functions'),
    seq_CSI('DECTID', 'q', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='select terminal id'),
    seq_CSI('DECCRTST', 'q', intermediates=(Intermediate.MINUS,), flags=Flags.NOP,
            comment='CRT saver time'),
    seq_CSI('XTERM_VERSION', 'q', pintro=(ParameterIntro.GT,),
            comment='request xterm version report'),
    seq_CSI('DECSTBM', 'r',
            comment='set top and bottom margins'),
    seq_CSI('DECSKCV', 'r', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set key click volume'),
    seq_CSI('DECCARA', 'r', intermediates=(Intermediate.CASH,),
            comment='change attributes in rectangular area'),
    seq_CSI('DECSCS', 'r', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='select communication speed'),
    seq_CSI('DECSMKR', 'r', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='select modifier key reporting'),
    seq_CSI('DECSEST', 'r', intermediates=(Intermediate.MINUS,), flags=Flags.NOP,
            comment='energy saver time'),
    seq_CSI('DECPCTERM', 'r', pintro=(ParameterIntro.WHAT,), flags=Flags.NOP,
            comment='pcterm'),
    seq_CSI('XTERM_RPM', 'r', pintro=(ParameterIntro.WHAT,),
            comment='xterm restore DEC private mode'),
    seq_CSI('DECSLRM', 's',
            comment='set left and right margins'),
    seq_CSI('SCOSC', 's',
            comment='SCO save cursor'),
    seq_CSI('DECSPRTT', 's', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='select printer type'),
    seq_CSI('DECSFC', 's', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='select flow control'),
    seq_CSI('XTERM_SHIFTESCAPE', 's', pintro=(ParameterIntro.GT,),
            comment='xterm set shift-escape'),
    seq_CSI('XTERM_SPM', 's', pintro=(ParameterIntro.WHAT,),
            comment='xterm save private mode'),
    seq_CSI('DECSLPP', 't',
            comment='set lines per page'),
    seq_CSI('XTERM_WM', 't',
            comment='xterm window management'),
    seq_CSI('DECSWBV', 't', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set warning bell volume'),
    seq_CSI('DECSRFR', 't', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='select refresh rate'),
    seq_CSI('DECRARA', 't', intermediates=(Intermediate.CASH,),
            comment='reverse attributes in rectangular area'),
    seq_CSI('XTERM_STM', 't', pintro=(ParameterIntro.GT,), flags=Flags.NOP,
            comment='xterm set title mode'),
    seq_CSI('SCORC', 'u',
            comment='SCO restore cursor'),
    seq_CSI('DECSMBV', 'u', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set margin bell volume'),
    seq_CSI('DECSTRL', 'u', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='set transmit rate limit'),
    seq_CSI('DECRQTSR', 'u', intermediates=(Intermediate.CASH,),
            comment='request terminal state report'),
    seq_CSI('DECSCP', 'u', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='select communication port'),
    seq_CSI('DECRQKT', 'u', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='request key type'),
    seq_CSI('DECRQUPSS', 'u', pintro=(ParameterIntro.WHAT,), flags=Flags.NOP,
            comment='request user preferred supplemental set'),
    seq_CSI('DECSLCK', 'v', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='set lock key style'),
    seq_CSI('DECRQDE', 'v', intermediates=(Intermediate.DQUOTE,),
            comment='request display extent'),
    seq_CSI('DECCRA', 'v', intermediates=(Intermediate.CASH,),
            comment='copy rectangular area'),
    seq_CSI('DECRPKT', 'v', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            direction = Direction.TTH,
            comment='report key type'),
    seq_CSI('WYCAA', 'w', flags=Flags.NOP,
            comment='redefine character display attribute association'),
    seq_CSI('DECRPDE', 'w', intermediates=(Intermediate.DQUOTE,),
            direction = Direction.TTH,
            comment='report displayed extent'),
    seq_CSI('DECRQPSR', 'w', intermediates=(Intermediate.CASH,),
            comment='request presentation state report'),
    seq_CSI('DECEFR', 'w', intermediates=(Intermediate.SQUOTE,), flags=Flags.NOP,
            comment='enable filter rectangle'),
    seq_CSI('DECSPP', 'w', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='set port parameter'),
    seq_CSI('DECREQTPARM', 'x',
            comment='request terminal parameters'),
    seq_CSI('WYCDIR', 'x', flags=Flags.NOP,
            comment='set current character attributes'),
    seq_CSI('DECFRA', 'x', intermediates=(Intermediate.CASH,),
            comment='fill rectangular area'),
    seq_CSI('DECES', 'x', intermediates=(Intermediate.AND,), flags=Flags.NOP,
            comment='enable session'),
    seq_CSI('DECSACE', 'x', intermediates=(Intermediate.MULT,),
            comment='select attribute change extent'),
    seq_CSI('DECRQPKFM', 'x', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='request program key free memory'),
    seq_CSI('DECSPMA', 'x', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='session page memory allocation'),
    seq_CSI('DECTST', 'y', flags=Flags.NOP,
            comment='invoke confidence test'),
    seq_CSI('XTERM_CHECKSUM_MODE', 'y', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm DECRQCRA checksum mode'),
    seq_CSI('DECRQCRA', 'y', intermediates=(Intermediate.MULT,),
            comment='request checksum of rectangular area'),
    seq_CSI('DECPKFMR', 'y', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='program key free memory report'),
    seq_CSI('DECUS', 'y', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='update session'),
    seq_CSI('WYSCRATE', 'z', flags=Flags.NOP,
            comment='set smooth scroll rate'),
    seq_CSI('DECERA', 'z', intermediates=(Intermediate.CASH,),
            comment='erase rectangular area'),
    seq_CSI('DECELR', 'z', intermediates=(Intermediate.SQUOTE,), flags=Flags.NOP,
            comment='enable locator reporting'),
    seq_CSI('DECINVM', 'z', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='invoke macro'),
    seq_CSI('DECPKA', 'z', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='program key action'),
    seq_CSI('DECDLDA', 'z', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='down line load allocation'),
    seq_CSI('XTERM_PUSHSGR', '{', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm push SGR stack'),
    seq_CSI('DECSERA', '{', intermediates=(Intermediate.CASH,),
            comment='selective erase rectangular area'),
    seq_CSI('DECSLE', '{', intermediates=(Intermediate.SQUOTE,), flags=Flags.NOP,
            comment='select locator events'),
    seq_CSI('DECSTGLT', '{', intermediates=(Intermediate.PCLOSE,), flags=Flags.NOP,
            comment='select color lookup table'),
    seq_CSI('DECSZS', '{', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='select zero symbol'),
    seq_CSI('XTERM_REPORTSGR', '|', intermediates=(Intermediate.HASH,),
            comment='xterm SGR report'),
    seq_CSI('DECSCPP', '|', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='select columns per page'),
    seq_CSI('DECRQLP', '|', intermediates=(Intermediate.SQUOTE,), flags=Flags.NOP,
            comment='request locator position'),
    seq_CSI('DECSNLS', '|', intermediates=(Intermediate.MULT,), flags=Flags.NOP,
            comment='set lines per screen'),
    seq_CSI('DECAC', '|', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='assign color'),
    seq_CSI('DECKBD', '}', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='keyboard language selection'),
    seq_CSI('XTERM_POPSGR', '}', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='xterm pop SGR stack'),
    seq_CSI('DECSASD', '}', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='select active status display'),
    seq_CSI('DECIC', '}', intermediates=(Intermediate.SQUOTE,),
            comment='insert column'),
    seq_CSI('DECATC', '}', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='alternate text color'),
    seq_CSI('DECFNK', '~', direction=Direction.BIDI, flags=Flags.NOP,
            comment='dec function key / XTERM bracketed paste'),
    seq_CSI('DECTME', '~', intermediates=(Intermediate.SPACE,), flags=Flags.NOP,
            comment='terminal mode emulation'),
    seq_CSI('DECSSDT', '~', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='select status display line type'),
    seq_CSI('DECDC', '~', intermediates=(Intermediate.SQUOTE,),
            comment='delete column'),
    seq_CSI('DECPS', '~', intermediates=(Intermediate.COMMA,), flags=Flags.NOP,
            comment='play sound'),

    # DCS sequences
    seq_DCS('XTERM_GETXRES', 'Q', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='xterm get X resource'),
    seq_DCS('RLOGIN_MML', 'm', intermediates=(Intermediate.HASH,), flags=Flags.NOP,
            comment='RLogin music macro language'),
    seq_DCS('DECREGIS', 'p', flags=Flags.NOP,
            comment='ReGIS graphics'),
    seq_DCS('DECRSTS', 'p', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='restore terminal state'),
    seq_DCS('XTERM_STCAP', 'p', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='xterm set termcap/terminfo'),
    seq_DCS('DECSIXEL', 'q', flags=Flags.NOP,
            comment='SIXEL graphics'),
    seq_DCS('DECRQSS', 'q', intermediates=(Intermediate.CASH,),
            comment='request selection or setting'),
    seq_DCS('XTERM_RQTCAP', 'q', intermediates=(Intermediate.PLUS,), flags=Flags.NOP,
            comment='xterm request termcap/terminfo'),
    seq_DCS('DECLBAN', 'r', flags=Flags.NOP,
            comment='load banner message'),
    seq_DCS('DECTSR', 's', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            direction = Direction.TTH,
            comment='terminal state report'),
    seq_DCS('XDGSYNC', 's', pintro=(ParameterIntro.EQUAL,), flags=Flags.NOP,
            comment='synchronous update'),
    seq_DCS('DECRSPS', 't', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            comment='restore presentation state'),
    seq_DCS('DECAUPSS', 'u', intermediates=(Intermediate.BANG,), flags=Flags.NOP,
            comment='assign user preferred supplemental sets'),
    seq_DCS('DECPSR', 'u', intermediates=(Intermediate.CASH,), flags=Flags.NOP,
            direction = Direction.TTH,
            comment='presentation state report'),
    seq_DCS('DECLANS', 'v', flags=Flags.NOP,
            comment='load answerback message'),
    seq_DCS('DECLBD', 'w', flags=Flags.NOP,
            comment='locator button define'),
    seq_DCS('DECPFK', 'x', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='program function key'),
    seq_DCS('DECPAK', 'y', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='program alphanumeric key'),
    seq_DCS('DECDMAC', 'z', intermediates=(Intermediate.BANG,), flags=Flags.NOP,
            comment='define macro'),
    seq_DCS('DECCKD', 'z', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            comment='copy key default'),
    seq_DCS('DECDLD', '{', flags=Flags.NOP,
            comment='dynamically redefinable character sets extension'),
    seq_DCS('DECSTUI', '{', intermediates=(Intermediate.BANG,), flags=Flags.NOP,
            comment='set terminal unit id'),
    seq_DCS('DECUDK', '|', flags=Flags.NOP,
            comment='user defined keys'),
    seq_DCS('WYLSFNT', '}', flags=Flags.NOP,
            comment='load soft font'),
    seq_DCS('DECRPFK', '}', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            direction=Direction.TTH,
            comment='report function key definition'),
    seq_DCS('DECRPAK', '~', intermediates=(Intermediate.DQUOTE,), flags=Flags.NOP,
            direction=Direction.TTH,
            comment='report all modifier/alphanumeric key state'),

    # SCI sequences

    # APC sequences

    # PM sequences

    # SOS sequences
    ]


# Output generator

'''
Returns: Dict[Sequence, List[NamedSequence]]
'''
def get_sequences(predicate):
    result={}
    for nseq in sequences:
        if predicate(nseq.seq):
            if nseq.seq in result:
                result[nseq.seq]+=[nseq]
            else:
                result[nseq.seq]=[nseq]

    return result

'''
Returns: Dict[str, Tuple[Flags, Direction, str]] mapping command name to (flags, direction, comment)
'''
def get_commands(predicate):
    cmds={}

    all_seqs = get_sequences(predicate)
    for _seq in all_seqs:
        seq_list = all_seqs[_seq]
        for seq in seq_list:
            if seq.name in cmds:
                flags, direction, comment = cmds[seq.name]

                if flags != seq.flags:
                    raise ValueError(f'{seq.name} flags inconsistent: {seq.flags} vs {flags}')
                if direction != seq.direction:
                    raise ValueError(f'{seq.name} direction inconsistent: {seq.direction} vs {direction}')
                if comment != seq.comment:
                    raise ValueError(f'{seq.name} comment inconsistent: {seq.comment} vs {comment}')

            cmds[seq.name]=(seq.flags, seq.direction, seq.comment)

        # Add an extra entry for the disambiguation command
        if len(seq_list) > 1:
            sorted_seqs = sorted(seq_list, key=lambda seq: seq.name)

            or_name='_OR_'.join(tuple([seq.name for seq in sorted_seqs]))
            or_comment=' or '.join(tuple([seq.comment for seq in sorted_seqs]))

            or_flags=None
            or_direction=Direction.HTT
            for seq in sorted_seqs:
                flags=seq.flags
                if flags is None or or_flags is None:
                    or_flags = None
                else:
                    or_flags &= flags

                or_direction |= seq.direction

            cmds[or_name] = (or_flags, or_direction, or_comment)

    return cmds

'''
Returns: Dict[Sequence, Tuple[Type, str, int, Tuple[ParameterIntro], Tuple[Intermediate], Flags, str)
'''
def get_seqs(predicate):
    seqs={}

    all_seqs = get_sequences(predicate)
    for seq in all_seqs:
        seq_list = all_seqs[seq]

        if len(seq_list) > 1:
            sorted_seqs = sorted(seq_list, key=lambda nseq: nseq.name)

            name='_OR_'.join(tuple([nseq.name for nseq in sorted_seqs]))
            comment=' or '.join(tuple([nseq.comment for nseq in sorted_seqs]))

            flags=None
            for nseq in sorted_seqs:
                _flags = nseq.flags
                if _flags is None or flags is None:
                    flags = None
                else:
                    flags &= _flags

        else:
            name=seq_list[0].name
            comment=seq_list[0].comment
            flags=seq_list[0].flags
            direction=seq_list[0].direction

        seqs[seq] = (seq.stype, name, seq.final, seq.pintro, seq.intermediates, flags, direction, comment)

    return seqs

''' Write copyright header '''
def write_header(outfile):
    outfile.write('''
/* Generated by parser-seq.py; do not edit! */

/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
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
def write_seqs(output, stype, sdir=Direction.HTT):
    outfile = open(output.as_posix(), 'w')
    write_header(outfile)
    outfile.write('''
#if !defined(_VTE_SEQ) || !defined(_VTE_NOQ)
#error "Must define _VTE_SEQ and _VTE_NOQ before including this file"
#endif

''')

    def name_or_none(e):
        if e is None:
            return 'NONE'
        else:
            return e.name

    def final_char(c):
        if c == 0x5c:
            return "'\\\\'"
        elif c >= 0x20 and c <= 0x7f:
            return f"'{c:c}'"
        else:
            return f'0x{c:02x}'

    def flags_to_dispatch_flags(flags):
        if flags is not None and flags & Flags.UNRIPE:
            return "VTE_DISPATCH_UNRIPE"
        return "0"

    seqs = get_seqs(lambda seq: seq.stype == stype)
    for seq in seqs:
        stype, name, final, pintro, intermediates, flags, direction, comment = seqs[seq]

        if len(intermediates) > 2:
            raise ValueError('{name} has too many intermediates')
        elif len(intermediates) == 1:
            intermediate0 = intermediates[0]
        else:
            intermediate0 = None

        if len(pintro) == 0:
            pintro0 = None
        else:
            pintro0 = pintro[0]

        if flags is not None and flags & Flags.NOP:
            macro = '_VTE_NOQ'
        else:
            macro = '_VTE_SEQ'

        if not direction & sdir:
            continue

        outfile.write(f'{macro}('
                      f'{name}, '
                      f'{stype.name}, '
                      f'{final_char(final)}, '
                      f'{name_or_none(pintro0)}, '
                      f'{len(intermediates)}, '
                      f'{name_or_none(intermediate0)}, '
                      f'{flags_to_dispatch_flags(flags)} '
                      f') '
                      f'/* {comment} */\n')


''' Write commands '''
def write_cmds(output):
    outfile = open(output.as_posix(), 'w')
    write_header(outfile)
    outfile.write('''
#if !defined(_VTE_CMD) || !defined(_VTE_NOP)
#error "Must define _VTE_CMD and _VTE_NOP before including this file"
#endif
''')

    outfile.write('/* Implemented in VTE: */\n')
    outfile.write('''
_VTE_CMD(NONE) /* placeholder */
_VTE_CMD(GRAPHIC) /* graphics character */
''')

    cmds = get_commands(lambda seq: True)
    for name in sorted(cmds):
        flags, direction, comment = cmds[name]
        if not direction & Direction.HTT:
            continue

        if flags is None or not (flags & Flags.NOP):
            if comment is not None:
                outfile.write(f'_VTE_CMD({name}) /* {comment} */\n')
            else:
                outfile.write(f'_VTE_CMD({name})\n')

    outfile.write('/* Unimplemented in VTE: */\n')
    for name in sorted(cmds):
        flags, direction, comment = cmds[name]
        if not direction & Direction.HTT:
            continue

        if flags is not None and flags & Flags.NOP:
            if comment is not None:
                outfile.write(f'_VTE_NOP({name}) /* {comment} */\n')
            else:
                outfile.write(f'_VTE_NOP({name})\n')


''' Write command handlers '''
def write_hdlr(output):

    def cmd_handler_macro(flags):
        if flags is None:
            return '_VTE_CMD_HANDLER'
        elif flags & Flags.NOP:
            return '_VTE_CMD_HANDLER_NOP'
        elif flags & Flags.HANDLER_RV:
            return '_VTE_CMD_HANDLER_R'
        else:
            return '_VTE_CMD_HANDLER'

    outfile = open(output.as_posix(), 'w')
    write_header(outfile)
    outfile.write('''
#if !defined(_VTE_CMD_HANDLER) || !defined(_VTE_CMD_HANDLER_R) || !defined(_VTE_CMD_HANDLER_NOP)
#error "Must define _VTE_CMD_HANDLER, _VTE_CMD_HANDLER_R and _VTE_CMD_HANDLER_NOP before including this file"
#endif
''')

    outfile.write('''
_VTE_CMD_HANDLER_NOP(NONE) /* placeholder */
_VTE_CMD_HANDLER(GRAPHIC) /* graphics character */
''')

    cmds = get_commands(lambda seq: True)
    for name in sorted(cmds):
        flags, direction, comment = cmds[name]
        if not direction & Direction.HTT:
            continue
        if comment is not None:
            outfile.write(f'{cmd_handler_macro(flags)}({name}) /* {comment} */\n')
        else:
            outfile.write(f'{cmd_handler_macro(flags)}({name})\n')


# main

''' main '''
if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='parser sequences list generator')
    parser.add_argument('--destdir',
                        type=pathlib.Path,
                        default=pathlib.PosixPath('.'),
                        help='Output directory')

    try:
        args = parser.parse_args()
    except Exception as e:
        print(f'Failed to parse arguments: {e}')
        sys.exit(1)

    write_seqs(args.destdir / "parser-c01.hh", Type.CONTROL)
    write_seqs(args.destdir / "parser-esc.hh", Type.ESCAPE)
    write_seqs(args.destdir / "parser-csi.hh", Type.CSI)
    write_seqs(args.destdir / "parser-dcs.hh", Type.DCS)
    write_seqs(args.destdir / "parser-sci.hh", Type.SCI)

    write_cmds(args.destdir / "parser-cmd.hh")
    write_hdlr(args.destdir / "parser-cmd-handlers.hh")
