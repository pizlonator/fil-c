flite_sources = [
  'gstflite.c',
  'gstflitetestsrc.c',
]

flite_libs = [
  'flite',
  'flite_cmu_us_kal',
  'flite_usenglish',
  'flite_cmulex'
]

if get_option('flite').disabled()
  subdir_done()
endif

flite_deps = [ ]
foreach flite_lib : flite_libs
  flite_lib_dep = cc.find_library(flite_lib, required : get_option('flite'))
  if flite_lib_dep.found()
    flite_deps += [flite_lib_dep]
  endif
endforeach
have_flite_h = cc.has_header_symbol('flite/flite.h', 'flite_init')
if not have_flite_h and get_option('flite').enabled()
  error('flite plugin enabled, but flite.h not found')
endif

if flite_deps.length() == flite_libs.length() and have_flite_h
  gstflite = library('gstflite',
    flite_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep] + flite_deps,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstflite]
endif
