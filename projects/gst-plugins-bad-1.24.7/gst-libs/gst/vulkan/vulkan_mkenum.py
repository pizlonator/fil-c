#!/usr/bin/env python3

# This is in its own file rather than inside meson.build
# because a) mixing the two is ugly and b) trying to
# make special characters such as \n go through all
# backends is a fool's errand.

import sys, subprocess

h_array = ['--fhead',
           "#pragma once\n\n#include <gst/gst.h>\n#include <gst/vulkan/vulkan_fwd.h>\nG_BEGIN_DECLS\n",
           '--fprod',
           "\n/* enumerations from \"@basename@\" */\n",
           '--vhead',
           "GST_VULKAN_API\nGType @enum_name@_get_type (void);\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n",
           '--ftail',
           "G_END_DECLS"
]

c_array = ['--fhead',
           "#ifdef HAVE_CONFIG_H\n#include \"config.h\"\n#endif\n#include \"vulkan-enumtypes.h\"\n\n#include <gst/vulkan/vulkan.h>\n\n#define C_ENUM(v) ((gint) v)\n#define C_FLAGS(v) ((guint) v)",
           '--fprod',
           "\n/* enumerations from \"@basename@\" */",
           '--vhead',
           "GType\n@enum_name@_get_type (void)\n{\n  static gsize static_g_define_type_id = 0;\n  if (g_once_init_enter (&static_g_define_type_id)) {\n    static const G@Type@Value values[] = {",
           '--vprod',
           "      { C_@TYPE@ (@VALUENAME@), \"@VALUENAME@\", \"@valuenick@\" },",
           '--vtail',
           "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&static_g_define_type_id, g_define_type_id);\n  }\n  return static_g_define_type_id;\n}\n"
]

cmd = []
argn = 1
# Find the full command needed to run glib-mkenums
# On UNIX-like, this is just the full path to glib-mkenums
# On Windows, this is the full path to interpreter + full path to glib-mkenums
for arg in sys.argv[1:]:
    cmd.append(arg)
    argn += 1
    if arg.endswith('glib-mkenums'):
        break
ofilename = sys.argv[argn]
headers = sys.argv[argn + 1:]

if ofilename.endswith('.h'):
    arg_array = h_array
else:
    arg_array = c_array

cmd_array = cmd + arg_array + headers
pc = subprocess.Popen(cmd_array, stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)
open(ofilename, 'wb').write(stdo)
