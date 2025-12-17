# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import Mapping, MutableMapping, MutableSequence, Sequence
from typing import TextIO, Union
import io
from cligen.types import ArgumentType, Desc, OptionDesc
from cligen.package import Info, version
import sys
import textwrap

INDENT = '  '


def get_aliases(options: Sequence[OptionDesc]) -> Mapping[str, Sequence[str]]:
    aliases: MutableMapping[str, MutableSequence[str]] = dict()
    for option in options:
        if option.aliases:
            val = aliases.get(option.aliases, list())
            val.append(option.long_option)
            aliases[option.aliases] = val
    return aliases


def get_chars(options: Sequence[OptionDesc]) -> Mapping[str, Union[str, int]]:
    chars: MutableMapping[str, Union[str, int]] = dict()
    chars_counter = 1
    short_opts: MutableMapping[str, str] = dict()
    for option in options:
        # If the short option is already taken, do not register twice
        if option.short_option and (option.short_option not in short_opts):
            chars[option.long_option] = option.short_option
            short_opts[option.short_option] = option.long_option
        else:
            if option.short_option:
                print((f'short option {option.short_option} for '
                       f'{option.long_option} is already '
                       f'taken by {short_opts[option.short_option]}'),
                      file=sys.stderr)
            chars[option.long_option] = chars_counter
            chars_counter += 1
        if option.disable_prefix:
            chars[
                f'{option.disable_prefix}{option.long_option}'
            ] = chars_counter
            chars_counter += 1
    return chars


# Reserved keywords in C, from 6.4.1 of N1570
KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if', 'inline',
    'int', 'long', 'register', 'restrict', 'return', 'short', 'signed',
    'sizeof', 'static', 'struct', 'switch', 'typedef', 'union', 'unsigned',
    'void', 'volatile', 'while', '_Alignas', '_Alignof', '_Atomic', '_Bool',
    '_Complex', '_Generic', '_Imaginary', '_Noreturn', '_Static_assert',
    '_Thread_local',
}


def escape_c_keyword(name: str) -> str:
    while name in KEYWORDS:
        name += '_'
    return name


def mangle(name: str) -> str:
    return ''.join([c if c in 'abcdefghijklmnopqrstuvwxyz0123456789_' else '_'
                    for c in name.lower()])


def format_long_opt(c: Union[str, int], long_opt: str, has_arg: str) -> str:
    if isinstance(c, str):
        return f"{INDENT}{{ \"{long_opt}\", {has_arg}, 0, '{c}' }},\n"
    else:
        return f'{INDENT}{{ "{long_opt}", {has_arg}, 0, CHAR_MAX + {c} }},\n'


def format_switch_case(c: Union[str, int], long_opt: str) -> str:
    if isinstance(c, str):
        return f"{INDENT*3}case '{c}':\n"
    else:
        return f'{INDENT*3}case CHAR_MAX + {c}: /* --{long_opt} */\n'


