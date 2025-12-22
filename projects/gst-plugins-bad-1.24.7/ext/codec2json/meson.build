codec2json_sources = files([
  'gstav12json.c',
  'gsth2642json.c',
  'gsth2652json.c',
  'gstvp82json.c',
  'plugin.c',
])

cp_args = [
  '-DGST_USE_UNSTABLE_API',
]

json_dep = dependency('json-glib-1.0', version : '>=1.6.6', fallback : ['json-glib', 'json_glib_dep'], required: get_option('codec2json'))

if json_dep.found()
  gstcodec2json = library('gstcodec2json',
    codec2json_sources,
    c_args : gst_plugins_bad_args + cp_args,
    include_directories : [configinc],
    install : true,
    install_dir : plugins_install_dir,
    dependencies : [gstvideo_dep, gstcodecparsers_dep, json_dep],
  )
  plugins += [gstcodec2json]
endif
