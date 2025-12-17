# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import Mapping, Sequence, TextIO
import io
import cligen.code
from cligen.types import Desc, ToolDesc, OptionDesc
from cligen.package import Info


HEADINGS = ['@heading', '@subheading', '@subsubheading']


def get_heading(level: int) -> str:
    return HEADINGS[min(level, len(HEADINGS)-1)]


SECTIONS = ['@section', '@subsection', '@subsubsection']


def get_section(level: int) -> str:
    return SECTIONS[min(level, len(SECTIONS)-1)]


def shift_headings(s: str, level: int) -> str:
    for (i, h) in reversed(list(enumerate(HEADINGS))):
        r = get_heading(level+i)
        s = s.replace(h, r)
    return s


def generate_options(tool: ToolDesc,
                     options: Sequence[OptionDesc],
                     level: int) -> str:
    docs = io.StringIO()
    for option in options:
        if option.aliases:
            docs.write(f'''\
{get_heading(level+1)} {option.long_option} option.
@anchor{{{tool.name} {option.long_option}}}

This is an alias for the @code{{{option.aliases}}} option,
@pxref{{{tool.name} {option.aliases}, \
the {option.aliases} option documentation}}.

''')
            continue

        if not option.detail or not option.description:
            continue
        if option.short_option:
            docs.write(
                f'{get_heading(level+1)} {option.long_option} option '
                f'(-{option.short_option}).\n'
            )
        else:
            docs.write(
                f'{get_heading(level+1)} {option.long_option} option.\n'
            )
        docs.write(f'''\
@anchor{{{tool.name} {option.long_option}}}

This is the ``{option.description.lower()}'' option.
''')
        if option.argument_type:
            if option.argument_name:
                docs.write((
                    f'This option takes a {option.argument_type} argument '
                    f'@file{{{option.argument_name}}}.\n'
                ))
            else:
                docs.write(f'This option takes '
                           f'a {option.argument_type} argument.\n')

        if len(option.conflicts) > 0 or len(option.requires) > 0 or \
           option.enabled:
            docs.write('''
@noindent
This option has some usage constraints.  It:
@itemize @bullet
''')
            if len(option.conflicts) > 0:
                docs.write(f'''\
@item
must not appear in combination with any of the following options:
{', '.join(option.conflicts)}.
''')
            if len(option.requires) > 0:
                docs.write(f'''\
@item
must appear in combination with the following options:
{', '.join(option.requires)}.
''')
            if option.disable_prefix:
                docs.write(f'''\
@item
can be disabled with --{option.disable_prefix}{option.long_option}.
''')
            if option.enabled:
                docs.write('''\
@item
It is enabled by default.
''')
            docs.write('@end itemize\n\n')

        docs.write(f'''\
{option.detail}
''')
        if option.deprecated:
            docs.write('\n@strong{NOTE}@strong{: THIS OPTION IS DEPRECATED}\n')

    return docs.getvalue()


LABELS = {
    'see-also': 'See Also',
    'examples': 'Examples',
    'files': 'Files'
}


def include(tool: ToolDesc,
            name: str,
            includes: Mapping[str, TextIO],
            level: int) -> str:
    docs = io.StringIO()
    f = includes.get(name)
    if f:
        docs.write(f'''\
@anchor{{{tool.name} {LABELS[name]}}}
{get_heading(level+2)} {tool.name} {LABELS[name]}
{shift_headings(f.read(), level)}\
''')
    return docs.getvalue()


def escape_texi(s: str) -> str:
    for c in ['@', '{', '}']:
        s = s.replace(c, f'@{c}')
    return s


def generate(desc: Desc, info: Info,
             includes: Mapping[str, TextIO],
             outfile: TextIO,
             level: int = 0,
             section_node: bool = True):
    description = includes.get('description')
    if description:
        detail = description.read()
    elif desc.tool.detail:
        detail = desc.tool.detail
    else:
        detail = ''

    section_docs = io.StringIO()
    for section in desc.sections:
        if section.ref:
            option_docs = generate_options(desc.tool,
                                           section.options,
                                           level+1)
            assert section.description
            section_docs.write(f'''\
@anchor{{{desc.tool.name} {section.ref}}}
{get_heading(level+1)} {section.ref} options
{section.description.strip('.')}.
{option_docs}\
''')
        else:
            section_docs.write(generate_options(desc.tool,
                                                section.options,
                                                level))

    heading = get_section(level) if section_node else get_heading(level)
    outfile.write(f'''\
@node {desc.tool.name} Invocation
{heading} Invoking {desc.tool.name}
@pindex {desc.tool.name}

{detail}

@anchor{{{desc.tool.name} usage}}
{get_heading(level+1)} {desc.tool.name} help/usage (@option{{-?}})
@cindex {desc.tool.name} help

The text printed is the same whether selected with the @code{{help}} option
(@option{{--help}}) or the @code{{more-help}} option \
(@option{{--more-help}}).  @code{{more-help}} will print
the usage text by passing it through a pager program.
@code{{more-help}} is disabled on platforms without a working
@code{{fork(2)}} function.  The @code{{PAGER}} environment variable is
used to select the program, defaulting to @file{{more}}.  Both will exit
with a status code of 0.

@exampleindent 0
@example
{escape_texi(cligen.code.usage(desc, info))}
@end example
@exampleindent 4

{section_docs.getvalue()}\
@anchor{{{desc.tool.name} exit status}}
{get_heading(level+1)} {desc.tool.name} exit status

One of the following exit values will be returned:
@table @samp
@item 0 (EXIT_SUCCESS)
Successful program execution.
@item 1 (EXIT_FAILURE)
The operation failed or the command syntax was not valid.
@end table
''')
    if 'see-also' in includes:
        outfile.write(include(desc.tool, 'see-also', includes, level))
    if 'examples' in includes:
        outfile.write(include(desc.tool, 'examples', includes, level))
    if 'files' in includes:
        outfile.write(include(desc.tool, 'files', includes, level))