def usage(desc: Desc, info: Info) -> str:
    out = io.StringIO()
    out.write(f'{desc.tool.name} - {desc.tool.title}\n')
    out.write(
        f'Usage:  {desc.tool.name} '
        f'[ -<flag> [<val>] | --<name>[{{=| }}<val>] ]... '
        f'{desc.tool.argument if desc.tool.argument else ""}\n'
    )
    for section in desc.sections:
        out.write('\n')
        if section.description != '':
            out.write(f'{section.description}:\n\n')
        for option in section.options:
            if option.deprecated:
                continue
            if option.short_option:
                header = f'   -{option.short_option}, --{option.long_option}'
            else:
                header = f'       --{option.long_option}'
            if option.argument_type:
                if option.argument_type:
                    arg = option.argument_type.get_name()
                else:
                    arg = 'arg'
                if option.argument_optional:
                    header += f'[={arg}]'
                else:
                    header += f'={arg}'
            if len(header) < 30:
                header = header.ljust(30)
            elif option.argument_type:
                header += ' '
            else:
                header += '  '
            if option.aliases:
                out.write(f'{header}an alias for the '
                          f"'{option.aliases}' option\n")
            else:
                out.write(f'{header}{option.description}\n')
            if len(option.conflicts) == 1:
                out.write(
                    f"\t\t\t\t- prohibits the option '{option.conflicts[0]}'\n"
                )
            elif len(option.conflicts) > 1:
                conflict_opts_concatenated = '\n'.join([
                    f'\t\t\t\t{o}' for o in option.conflicts
                ])
                out.write(
                    '\t\t\t\t- prohibits these options:\n' +
                    conflict_opts_concatenated + '\n'
                )
            if len(option.requires) == 1:
                out.write(
                    f"\t\t\t\t- requires the option '{option.requires[0]}'\n"
                )
            elif len(option.requires) > 1:
                require_opts_concatenated = '\n'.join([
                    f'\t\t\t\t{o}' for o in option.requires
                ])
                out.write(
                    '\t\t\t\t- requires these options:\n' +
                    require_opts_concatenated + '\n'
                )
            if option.file_exists:
                out.write('\t\t\t\t- file must pre-exist\n')
            if option.enabled:
                out.write('\t\t\t\t- enabled by default\n')
                if option.disable_prefix:
                    out.write(
                        '\t\t\t\t- disabled as '
                        f"'--{option.disable_prefix}{option.long_option}'\n"
                    )
            if option.argument_range:
                out.write(
                    '\t\t\t\t- it must be in the range:\n'
                    f'\t\t\t\t  {option.argument_range.minimum} to '
                    f'{option.argument_range.maximum}\n'
                )
    out.write(textwrap.dedent('''
        Options are specified by doubled hyphens and their name or by a single
        hyphen and the flag character.
    '''))
    if desc.tool.argument:
        out.write(('Operands and options may be intermixed.  '
                   'They will be reordered.\n'))
    if desc.tool.detail:
        out.write(f'\n{desc.tool.detail}\n')
    if info.bug_email:
        out.write(f'\nPlease send bug reports to:  <{info.bug_email}>\n')
    return out.getvalue()


