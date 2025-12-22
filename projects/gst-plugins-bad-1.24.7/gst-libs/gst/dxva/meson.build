dxva_sources = files(
  'gstdxvaav1decoder.cpp',
  'gstdxvah264decoder.cpp',
  'gstdxvah265decoder.cpp',
  'gstdxvampeg2decoder.cpp',
  'gstdxvautils.cpp',
  'gstdxvavp8decoder.cpp',
  'gstdxvavp9decoder.cpp',
)

dxva_headers = files(
  'gstdxvaav1decoder.h',
  'gstdxvah264decoder.h',
  'gstdxvah265decoder.h',
  'gstdxvampeg2decoder.h',
  'gstdxvatypes.h',
  'gstdxvautils.h',
  'gstdxvavp8decoder.h',
  'gstdxvavp9decoder.h',
)

extra_args = [
  '-DGST_USE_UNSTABLE_API',
  '-DBUILDING_GST_DXVA',
  '-DG_LOG_DOMAIN="GStreamer-Dxva"',
]

if host_system != 'windows' and not build_gir
  subdir_done()
endif

pkg_name = 'gstreamer-dxva-' + api_version
gstdxva = library('gstdxva-' + api_version,
  dxva_sources,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gstvideo_dep, gstcodecs_dep]
)

library_def = {'lib': gstdxva}
gen_sources = []
if build_gir
  gir = {
    'sources' : dxva_sources + dxva_headers,
    'namespace' : 'GstDxva',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstVideo-1.0', 'GstCodecs-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    'dependencies' : [gstvideo_dep, gstcodecs_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    dxva_gir = gnome.generate_gir(gstdxva, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [dxva_gir]}
    gen_sources += dxva_gir
  endif
endif

gst_libraries += [[pkg_name, library_def]]

# Still non-public api, should not install headers
gstdxva_dep = declare_dependency(link_with : gstdxva,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [gstvideo_dep, gstcodecs_dep])

meson.override_dependency(pkg_name, gstdxva_dep)
