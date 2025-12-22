ladspa_sources = [
  'gstladspa.c',
  'gstladspafilter.c',
  'gstladspasource.c',
  'gstladspasink.c',
  'gstladspautils.c',
]

ladspa_cargs = []

if get_option('ladspa').disabled()
  subdir_done()
endif

have_ladspa_h = cc.has_header('ladspa.h')
if not have_ladspa_h and get_option('ladspa').enabled()
  error('ladspa plugin is enabled but ladspa.h not found')
endif

if have_ladspa_h
  lrdf_dep = dependency('lrdf', required : get_option('ladspa-rdf'))
  if lrdf_dep.found()
    ladspa_cargs = ['-DHAVE_LRDF']
  endif

  gstladspa = library('gstladspa',
    ladspa_sources,
    c_args : gst_plugins_bad_args + ladspa_cargs,
    link_args : noseh_link_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstaudio_dep, gstbase_dep, gmodule_dep, lrdf_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstladspa]
endif
