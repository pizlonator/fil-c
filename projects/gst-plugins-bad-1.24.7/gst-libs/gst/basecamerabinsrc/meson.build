camerabin_sources = files(
  'gstcamerabin-enum.c',
  'gstcamerabinpreview.c',
  'gstbasecamerasrc.c',
)
camerabin_headers = files(
  'basecamerabinsrc-prelude.h',
  'gstcamerabin-enum.h',
  'gstcamerabinpreview.h',
  'gstbasecamerasrc.h',
)
install_headers(camerabin_headers, subdir : 'gstreamer-1.0/gst/basecamerabinsrc')

gstbasecamerabin = library('gstbasecamerabinsrc-' + api_version,
  camerabin_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_BASE_CAMERA_BIN_SRC', '-DG_LOG_DOMAIN="GStreamer-BaseCameraBinSrc"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstapp_dep],
)

library_def = {'lib': gstbasecamerabin}
pkg_name = 'gstreamer-bad-base-camerabinsrc-1.0'
gen_sources = []
if build_gir and not static_build
  gir = {
    'sources' : camerabin_sources + camerabin_headers,
    'namespace' : 'GstBadBaseCameraBin',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstApp-1.0'],
    'install' : false, # Only for the documentation
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    'dependencies' : [gstapp_dep],
    'build_by_default' : true,
  }
  library_def += {'gir': [gir]}
  if not static_build
    basecamerabin_gir = gnome.generate_gir(gstbasecamerabin, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [basecamerabin_gir]}
    gen_sources += basecamerabin_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstbasecamerabin_dep = declare_dependency(link_with : gstbasecamerabin,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [gstapp_dep])
meson.override_dependency(pkg_name, gstbasecamerabin_dep)
