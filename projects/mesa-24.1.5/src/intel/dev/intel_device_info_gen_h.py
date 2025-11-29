#!/usr/bin/env python3
COPYRIGHT = """\
/*
 * Copyright 2024 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
"""

import os
import sys
from textwrap import indent

from mako.template import Template
from mako import exceptions

from intel_device_info import TYPES_BY_NAME, Enum

template = COPYRIGHT + """

/* DO NOT EDIT - This file generated automatically by intel_device_info_gen_h.py script */

#ifndef INTEL_DEVICE_INFO_GEN_H
#define INTEL_DEVICE_INFO_GEN_H

#include <stdbool.h>
#include <stdint.h>

#include "util/macros.h"
#include "compiler/shader_enums.h"
#include "intel/dev/intel_wa.h"

#ifdef __cplusplus
extern "C" {
#endif
<%! import intel_device_info %>
% for decl in intel_device_info.TYPES:
% if isinstance(decl, intel_device_info.Define):
${format_define(decl)}
% elif isinstance(decl, intel_device_info.Enum) and not decl.external:

enum ${decl.name} {
% for value in decl.values:
${format_enum_value(value)}
% endfor
};
% elif isinstance(decl, intel_device_info.Struct):

struct ${decl.name}
{
% for member in decl.members:
${format_struct_member(member)}
% endfor
% if decl.name == "intel_device_info":
   BITSET_DECLARE(workarounds, INTEL_WA_NUM);
% endif
};
% endif
% endfor

#ifdef __cplusplus
}
#endif

#endif /* INTEL_DEVICE_INFO_GEN_H */
"""

def format_enum_value(v):
    """
    Routine to format the individual lines within an enum declaration.
    This is inconvenient to implement with mako.  Templates are an
    inconvenient tool for conditionally formatting:
      - inline comments
      - "grouped" values as required by intel_platform
      - specific values
    """
    comment = ""
    if v.comment:
        comment = f" /* {v.comment} */"
    value = ""
    if v.value is not None:
        value = f" = {v.value}"
    decl = f"{v.name}{value},{comment}"
    if v.group_begin:
        decl = f"{decl}\nINTEL_PLATFORM_{v.group_begin}_START = {v.name},"
    if v.group_end:
        decl = f"{decl}\nINTEL_PLATFORM_{v.group_end}_END = {v.name},"
    return indent(decl, "   ")

def format_define(v):
    """
    Routine to format the printing of a macro declaration.  Conditional
    inline comments are difficult to format in mako.
    """
    comment = ""
    if v.comment:
        comment = f" /* {v.comment} */"
    return f"#define {v.name} ({v.value}){comment}"

def format_struct_member(m):
    """
    Routine to format the printing of a struct member.  Mako templates are not
    helpful in formatting the following aspects of intel_device_info structs:
     - multiline vs single line comments
     - optional array lengths
     - enum / struct member type declarations
    """
    comment = ""
    if m.comment:
        if "\n" in m.comment:
            comment_lines = [ f" * {line}".rstrip() for line in m.comment.split('\n')]
            comment_lines.insert(0, "\n/**")
            comment_lines.append(" */\n")
            comment = '\n'.join(comment_lines)
        else:
            comment = f"\n/* {m.comment} */\n"
    array = ""
    if m.array:
        array = f"[{m.array}]"
    member_type = m.member_type
    if member_type in TYPES_BY_NAME:
        if isinstance(TYPES_BY_NAME[member_type], Enum):
            member_type = f"enum {member_type}"
        else:
            member_type = f"struct {member_type}"
    return indent(f"{comment}{member_type} {m.name}{array};", "   ")

def main():
    """print intel_device_info_gen.h at the specified path"""
    if len(sys.argv) > 1:
        outf = open(sys.argv[1], 'w', encoding='utf-8')
    else:
        outf = sys.stdout

    try:
        outf.write(Template(template).render(format_enum_value=format_enum_value,
                                             format_struct_member=format_struct_member,
                                             format_define=format_define))
    except:
        # provide some debug information to the user
        print(exceptions.text_error_template().render(format_enum_value=format_enum_value,
                                                      format_struct_member=format_struct_member,
                                                      format_define=format_define))
        sys.exit(1)
    outf.close()

if __name__ == "__main__":
    main()