def generate_source(desc: Desc, info: Info, outfile: TextIO):
    long_opts = io.StringIO()
    short_opts = list()
    default_definitions = io.StringIO()
    default_statements = io.StringIO()
    switch_cases = io.StringIO()
    enable_statements = io.StringIO()
    constraint_statements = io.StringIO()
    has_list_arg = False
    has_number_arg = False
    has_default_arg = False

    options = [option for section in desc.sections
               for option in section.options]
    chars = get_chars(options)
    aliases = get_aliases(options)

    struct_name = f'{mangle(desc.tool.name)}_options'
    global_name = f'{mangle(desc.tool.name)}_options'

    switch_cases.write(f"{INDENT*3}case '\\0': /* Long option.  */\n")
    switch_cases.write(f'{INDENT*4}break;\n')

    for option in options:
        long_opt = option.long_option
        arg_type = option.argument_type
        lower_opt = mangle(long_opt)
        upper_opt = lower_opt.upper()
        lower_opt = escape_c_keyword(lower_opt)

        # aliases are handled differently
        if option.aliases:
            continue

        if arg_type:
            if option.argument_optional:
                has_arg = 'optional_argument'
            else:
                has_arg = 'required_argument'
        else:
            has_arg = 'no_argument'

        c = chars[long_opt]

        if isinstance(c, str):
            if arg_type:
                short_opts.append(c + ':')
            else:
                short_opts.append(c)

        long_opts.write(format_long_opt(c, long_opt, has_arg))
        switch_cases.write(format_switch_case(c, long_opt))

        for alias in aliases.get(long_opt, list()):
            c = chars[alias]
            long_opts.write(format_long_opt(c, alias, has_arg))
            switch_cases.write(format_switch_case(c, alias))

        switch_cases.write(f'{INDENT*4}opts->present.{lower_opt} = true;\n')

        if arg_type:
            if option.multiple:
                has_list_arg = True
                switch_cases.write((
                    f'{INDENT*4}append_to_list (&opts->list.{lower_opt}, '
                    f'"{long_opt}", optarg);\n'
                ))
            else:
                switch_cases.write(
                    f'{INDENT*4}opts->arg.{lower_opt} = optarg;\n'
                )
                if arg_type == ArgumentType.NUMBER:
                    has_number_arg = True
                    switch_cases.write((
                        f'{INDENT*4}opts->value.{lower_opt} = '
                        'parse_number(optarg);\n'
                    ))
                if option.argument_default:
                    has_default_arg = True
                    default_definitions.write(
                        f'static const char *{lower_opt}_default = '
                        f'"{option.argument_default}";\n'
                    )
                    default_statements.write(
                        f'{INDENT}opts->arg.{lower_opt} = '
                        f'{lower_opt}_default;\n'
                    )
                    if arg_type == ArgumentType.NUMBER:
                        assert isinstance(option.argument_default, int)
                        default_statements.write((
                            f'{INDENT}opts->value.{lower_opt} = '
                            f'{option.argument_default};\n'
                        ))

        switch_cases.write(
            f'{INDENT*4}opts->enabled.{lower_opt} = true;\n'
        )

        switch_cases.write(f'{INDENT*4}break;\n')

        if option.enabled:
            enable_statements.write(
                f'{INDENT}opts->enabled.{lower_opt} = true;\n'
            )
        if option.disable_prefix:
            disable_opt = f'{option.disable_prefix}{long_opt}'
            c = chars[disable_opt]
            long_opts.write(format_long_opt(c, disable_opt, has_arg))
            switch_cases.write(format_switch_case(c, disable_opt))
            switch_cases.write(
                f'{INDENT*4}opts->present.{lower_opt} = true;\n'
            )
            switch_cases.write(
                f'{INDENT*4}opts->enabled.{lower_opt} = false;\n'
            )
            switch_cases.write(f'{INDENT*4}break;\n')

        for conflict_opt in option.conflicts:
            constraint_statements.write(f'''\
{INDENT}if (HAVE_OPT({upper_opt}) && HAVE_OPT({mangle(conflict_opt).upper()}))
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "the '%s' and '%s' options conflict\\n",
{INDENT*3}         "{long_opt}", "{mangle(conflict_opt)}");
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
''')
        for require_opt in option.requires:
            constraint_statements.write(f'''\
{INDENT}if (HAVE_OPT({upper_opt}) && !HAVE_OPT({mangle(require_opt).upper()}))
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "%s option requires the %s options\\n",
{INDENT*3}         "{long_opt}", "{mangle(require_opt)}");
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
''')
        arg_range = option.argument_range
        if arg_range:
            constraint_statements.write(f'''\
{INDENT}if (HAVE_OPT({upper_opt}) && \
OPT_VALUE_{upper_opt} < {arg_range.minimum})
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "%s option value %d is out of range\\n",
{INDENT*3}         "{long_opt}", opts->value.{lower_opt});
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
''')
            constraint_statements.write(f'''\
{INDENT}if (HAVE_OPT({upper_opt}) && \
OPT_VALUE_{upper_opt} > {arg_range.maximum})
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "%s option value %d is out of range\\n",
{INDENT*3}         "{long_opt}", opts->value.{lower_opt});
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
''')

    long_opts.write(f'{INDENT}{{ 0, 0, 0, 0 }}\n')

    switch_cases.write(f'{INDENT*3}default:\n')
    switch_cases.write(f'{INDENT*4}usage (stderr, EXIT_FAILURE);\n')
    switch_cases.write(f'{INDENT*4}break;\n')

    argument = desc.tool.argument
    if argument:
        if argument.startswith('[') and argument.endswith(']'):
            argument = argument[1:-1]
            argument_statement = ''
        else:
            argument_statement = f'''\
{INDENT}if (optind == argc)
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "Command line arguments required\\n");
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
'''
    else:
        argument_statement = f'''\
{INDENT}if (optind < argc)
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "Command line arguments are not allowed\\n");
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}
'''

    short_opts_concatenated = ''.join(sorted(short_opts))
    usage_stringified = '\n'.join([
        f'{INDENT*2}"{line}\\n"' for line in usage(desc, info).split('\n')
    ])
    version_v = version(desc, info, 'v')
    version_c = version(desc, info, 'c')
    version_n = version(desc, info, 'n')
    version_v_stringified = '\n'.join([
        f'{INDENT*6}"{line}\\n"' for line in version_v.split('\n')
    ])
    version_c_stringified = '\n'.join([
        f'{INDENT*6}"{line}\\n"' for line in version_c.split('\n')
    ])
    version_n_stringified = '\n'.join([
        f'{INDENT*6}"{line}\\n"' for line in version_n.split('\n')
    ])

    if outfile.name.endswith('.c'):
        outfile_base = outfile.name[:-len('.c')]
    else:
        outfile_base = outfile.name

    outfile.write(f'''\
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "{outfile_base}.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* !_WIN32 */
#include <limits.h>

struct {struct_name} {global_name};

''')

    if has_list_arg:
        outfile.write(f'''\
/* Copied from xsize.h in Gnulib */

/* Sum of two sizes, with overflow check.  */
static inline size_t
xsum (size_t size1, size_t size2)
{{
{INDENT}size_t sum = size1 + size2;
{INDENT}return (sum >= size1 ? sum : SIZE_MAX);
}}

/* Check for overflow.  */
#define size_overflow_p(SIZE) \
{INDENT}((SIZE) == SIZE_MAX)

static void
append_to_list (struct {mangle(desc.tool.name)}_list *list,
                const char *name, const char *arg)
{{
{INDENT}const char **tmp;
{INDENT}size_t new_count = xsum (list->count, 1);

{INDENT}if (size_overflow_p (new_count))
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "too many arguments for %s\\n",
{INDENT*3}         name);
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}

{INDENT}tmp = reallocarray (list->args, new_count, sizeof (char *));
{INDENT}if (!tmp)
{INDENT*2}{{
{INDENT*3}fprintf (stderr, "unable to allocate memory for %s\\n",
{INDENT*3}         name);
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}

{INDENT}list->args = tmp;
{INDENT}list->args[list->count] = optarg;
{INDENT}list->count = new_count;
}}

''')

    if has_number_arg:
        outfile.write(f'''\
static long
parse_number (const char *arg)
{{
{INDENT}char *endptr = NULL;
{INDENT}errno = 0;
{INDENT}long result;

{INDENT}if (strncmp (arg, "0x", 2) == 0)
{INDENT*2}result = strtol (arg + 2, &endptr, 16);
{INDENT}else if (strncmp (arg, "0", 1) == 0
{INDENT}         && strspn (arg, "012345678") == strlen (optarg))
{INDENT*2}result = strtol (arg + 1, &endptr, 8);
{INDENT}else
{INDENT*2}result = strtol (arg, &endptr, 10);

{INDENT}if (errno != 0 || (endptr && *endptr != '\\0'))
{INDENT*2}{{
{INDENT*3}char buf[80];
{INDENT*3}snprintf (buf, sizeof(buf),
{INDENT*3}          "'%s' is not a recognizable number",
{INDENT*3}          arg);
{INDENT*3}perror (buf);
{INDENT*3}exit (EXIT_FAILURE);
{INDENT*2}}}

{INDENT}return result;
}}

''')

    outfile.write(f'''\
/* Long options.  */
static const struct option long_options[] =
{{
{long_opts.getvalue()}
}};

''')
    if has_default_arg:
        outfile.write(f'''\
/* Default options.  */
{default_definitions.getvalue().rstrip()}

''')
    outfile.write(f'''\
int
process_options (int argc, char **argv)
{{
{INDENT}struct {struct_name} *opts = &{global_name};
{INDENT}int opt;

''')
    if has_default_arg:
        outfile.write(f'''\
{default_statements.getvalue().rstrip()}
''')
    outfile.write(f'''\
{enable_statements.getvalue().rstrip()}
{INDENT}while ((opt = getopt_long (argc, argv, "{short_opts_concatenated}",
{INDENT}                           long_options, NULL)) != EOF)
{INDENT*2}switch (opt)
{INDENT*3}{{
{switch_cases.getvalue().rstrip()}
{INDENT*3}}}

{constraint_statements.getvalue().rstrip()}
{argument_statement}

{INDENT}if (HAVE_OPT(HELP))
{INDENT*2}{{
{INDENT*3}USAGE(0);
{INDENT*2}}}

{INDENT}if (HAVE_OPT(MORE_HELP))
#ifdef _WIN32
{INDENT*2}{{
{INDENT*3}USAGE(0);
{INDENT*2}}}
#else /* _WIN32 */
{INDENT*2}{{
{INDENT*3}pid_t pid;
{INDENT*3}int pfds[2];

{INDENT*3}if (pipe (pfds) < 0)
{INDENT*4}{{
{INDENT*5}perror ("pipe");
{INDENT*5}exit (EXIT_FAILURE);
{INDENT*4}}}

{INDENT*3}pid = fork ();
{INDENT*3}if (pid < 0)
{INDENT*4}{{
{INDENT*5}perror ("fork");
{INDENT*5}exit (EXIT_FAILURE);
{INDENT*4}}}

{INDENT*3}if (pid == 0)
{INDENT*4}{{
{INDENT*5}close (pfds[0]);
{INDENT*5}dup2 (pfds[1], STDOUT_FILENO);
{INDENT*5}close (pfds[1]);

{INDENT*5}usage (stdout, 0);
{INDENT*4}}}
{INDENT*3}else
{INDENT*4}{{
{INDENT*5}const char *args[2];
{INDENT*5}const char *envvar;

{INDENT*5}close (pfds[1]);
{INDENT*5}dup2 (pfds[0], STDIN_FILENO);
{INDENT*5}close (pfds[0]);

{INDENT*5}envvar = secure_getenv ("PAGER");
{INDENT*5}if (!envvar || *envvar == '\\0')
{INDENT*6}args[0] = "more";
{INDENT*5}else
{INDENT*6}args[0] = envvar;

{INDENT*5}args[1] = NULL;

{INDENT*5}execvp (args[0], (char * const *)args);

{INDENT*5}exit (EXIT_FAILURE);
{INDENT*4}}}
{INDENT*2}}}
#endif /* !_WIN32 */

{INDENT}if (HAVE_OPT(VERSION))
{INDENT*2}{{
{INDENT*3}if (!OPT_ARG_VERSION || !strcmp (OPT_ARG_VERSION, "c"))
{INDENT*4}{{
{INDENT*5}const char str[] =
{version_c_stringified};
{INDENT*5}fprintf (stdout, "%s", str);
{INDENT*5}exit(0);
{INDENT*4}}}
{INDENT*3}else if (!strcmp (OPT_ARG_VERSION, "v"))
{INDENT*4}{{
{INDENT*5}const char str[] =
{version_v_stringified};
{INDENT*5}fprintf (stdout, "%s", str);
{INDENT*5}exit(0);
{INDENT*4}}}
{INDENT*3}else if (!strcmp (OPT_ARG_VERSION, "n"))
{INDENT*4}{{
{INDENT*5}const char str[] =
{version_n_stringified};
{INDENT*5}fprintf (stdout, "%s", str);
{INDENT*5}exit(0);
{INDENT*4}}}
{INDENT*3}else
{INDENT*4}{{
{INDENT*5}fprintf (stderr,
{INDENT*5}         "version option argument 'a' invalid.  Use:\\n"
{INDENT*5}         "	'v' - version only\\n"
{INDENT*5}         "	'c' - version and copyright\\n"
{INDENT*5}         "	'n' - version and full copyright notice\\n");
{INDENT*5}exit (EXIT_FAILURE);
{INDENT*4}}}
{INDENT*2}}}

{INDENT}return optind;
}}

void
usage (FILE *out, int status)
{{
{INDENT}const char str[] =
{usage_stringified};
{INDENT}fprintf (out, "%s", str);
{INDENT}exit (status);
}}
''')


