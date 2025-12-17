# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import NamedTuple, Optional, Sequence, Union
from enum import Enum, auto
import textwrap


class ToolDesc(NamedTuple):
    name: str
    title: str
    description: str
    short_usage: str
    detail: Optional[str] = None
    argument: Optional[str] = None
    reorder_arguments: bool = False

    @classmethod
    def from_json(cls, obj):
        return cls(name=obj['name'],
                   title=obj['title'],
                   description=obj['description'],
                   detail=obj['detail'],
                   short_usage=obj['short-usage'],
                   argument=obj.get('argument'),
                   reorder_arguments=obj.get('reorder-arguments', False))


class ArgumentType(Enum):
    STRING = auto()
    NUMBER = auto()
    FILE = auto()
    KEYWORD = auto()

    @classmethod
    def from_json(cls, obj):
        return cls.__members__[obj.upper()]

    def get_name(self) -> str:
        ARG_TYPE_TO_VALUE = {
            ArgumentType.STRING: 'str',
            ArgumentType.NUMBER: 'num',
            ArgumentType.FILE: 'file',
            ArgumentType.KEYWORD: 'arg',
        }
        return ARG_TYPE_TO_VALUE[self]


class Range(NamedTuple):
    minimum: Optional[int]
    maximum: Optional[int]

    @classmethod
    def from_json(cls, obj):
        default = cls.default()
        return cls(minimum=obj.get('min', default.minimum),
                   maximum=obj.get('max', default.maximum))


class SignedRange(Range):
    @classmethod
    def default(cls):
        return cls(minimum=-2**31-1, maximum=2**31)


class UnsignedRange(Range):
    @classmethod
    def default(cls):
        return cls(minimum=0, maximum=2**32)


class OptionDesc(NamedTuple):
    long_option: str
    short_option: Optional[str] = None
    description: Optional[str] = None
    detail: Optional[str] = None
    argument_optional: bool = False
    file_exists: bool = False
    deprecated: bool = False
    aliases: Optional[str] = None
    conflicts: Sequence[str] = list()
    requires: Sequence[str] = list()
    argument_range: Optional[Range] = None
    argument_type: Optional[ArgumentType] = None
    argument_name: Optional[str] = None
    argument_default: Optional[Union[str, int]] = None
    multiple: bool = False
    occur_range: Optional[Range] = None
    enabled: bool = False
    disable_prefix: Optional[str] = None
    enable_prefix: Optional[str] = None

    @classmethod
    def from_json(cls, obj):
        return cls(long_option=obj['long-option'],
                   short_option=obj.get('short-option'),
                   description=obj.get('description'),
                   detail=obj.get('detail'),
                   argument_optional=obj.get('argument-optional', False),
                   file_exists=obj.get('file-exists', False),
                   deprecated=obj.get('deprecated', False),
                   aliases=obj.get('aliases'),
                   conflicts=obj.get('conflicts', list()),
                   requires=obj.get('requires', list()),
                   argument_range=SignedRange.from_json(
                       obj['argument-range']
                   ) if 'argument-range' in obj else None,
                   argument_type=ArgumentType.from_json(
                       obj['argument-type']
                   ) if 'argument-type' in obj else None,
                   argument_name=obj.get('argument-name'),
                   argument_default=obj.get('argument-default'),
                   multiple=obj.get('multiple'),
                   occur_range=UnsignedRange.from_json(
                       obj['occurrences']
                   ) if 'occurrences' in obj else None,
                   enabled=obj.get('enabled', False),
                   disable_prefix=obj.get('disable-prefix'),
                   enable_prefix=obj.get('enable-prefix'))


class SectionDesc(NamedTuple):
    ref: Optional[str] = None
    description: Optional[str] = None
    options: Sequence[OptionDesc] = list()

    @classmethod
    def from_json(cls, obj):
        return cls(ref=obj.get('ref'),
                   description=obj.get('description'),
                   options=[OptionDesc.from_json(option)
                            for option in obj['options']])

    @classmethod
    def default(cls):
        return DEFAULT_SECTION


class Desc(NamedTuple):
    tool: ToolDesc
    sections: Sequence[SectionDesc] = list()

    @classmethod
    def from_json(cls, obj):
        return cls(tool=ToolDesc.from_json(obj['tool']),
                   sections=[
                       SectionDesc.from_json(section)
                       for section in obj['sections']
                   ] + [
                       SectionDesc.default()
                   ])


DEFAULT_SECTION = SectionDesc(
    description='Version, usage and configuration options',
    options=[
        OptionDesc(
            long_option='version',
            short_option='v',
            argument_type=ArgumentType.KEYWORD,
            argument_optional=True,
            description='output version information and exit',
            detail=textwrap.fill(textwrap.dedent("""\
            Output version of program and exit.
            The default mode is `v', a simple version.
            The `c' mode will print copyright information and
            `n' will print the full copyright notice.\
        """), width=72, fix_sentence_endings=True)
        ),
        OptionDesc(
            long_option='help',
            short_option='h',
            description='display extended usage information and exit',
            detail='Display usage information and exit.'
        ),
        OptionDesc(
            long_option='more-help',
            short_option='!',
            description='extended usage information passed thru pager',
            detail='Pass the extended usage information through a pager.'
        )
    ]
)
