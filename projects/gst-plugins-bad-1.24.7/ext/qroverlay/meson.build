qrencode_dep = dependency('libqrencode', required: get_option('qroverlay'))
if qrencode_dep.found()
  json_dep = dependency('json-glib-1.0', fallback : ['json-glib', 'json_glib_dep'], required: get_option('qroverlay'))
  if json_dep.found()
    gstqroverlay = library('gstqroverlay', ['gstqroverlay.c', 'gstdebugqroverlay.c', 'gstbaseqroverlay.c', 'gstqroverlayelement.c', 'gstqroverlayplugin.c'],
      c_args : gst_plugins_bad_args,
      include_directories : [configinc],
      dependencies : [gstvideo_dep, qrencode_dep, json_dep],
      install : true,
      install_dir : plugins_install_dir,
    )
    plugins += [gstqroverlay]
  endif
endif

