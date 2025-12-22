if get_option('vulkan').disabled()
  subdir_done()
endif

if not gstvulkan_dep.found()
  if get_option('vulkan').enabled()
    error('GStreamer Vulkan plugin required via options, but needed dependencies not found.')
  else
    subdir_done()
  endif
endif

# Should already have checked for this
assert(glslc.found(), 'Expected glslc to be available')

subdir('shaders')

vulkan_sources = files(
  'gstvulkan.c',
  'gstvulkanelement.c',
  'vkcolorconvert.c',
  'vkdownload.c',
  'vkdeviceprovider.c',
  'vkimageidentity.c',
  'vkshaderspv.c',
  'vksink.c',
  'vkupload.c',
  'vkviewconvert.c',
  'vkoverlaycompositor.c',
)

vulkan_plugin_enum_headers = files(
  'vkviewconvert.h',
)

extra_deps = []
extra_args = []
if vulkan_conf.get('GST_VULKAN_HAVE_VIDEO_EXTENSIONS') == 1
  vulkan_sources += files('vkh264dec.c', 'vkh265dec.c')
  extra_deps += gstcodecs_dep
  extra_args += ['-DGST_USE_UNSTABLE_API']
endif

vulkan_plugin_enums = gnome.mkenums_simple('gstvulkan-plugins-enumtypes',
    sources : vulkan_plugin_enum_headers,
    body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
    header_prefix : '#include <gst/gst.h>')

gstvulkan_plugin = library('gstvulkan',
  vulkan_sources, vulkan_compiled_shader_sources, vulkan_plugin_enums,
  c_args : gst_plugins_bad_args + extra_args,
  objc_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, gstbase_dep, gstvulkan_dep, vulkan_dep, gio_dep] + extra_deps,
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstvulkan_plugin]
