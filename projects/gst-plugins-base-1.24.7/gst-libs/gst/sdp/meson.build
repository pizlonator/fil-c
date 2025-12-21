gst_sdp_headers = files([
  'sdp.h',
  'sdp-prelude.h',
  'gstsdp.h',
  'gstmikey.h',
  'gstsdpmessage.h',
])
install_headers(gst_sdp_headers, subdir : 'gstreamer-1.0/gst/sdp/')

sdp_deps = [rtp_dep, gst_dep, gio_dep, pbutils_dep]
gst_sdp_sources = files(['gstsdpmessage.c', 'gstmikey.c'])
gstsdp = library('gstsdp-@0@'.format(api_version),
  gst_sdp_sources,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_SDP', '-DG_LOG_DOMAIN="GStreamer-SDP"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : sdp_deps,
)

library_def = {'lib': gstsdp}
pkg_name = 'gstreamer-sdp-1.0'
sdp_gen_sources  = []
pkgconfig.generate(gstsdp,
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'SDP helper functions',
)

if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/sdp/sdp.h' ]
  gir = {
    'sources' : gst_sdp_sources + gst_sdp_headers,
    'namespace' : 'GstSdp',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : sdp_deps
  }

  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    sdp_gir = gnome.generate_gir(gstsdp, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [sdp_gir]}
    sdp_gen_sources += [sdp_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

sdp_dep = declare_dependency(link_with: gstsdp,
  include_directories : [libsinc],
  dependencies : sdp_deps,
  sources: sdp_gen_sources)

meson.override_dependency(pkg_name, sdp_dep)
