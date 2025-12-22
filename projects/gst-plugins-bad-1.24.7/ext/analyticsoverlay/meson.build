analyticsoverlay_sources = [
  'gstanalyticsoverlay.c',
  'gstobjectdetectionoverlay.c',
]

gstanalyticsoverlay_ext_dep = dependency('pangocairo', version : '>=1.22.0',
  required : get_option('analyticsoverlay'),
  fallback: ['pango', 'libpangocairo_dep'],
  default_options: ['cairo=enabled'])

if gstanalyticsoverlay_ext_dep.found()
  gstanalyticsoverlay = library('gstanalyticsoverlay',
    analyticsoverlay_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc, libsinc],
    dependencies : [
      gstbase_dep, gstvideo_dep, gstanalytics_dep, gstanalyticsoverlay_ext_dep
      ],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstanalyticsoverlay]
endif