def generate_header(desc: Desc, info: Info, outfile: TextIO):
    struct_members_present = io.StringIO()
    struct_members_arg = io.StringIO()
    struct_members_value = io.StringIO()
    struct_members_enabled = io.StringIO()
    struct_members_list = io.StringIO()
    have_opts = io.StringIO()
    opt_args = io.StringIO()
    opt_values = io.StringIO()
    enabled_opts = io.StringIO()
    opts_count = io.StringIO()
    opts_array = io.StringIO()
    has_list_arg = False

    struct_name = f'{mangle(desc.tool.name)}_options'
    global_name = f'{mangle(desc.tool.name)}_options'
    list_struct_name = f'{mangle(desc.tool.name)}_list'

    options = [option for section in desc.sections
               for option in section.options]

    for option in options:
        long_opt = option.long_option
        arg_type = option.argument_type
        lower_opt = mangle(long_opt)
        upper_opt = lower_opt.upper()
        lower_opt = escape_c_keyword(lower_opt)

        # aliases are handled differently
        if option.aliases:
            continue

        struct_members_present.write(f'{INDENT*2}bool {lower_opt};\n')

        if arg_type:
            if option.multiple:
                has_list_arg = True
                struct_members_list.write(
                    f'{INDENT*2}struct {list_struct_name} {lower_opt};\n'
                )
                opts_count.write((
                    f'#define OPTS_COUNT_{upper_opt} '
                    f'{global_name}.list.{lower_opt}.count\n'
                ))
                opts_array.write((
                    f'#define OPTS_ARRAY_{upper_opt} '
                    f'{global_name}.list.{lower_opt}.args\n'
                ))
            else:
                struct_members_arg.write(
                    f'{INDENT*2}const char *{lower_opt};\n'
                )
                if arg_type == ArgumentType.NUMBER:
                    struct_members_value.write(f'{INDENT*2}int {lower_opt};\n')
                    opt_values.write((
                        f'#define OPT_VALUE_{upper_opt} '
                        f'{global_name}.value.{lower_opt}\n'
                    ))

        struct_members_enabled.write(f'{INDENT*2}bool {lower_opt};\n')
        enabled_opts.write((
            f'#define ENABLED_OPT_{upper_opt} '
            f'{global_name}.enabled.{lower_opt}\n'
        ))

        have_opts.write((
            f'#define HAVE_OPT_{upper_opt} '
            f'{global_name}.present.{lower_opt}\n'
        ))
        opt_args.write((
            f'#define OPT_ARG_{upper_opt} '
            f'{global_name}.arg.{lower_opt}\n'
        ))

    header_guard = f'{mangle(outfile.name).upper()}_'

    outfile.write(f'''\
#include <stdbool.h>
#include <stdio.h>

#ifndef {header_guard}
#define {header_guard} 1

struct {list_struct_name}
{{
{INDENT}const char **args;
{INDENT}unsigned int count;
}};

struct {struct_name}
{{
{INDENT}/* Options present in the command line */
{INDENT}struct
{INDENT}{{
{struct_members_present.getvalue().rstrip()}
{INDENT}}} present;

{INDENT}/* Option arguments in raw string form */
{INDENT}struct
{INDENT}{{
{struct_members_arg.getvalue().rstrip()}
{INDENT}}} arg;

{INDENT}/* Option arguments parsed as integer */
{INDENT}struct
{INDENT}{{
{struct_members_value.getvalue().rstrip()}
{INDENT}}} value;
''')
    if has_list_arg:
        outfile.write(f'''
{INDENT}/* Option arguments parsed as list */
{INDENT}struct
{INDENT}{{
{struct_members_list.getvalue().rstrip()}
{INDENT}}} list;
''')
    outfile.write(f'''
{INDENT}/* Option enablement status */
{INDENT}struct
{INDENT}{{
{struct_members_enabled.getvalue().rstrip()}
{INDENT}}} enabled;
}};

#define HAVE_OPT(name) HAVE_OPT_ ## name
#define OPT_ARG(name) OPT_ARG_ ## name
#define ENABLED_OPT(name) ENABLED_OPT_ ## name
#define OPTS_COUNT(name) OPTS_COUNT_ ## name
#define OPTS_ARRAY(name) OPTS_ARRAY_ ## name
#define USAGE(status) usage (stdout, (status))

{have_opts.getvalue()}
{opt_args.getvalue()}
{opt_values.getvalue()}
{enabled_opts.getvalue()}
{opts_count.getvalue()}
{opts_array.getvalue()}

extern struct {struct_name} {global_name};
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* {header_guard} */
''')
