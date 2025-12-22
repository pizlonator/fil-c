d3dvideosink_sources = [
  'd3dhelpers.c',
  'd3dvideosink.c',
  'gstd3d9overlay.c'
]

if host_system != 'windows' or get_option('d3dvideosink').disabled()
  subdir_done()
endif

d3d_dep = cc.find_library('d3d9', required : get_option('d3dvideosink'))
gdi_dep = cc.find_library('gdi32', required : get_option('d3dvideosink'))
have_d3d9_h = cc.has_header('d3d9.h')
if not have_d3d9_h and get_option('d3dvideosink').enabled()
  error('d3dvideosink plugin enabled but d3d9.h not found')
endif

if d3d_dep.found() and gdi_dep.found() and have_d3d9_h
  gstd3dvideosink = library('gstd3d',
    d3dvideosink_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, d3d_dep, gdi_dep],
    install : true,
    install_dir : plugins_install_dir)
  plugins += [gstd3dvideosink]
endif
