insert_sources = files('gstinsertbin.c')
insert_headers = files('gstinsertbin.h')
install_headers(insert_headers, subdir : 'gstreamer-1.0/gst/insertbin')

gstinsertbin = library('gstinsertbin-' + api_version,
  insert_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_INSERT_BIN', '-DG_LOG_DOMAIN="GStreamer-InsertBin"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gst_dep],
)

library_def = {'lib': gstinsertbin}
pkg_name = 'gstreamer-insertbin-1.0'
pkgconfig.generate(gstinsertbin,
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Bin to automatically and insertally link elements',
)

gen_sources = []
if build_gir
  gir = {
    'sources' : insert_sources + insert_headers,
    'namespace' : 'GstInsertBin',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'] + ['--c-include=gst/insertbin/gstinsertbin.h'],
    'dependencies' : [gst_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    insertbin_gir = gnome.generate_gir(gstinsertbin, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [insertbin_gir]}
    gen_sources += insertbin_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstinsertbin_dep = declare_dependency(link_with : gstinsertbin,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [gst_dep])

meson.override_dependency(pkg_name, gstinsertbin_dep)
