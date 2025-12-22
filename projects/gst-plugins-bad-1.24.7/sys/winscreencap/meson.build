winscreencap_sources = [
  'gstdx9screencapsrc.c',
  'gstgdiscreencapsrc.c',
  'gstwinscreencap.c',
]

if host_system != 'windows' or get_option('winscreencap').disabled()
  subdir_done()
endif

d3d_dep = cc.find_library('d3d9', required : get_option('winscreencap'))
gdi_dep = cc.find_library('gdi32', required : get_option('winscreencap'))
windowscodecs_dep = cc.find_library('windowscodecs',  required : false)
dxguid_dep = cc.find_library('dxguid',  required : false)

have_d3d9_h = cc.has_header('d3d9.h')
if not have_d3d9_h and get_option('winscreencap').enabled()
  error('winscreencap plugin enabled but d3d9.h not found')
endif

if d3d_dep.found() and gdi_dep.found() and have_d3d9_h
  gstwinscreencap = library('gstwinscreencap',
    winscreencap_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, d3d_dep, gdi_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstwinscreencap]
endif
