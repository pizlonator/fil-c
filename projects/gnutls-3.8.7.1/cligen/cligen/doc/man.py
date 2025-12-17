# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import Mapping, Optional, TextIO, Sequence
import io
import os
import re
import time
from cligen.types import ArgumentType, Desc, ToolDesc, OptionDesc
from cligen.package import Info, license


def generate_options(tool: ToolDesc,
                     options: Sequence[OptionDesc]) -> str:
    docs = io.StringIO()
    for option in options:
        long_opt = option.long_option
        long_opt_escaped = long_opt.replace('-', '\\-')
        short_opt = option.short_option
        desc = option.description
        disable_prefix = option.disable_prefix
        if disable_prefix:
            disable_opt: Optional[str] = f'{disable_prefix}{long_opt}'
        else:
            disable_opt = None
        if option.aliases:
            docs.write(f'''\
.TP
.NOP \\f\\*[B-Font]\\-\\-{long_opt_escaped}\\f[]
This is an alias for the \\fI--{option.aliases}\\fR option.
''')
            if option.deprecated:
                docs.write('''\
.sp
.B
NOTE: THIS OPTION IS DEPRECATED
''')
            continue

        if option.argument_type:
            if option.argument_name:
                arg_name = option.argument_name.lower()
            else:
                arg_name = option.argument_type.get_name()
            arg = f'\\f\\*[I-Font]{arg_name}\\f[]'
            long_arg = f'={arg}'
            short_arg = f' {arg}'
        else:
            long_arg = ''
            short_arg = ''
        formatted_options = list()
        if short_opt:
            formatted_options.append(
                f'\\f\\*[B-Font]\\-{short_opt}\\f[]{short_arg}'
            )
        formatted_options.append(
            f'\\f\\*[B-Font]\\-\\-{long_opt_escaped}\\f[]{long_arg}'
        )
        if disable_opt:
            disable_opt_escaped = disable_opt.replace('-', '\\-')
            formatted_options.append(
                f'\\f\\*[B-Font]\\-\\-{disable_opt_escaped}\\f[]'
            )
        docs.write(f'''\
.TP
.NOP {', '.join(formatted_options)}
''')
        if desc and desc[0].isupper():
            docs.write(f'{desc}.\n')
        if option.multiple:
            docs.write(
                'This option may appear an unlimited number of times.\n'
            )
        if option.argument_type == ArgumentType.NUMBER:
            docs.write(
                'This option takes an integer number as its argument.\n'
            )
            if option.argument_range:
                docs.write(f'''\
The value of
\\f\\*[I-Font]{arg_name}\\f[]
is constrained to being:
.in +4
.nf
.na
in the range {option.argument_range.minimum} through \
{option.argument_range.maximum}
.fi
.in -4
''')
            if option.argument_default:
                docs.write(f'''\
The default
\\f\\*[I-Font]number\\f[]
for this option is:
.ti +4
 {option.argument_default}
.sp
''')
        if len(option.conflicts) > 0:
            docs.write(f'''\
This option must not appear in combination with any of the following options:
{', '.join(option.conflicts)}.
''')
        if len(option.requires) > 0:
            docs.write(f'''\
This option must appear in combination with the following options:
{', '.join(option.requires)}.
''')
        if disable_opt:
            disable_opt_escaped = disable_opt.replace('-', '\\-')
            docs.write((
                f'The \\fI{disable_opt_escaped}\\fP form '
                'will disable the option.\n'
            ))
        if option.enabled:
            docs.write('This option is enabled by default.\n')
        if desc and desc[0].isupper():
            docs.write('.sp\n')
        if option.detail:
            docs.write(f'{text_to_man(option.detail)}\n')
        if option.deprecated:
            docs.write('''\
.sp
.B
NOTE: THIS OPTION IS DEPRECATED
''')
    return docs.getvalue()


def text_to_man(s: str) -> str:
    s = re.sub(r'-', r'\\-', s)
    s = re.sub(r'(?m)^$', r'.sp', s)
    s = re.sub(r"``(.*)''", r'\\(lq\1\\(rq', s)
    return s


