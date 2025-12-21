extra_args = []
if cc.has_argument ('-Wno-parentheses')
  extra_args += '-Wno-parentheses'
endif

if host_system == 'darwin'
  extra_c_args = ['-xobjective-c']
else
  extra_c_args = []
endif

gstgtkhelper = static_library ('gstgtkhelper',
  ['gstgtk.c'],
  c_args : gst_plugins_base_args + extra_c_args,
  include_directories : [configinc, libsinc],
  dependencies : [gst_base_dep, video_dep, gtk_dep, gstgl_dep],
  install : false)

gstgtkhelper_dep = declare_dependency(link_with: gstgtkhelper,
  compile_args : extra_args,
  include_directories : include_directories('.'),
  dependencies : [gst_base_dep, video_dep, gtk_dep, gstgl_dep])

subdir('filternovideooverlay')
subdir('filtervideooverlay')
subdir('fxtest')
subdir('switchvideooverlay')
subdir('3dvideo')
