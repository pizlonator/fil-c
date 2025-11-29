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

import hashlib
import os
import sys

from mako.template import Template
from mako import exceptions

from intel_device_info import TYPES, TYPES_BY_NAME, FUNDAMENTAL_TYPES, Enum

template = COPYRIGHT + """

/* DO NOT EDIT - This file generated automatically by intel_device_serialize_c.py script */

#include "dev/intel_device_info_serialize.h"
#include "util/parson.h"
<%! import intel_device_info %>
% for decl in TYPES:
% if isinstance(decl, intel_device_info.Enum):
static JSON_Value *
dump_${decl.name}(enum ${decl.name} arg) {
   switch (arg) {
% for value in decl.values:
   case ${value}:
      return json_value_init_string("${value}");
% endfor
   default:
      unreachable("invalid ${decl.name} value");
   }
}

static enum ${decl.name}
load_${decl.name}(const char * val) {
% for value in decl.values:
   if (strcmp("${value}", val) == 0)
      return ${value};
% endfor
   assert(false);
   return (enum ${decl.name}) -1;
}

% endif
% if isinstance(decl, intel_device_info.Struct):
static JSON_Value *
dump_${decl.name}(const struct ${decl.name} *arg) {
   JSON_Value *val = json_value_init_object();
   JSON_Object *json = json_object(val);
% for member in decl.members:
% if member.member_type in intel_device_info.INT_TYPES:
% if member.array:
   {
      JSON_Value *jtmp = json_value_init_array();
      JSON_Array *jarray = json_array(jtmp);
      for (unsigned i = 0; i < sizeof(arg->${member.name}) / sizeof(*arg->${member.name}); ++i)
          json_array_append_number(jarray, arg->${member.name}[i]);
      json_object_set_value(json, "${member.name}", jtmp);
   }
% else:
   json_object_set_number(json, "${member.name}", arg->${member.name});
% endif
% elif member.member_type == "char":
   json_object_set_string(json, "${member.name}", arg->${member.name});
% elif member.member_type == "bool":
   json_object_set_boolean(json, "${member.name}", arg->${member.name});
% elif isinstance(intel_device_info.TYPES_BY_NAME[member.member_type], intel_device_info.Enum):
   json_object_set_value(json, "${member.name}", dump_${member.member_type}(arg->${member.name}));
% elif isinstance(intel_device_info.TYPES_BY_NAME[member.member_type], intel_device_info.Struct):
% if member.array:
   {
      JSON_Value *jtmp = json_value_init_array();
      JSON_Array *jarray = json_array(jtmp);
      for (unsigned i = 0; i < sizeof(arg->${member.name}) / sizeof(*arg->${member.name}); ++i)
         json_array_append_value(jarray, dump_${member.member_type}(&arg->${member.name}[i]));
      json_object_set_value(json, "${member.name}", jtmp);
   }
% else:
   json_object_set_value(json, "${member.name}", dump_${member.member_type}(&arg->${member.name}));
% endif
% endif
% endfor
   return val;
}

static void
load_${decl.name}(JSON_Object *json, struct ${decl.name} *target) {
% for member in decl.members:
% if member.member_type in intel_device_info.INT_TYPES:
% if member.array:
   {
      JSON_Array *array = json_object_get_array(json, "${member.name}");
      for (unsigned i = 0; i < json_array_get_count(array); ++i)
         target->${member.name}[i] = json_array_get_number(array, i);
   }
% else:
   target->${member.name} = json_object_get_number(json, "${member.name}");
% endif
% elif member.member_type == "char":
   strncpy(target->${member.name},
           json_object_get_string(json, "${member.name}"),
           sizeof(target->${member.name}) - 1);
% elif member.member_type == "bool":
   target->${member.name} = json_object_get_boolean(json, "${member.name}");
% elif isinstance(intel_device_info.TYPES_BY_NAME[member.member_type], intel_device_info.Enum):
   target->${member.name} = load_${member.member_type}(json_object_get_string(json, "${member.name}"));
% elif isinstance(intel_device_info.TYPES_BY_NAME[member.member_type], intel_device_info.Struct):
% if member.array:
   {
      JSON_Array *array = json_object_get_array(json, "${member.name}");
      for (unsigned i = 0; i < json_array_get_count(array); ++i)
         load_${member.member_type}(json_array_get_object(array, i), target->${member.name} + i);
   }
% else:
   load_${member.member_type}(json_object_get_object(json, "${member.name}"), &target->${member.name});
% endif
% endif
% endfor
}

% endif
% endfor
bool
intel_device_info_from_json(const char *path,
                            struct intel_device_info *devinfo) {
   JSON_Value *root = json_parse_file(path);
   JSON_Object *obj = json_object(root);
   const char *devinfo_type_sha1 = "${checksum}";

   /* verify that json was generated with a compatible driver */
   if (strncmp(devinfo_type_sha1,
               json_object_get_string(obj, "devinfo_type_sha1"),
               strlen(devinfo_type_sha1)))
      return false;
   load_intel_device_info(obj, devinfo);

   /* override no_hw to indicate that the GPU has been stubbed */
   devinfo->no_hw = true;

   json_value_free(root);
   intel_device_info_init_was(devinfo);
   return true;
}

JSON_Value *
intel_device_info_dump_json(const struct intel_device_info *devinfo) {
   JSON_Value * root = dump_intel_device_info(devinfo);

   /* record a hash of the structure type declaration, to prevent loading with
    * an incompatible driver
    */
   JSON_Object *obj = json_object(root);
   json_object_set_string(obj, "devinfo_type_sha1", "${checksum}");

   return root;
}

"""

def declaration_checksum(typename):
    """Calculate a stable checksum for the struct that will change whenever any
    of the members change.  We will refer to this value as the 'type declaration hash'"""
    h = hashlib.new('sha1')
    struct_description = TYPES_BY_NAME[typename]
    for member in struct_description.members:
        h.update(member.name.encode(encoding="utf-8"))
        h.update(f"{member.array}".encode(encoding="utf-8"))
        member_type = member.member_type
        if member_type in FUNDAMENTAL_TYPES:
            h.update(member_type.encode(encoding="utf-8"))
        elif isinstance(TYPES_BY_NAME[member_type], Enum):
            h.update(member_type.encode(encoding="utf-8"))
        else:
            # encode the type declaration hash for the nested struct
            h.update(declaration_checksum(member_type).encode(encoding="utf-8"))
    return h.hexdigest()

def main():
    """print intel_device_serialize.c at the specified path"""
    if len(sys.argv) > 1:
        outf = open(sys.argv[1], 'w', encoding='utf-8')
    else:
        outf = sys.stdout
    try:
        outf.write(Template(template).render(TYPES=TYPES,
                                             checksum=declaration_checksum("intel_device_info")))
    except:
        print(exceptions.text_error_template().render(
            TYPES=TYPES,
            checksum=declaration_checksum("intel_device_info")))
        sys.exit(1)
    outf.close()

if __name__ == "__main__":
    main()