def texi_to_man(s: str) -> str:
    s = text_to_man(s)
    s = re.sub(r'@([{}@])', r'\1', s)
    s = re.sub(r'@code\{(.*?)\}', r'\\fB\1\\fP', s)
    s = re.sub(r'@file\{(.*?)\}', r'\\fI\1\\fP', s)
    s = re.sub(r'@subheading (.*)', r'''.br
\\fB\1\\fP
.br''', s)
    s = re.sub(r'@example', r'''.br
.in +4
.nf''', s)
    s = re.sub(r'@end example', r'''.in -4
.fi''', s)
    return s


def include(name: str, includes: Mapping[str, TextIO]) -> str:
    docs = io.StringIO()
    f = includes.get(name)
    if f:
        docs.write(texi_to_man(f.read().strip()))
    return docs.getvalue()


def generate(desc: Desc, info: Info,
             includes: Mapping[str, TextIO],
             outfile: TextIO):
    description = includes.get('description')
    if description:
        detail = texi_to_man(description.read())
    elif desc.tool.detail:
        detail = desc.tool.detail
    else:
        detail = ''

    section_docs = io.StringIO()
    for section in desc.sections:
        if section.ref:
            option_docs = generate_options(desc.tool, section.options)
            section_docs.write(f'''\
.SS "{section.description}"
{option_docs}\
''')
        else:
            section_docs.write(generate_options(desc.tool, section.options))

    formatted_date = time.strftime(
        '%d %b %Y',
        time.gmtime(int(os.environ.get('SOURCE_DATE_EPOCH', time.time())))
    )
    detail_concatenated = '\n.sp\n'.join(detail.strip().split('\n\n'))
    outfile.write(f'''\
.de1 NOP
.  it 1 an-trap
.  if \\\\n[.$] \\,\\\\$*\\/
..
.ie t \\
.ds B-Font [CB]
.ds I-Font [CI]
.ds R-Font [CR]
.el \\
.ds B-Font B
.ds I-Font I
.ds R-Font R
.TH {desc.tool.name} 1 "{formatted_date}" "{info.version}" "User Commands"
.SH NAME
\\f\\*[B-Font]{desc.tool.name}\\fP
\\- {desc.tool.title}
.SH SYNOPSIS
\\f\\*[B-Font]{desc.tool.name}\\fP
.\\" Mixture of short (flag) options and long options
[\\f\\*[B-Font]\\-flags\\f[]]
[\\f\\*[B-Font]\\-flag\\f[] [\\f\\*[I-Font]value\\f[]]]
[\\f\\*[B-Font]\\-\\-option-name\\f[][[=| ]\\f\\*[I-Font]value\\f[]]]
''')
    if desc.tool.argument:
        outfile.write(f'''\
{desc.tool.argument}
.sp \\n(Ppu
.ne 2

Operands and options may be intermixed.  They will be reordered.
.sp \\n(Ppu
.ne 2
''')
    else:
        outfile.write('''\
.sp \\n(Ppu
.ne 2

All arguments must be options.
.sp \\n(Ppu
.ne 2
''')
    outfile.write(f'''\
.SH "DESCRIPTION"
{detail_concatenated}
.sp
.SH "OPTIONS"
{section_docs.getvalue()}
''')
    if 'files' in includes:
        outfile.write(f'''\
.SH FILES
{include('files', includes)}
''')
    if 'examples' in includes:
        outfile.write(f'''\
.sp
.SH EXAMPLES
{include('examples', includes)}
''')
    outfile.write('''\
.SH "EXIT STATUS"
One of the following exit values will be returned:
.TP
.NOP 0 " (EXIT_SUCCESS)"
Successful program execution.
.TP
.NOP 1 " (EXIT_FAILURE)"
The operation failed or the command syntax was not valid.
.PP
''')
    if 'see-also' in includes:
        outfile.write(f'''\
.SH "SEE ALSO"
{include('see-also', includes)}
''')
    outfile.write(f'''\
.SH "AUTHORS"
{', '.join(info.authors)}
.SH "COPYRIGHT"
Copyright (C) {info.copyright_year} {info.copyright_holder}
{license(desc, info, 'brief')}.
''')
    if info.bug_email:
        outfile.write(f'''\
.SH "BUGS"
Please send bug reports to: {info.bug_email}
''')
