spandsp_sources = [
  'gstdtmfdetect.c',
  'gstdtmfdetect.h',
  'gstspandsp.c',
  'gstspanplc.c',
  'gstspanplc.h',
  'gsttonegeneratesrc.c',
  'gsttonegeneratesrc.h',
]

spandsp_dep = dependency('spandsp', version : '>= 0.0.6', required : get_option('spandsp'))

if spandsp_dep.found()
  gstspandsp = library('gstspandsp',
    spandsp_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, spandsp_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstspandsp]
endif
