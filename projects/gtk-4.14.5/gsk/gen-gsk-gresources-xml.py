#!/usr/bin/env python3
#
# Generate gsk.resources.xml
#
# Usage: gen-gsk-gresources-xml OUTPUT-FILE [INPUT-FILE1] [INPUT-FILE2] ...

import os, sys
import filecmp

def replace_if_changed(new, old):
  '''
  Compare contents and only replace if changed to avoid triggering a rebuild.
  '''
  try:
    changed = not filecmp.cmp(new, old, shallow=False)
  except FileNotFoundError:
    changed = True
  if changed:
    os.replace(new, old)
  else:
    os.remove(new)

gl_source_shaders = []
ngl_source_shaders = []
vulkan_compiled_shaders = []
gpu_vulkan_compiled_shaders = []
vulkan_shaders = []

for f in sys.argv[2:]:
  if f.endswith('.glsl'):
    if f.find('gsk/gpu') > -1:
      ngl_source_shaders.append(f)
    else:
      gl_source_shaders.append(f)
  elif f.endswith('.spv'):
    if f.find('gsk/gpu') > -1:
      gpu_vulkan_compiled_shaders.append(f)
    else:
      vulkan_compiled_shaders.append(f)
  elif f.endswith('.frag') or f.endswith('.vert'):
    vulkan_shaders.append(f)
  else:
    raise Exception(f"No idea what XML to generate for {f}")

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgsk'>

'''

for f in gl_source_shaders:
  xml += '    <file alias=\'gl/{0}\'>gl/resources/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in ngl_source_shaders:
  xml += '    <file alias=\'shaders/gl/{0}\'>gpu/shaders/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in vulkan_compiled_shaders:
  xml += '    <file alias=\'vulkan/{0}\'>vulkan/resources/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in gpu_vulkan_compiled_shaders:
  xml += '    <file alias=\'shaders/vulkan/{0}\'>gpu/shaders/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in vulkan_shaders:
  xml += '    <file alias=\'vulkan/{0}\'>vulkan/resources/{0}</file>\n'.format(os.path.basename(f))

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 1 and sys.argv[1] != '-':
  outfile = sys.argv[1]
  tmpfile = outfile + '~'
  with open(tmpfile, 'w') as f:
    f.write(xml)
  replace_if_changed(tmpfile, outfile)
else:
  print(xml)
