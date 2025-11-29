#!/usr/bin/env python3

import sys
import re
import os

name = os.path.splitext(os.path.splitext(os.path.basename(sys.argv[1]))[0])[0][6:]
var_name = "gsk_gpu_" + name.replace('-', '_')
struct_name = "GskGpu" + name.title().replace('-', '') + "Instance"

with open(sys.argv[1]) as f:
    lines = f.readlines()
    matches = []

    for line in lines:
        match = re.search(r"^IN\(([0-9]+)\) ([a-z0-9]+) ([a-zA-Z0-9_]+);$", line)
        if not match:
            if re.search(r"layout.*\sin\s.*", line):
                raise Exception("Failed to parse file")
            continue
        if not match.group(3).startswith('in'):
            raise Exception("Variable doesn't start with 'in'")
        matches.append({'name': ''.join('_' + char.lower() if char.isupper() else char for char in match.group(3))[3:],
                        'attrib_name': match.group(3),
                        'location': int(match.group(1)),
                        'type': match.group(2)})

print(f'''/* This file is auto-generated; any change will not be preserved */
#pragma once

typedef struct _{struct_name} {struct_name};

struct _{struct_name} {{''')

expected = 0
for match in matches:
    if expected != int(match['location']):
        raise Exception(f"Should be layout location {expected} but is {match['location']}")  # noqa

    if match['type'] == 'float':
        print(f"  float {match['name']};")
        expected += 1
    elif match['type'] == 'int':
        print(f"  gint32 {match['name']};")
        expected += 1
    elif match['type'] == 'uint':
        print(f"  guint32 {match['name']};")
        expected += 1
    elif match['type'] == 'uvec2':
        print(f"  guint32 {match['name']}[2];")
        expected += 1
    elif match['type'] == 'vec2':
        print(f"  float {match['name']}[2];")
        expected += 1
    elif match['type'] == 'vec3':
        print(f"  float {match['name']}[3];")
        expected += 1
    elif match['type'] == 'vec4':
        print(f"  float {match['name']}[4];")
        expected += 1
    elif match['type'] == 'mat3x4':
        print(f"  float {match['name']}[12];")
        expected += 3
    elif match['type'] == 'mat4':
        print(f"  float {match['name']}[16];")
        expected += 4
    else:
        raise Exception(f"Don't know what a {match['type']} is")

print(f'''}};
''')

print(f'''static inline void
{var_name}_setup_vao (gsize offset)
{{''')
for i, match in enumerate(matches):
    if match['type'] == 'float':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         1,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'uint':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribIPointer ({match['location']},
                          1,
                          GL_UNSIGNED_INT,
                          sizeof ({struct_name}),
                          GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'uvec2':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribIPointer ({match['location']},
                          2,
                          GL_UNSIGNED_INT,
                          sizeof ({struct_name}),
                          GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'vec2':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'vec3':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         3,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'vec4':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));''');
    elif match['type'] == 'mat3x4':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));
  glEnableVertexAttribArray ({int(match['location'] + 1)});
  glVertexAttribDivisor ({int(match['location'] + 1)}, 1);
  glVertexAttribPointer ({int(match['location']) + 1},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4));
  glEnableVertexAttribArray ({int(match['location'] + 2)});
  glVertexAttribDivisor ({int(match['location'] + 2)}, 1);
  glVertexAttribPointer ({int(match['location']) + 2},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8));''')
    elif match['type'] == 'mat4':
        print(f'''  glEnableVertexAttribArray ({match['location']});
  glVertexAttribDivisor ({match['location']}, 1);
  glVertexAttribPointer ({match['location']},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']})));
  glEnableVertexAttribArray ({int(match['location'] + 1)});
  glVertexAttribDivisor ({int(match['location'] + 1)}, 1);
  glVertexAttribPointer ({int(match['location']) + 1},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4));
  glEnableVertexAttribArray ({int(match['location'] + 2)});
  glVertexAttribDivisor ({int(match['location'] + 2)}, 1);
  glVertexAttribPointer ({int(match['location']) + 2},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8));
  glEnableVertexAttribArray ({int(match['location'] + 3)});
  glVertexAttribDivisor ({int(match['location'] + 3)}, 1);
  glVertexAttribPointer ({int(match['location']) + 3},
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         sizeof ({struct_name}),
                         GSIZE_TO_POINTER (offset + G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 12));''')
    else:
        raise Exception(f"Don't know what a {match['type']} is")

print(f'''}}

''');


print(f'''static void
{var_name}_setup_attrib_locations (GLuint program)
{{''')

for match in matches:
    print(f'''  glBindAttribLocation (program, {match['location']}, "{match['attrib_name']}");''')

print(f'''}}

''');


print(f'''#ifdef GDK_RENDERING_VULKAN

static const VkPipelineVertexInputStateCreateInfo {var_name}_info = {{
  .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  .vertexBindingDescriptionCount = 1,
  .pVertexBindingDescriptions = (VkVertexInputBindingDescription[1]) {{
      {{
        .binding = 0,
        .stride = sizeof ({struct_name}),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }}
  }},
  .vertexAttributeDescriptionCount = {expected},
  .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[{expected}]) {{''')

for match in matches:
    if match['type'] == 'float':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'int':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_SINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

    elif match['type'] == 'uint':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_UINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'uvec2':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32_UINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'vec2':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'vec3':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'vec4':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')
    elif match['type'] == 'mat3x4':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},
      {{
        .location = {int(match['location']) + 1},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4,
      }},
      {{
        .location = {int(match['location']) + 2},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8,
      }},''')
    elif match['type'] == 'mat4':
        print(f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},
      {{
        .location = {int(match['location']) + 1},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4,
      }},
      {{
        .location = {int(match['location']) + 2},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8,
      }},
      {{
        .location = {int(match['location']) + 3},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 12,
      }},''')
    else:
        raise Exception(f"Don't know what a {match['type']} is")

print(f'''  }},
}};

#endif
''');
