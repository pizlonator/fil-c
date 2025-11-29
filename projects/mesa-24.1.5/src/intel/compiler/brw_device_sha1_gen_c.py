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

import argparse
import os
import sys

from mako.template import Template
from mako import exceptions

sys.path.append(f"{os.path.dirname(sys.argv[0])}/../dev")
import intel_device_info

template = COPYRIGHT + """

/* DO NOT EDIT - This file generated automatically by intel_device_serialize_c.py script */

#include "dev/intel_device_info.h"
#include "brw_compiler.h"
#define SHA_UPDATE_FIELD(field)     _mesa_sha1_update(ctx, &devinfo->field, sizeof(devinfo->field))

void
brw_device_sha1_update(struct mesa_sha1 *ctx,
                       const struct intel_device_info *devinfo) {
% for member in compiler_fields:
% if member.ray_tracing_field:
   if (devinfo->has_ray_tracing)
      SHA_UPDATE_FIELD(${member.name});
% else:
   SHA_UPDATE_FIELD(${member.name});
% endif
% endfor
}

#undef SHA_UPDATE_FIELD

"""

def main():
    """print intel_device_serialize.c at the specified path"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--outdir', required=True,
                        help='Directory to put the generated files in')
    args = parser.parse_args()
    path = os.path.join(args.outdir, 'brw_device_sha1_gen.c')
    device_members = intel_device_info.TYPES_BY_NAME["intel_device_info"].members
    compiler_fields = [field for field in device_members if field.compiler_field]
    with open(path, 'w', encoding='utf-8') as f:
        try:
            f.write(Template(template).render(compiler_fields=compiler_fields))
        except:
            print(exceptions.text_error_template().render(compiler_fields=compiler_fields))

if __name__ == "__main__":
    main()
