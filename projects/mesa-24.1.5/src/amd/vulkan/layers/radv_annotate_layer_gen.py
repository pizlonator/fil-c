COPYRIGHT=u"""
/* Copyright © 2021 Intel Corporation
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */
"""

import argparse
import os
import sys
import xml.etree.ElementTree as et

import mako
from mako.template import Template

sys.path.append(os.path.join(sys.path[0], '../../../vulkan/util/'))

from vk_entrypoints import get_entrypoints_from_xml

EXCLUDED_COMMANDS = [
    'CmdBeginRenderPass',
    'CmdEndRenderPass',
    'CmdDispatch',
]

TEMPLATE = Template(COPYRIGHT + """
/* This file generated from ${filename}, don't edit directly. */

#include "radv_cmd_buffer.h"
#include "radv_entrypoints.h"

#define ANNOTATE(command, ...) \
   struct radv_cmd_buffer *cmd_buffer = radv_cmd_buffer_from_handle(commandBuffer); \
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer); \
   radv_cmd_buffer_annotate(cmd_buffer, #command); \
   device->layer_dispatch.annotate.command(__VA_ARGS__)

% for c in commands:
% if c.guard is not None:
#ifdef ${c.guard}
% endif
VKAPI_ATTR ${c.return_type} VKAPI_CALL
annotate_${c.name}(${c.decl_params()})
{
   ANNOTATE(${c.name}, ${c.call_params()});
}

% if c.guard is not None:
#endif // ${c.guard}
% endif
% endfor
""")

# str.removesuffix requires python 3.9+ so implement our own to not break build
# on older versions
def removesuffix(s, suffix):
    l = len(suffix)
    if l == 0:
        return s
    idx = s.find(suffix)
    if idx == len(s) - l:
        return s[:-l]
    return s


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-c", required=True, help="Output C file.")
    parser.add_argument("--beta", required=True, help="Enable beta extensions.")
    parser.add_argument("--xml",
                        help="Vulkan API XML file.",
                        required=True, action="append", dest="xml_files")
    args = parser.parse_args()

    commands = []
    commands_names = []
    for e in get_entrypoints_from_xml(args.xml_files, args.beta):
        if not e.name.startswith('Cmd') or e.alias or e.return_type != "void":
            continue

        stripped_name = removesuffix(removesuffix(removesuffix(e.name, 'EXT'), 'KHR'), '2')
        if stripped_name in commands_names or stripped_name in EXCLUDED_COMMANDS:
            continue

        commands.append(e)
        commands_names.append(stripped_name)

    environment = {
        "filename": os.path.basename(__file__),
        "commands": commands,
    }

    try:
        with open(args.out_c, "w", encoding='utf-8') as f:
            f.write(TEMPLATE.render(**environment))
    except Exception:
        # In the event there"s an error, this uses some helpers from mako
        # to print a useful stack trace and prints it, then exits with
        # status 1, if python is run with debug; otherwise it just raises
        # the exception
        print(mako.exceptions.text_error_template().render(), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
