pbutils_sources = files([
  'gstpluginsbaseversion.c',
  'pbutils.c',
  'codec-utils.c',
  'descriptions.c',
  'encoding-profile.c',
  'encoding-target.c',
  'install-plugins.c',
  'missing-plugins.c',
  'gstaudiovisualizer.c',
  'gstdiscoverer.c',
  'gstdiscoverer-types.c'
])

pbconf = configuration_data()
pbconf.set('PACKAGE_VERSION_MAJOR', gst_version_major)
pbconf.set('PACKAGE_VERSION_MINOR', gst_version_minor)
pbconf.set('PACKAGE_VERSION_MICRO', gst_version_micro)
pbconf.set('PACKAGE_VERSION_NANO', gst_version_nano)
gst_pbutils_version_h = configure_file(input : 'gstpluginsbaseversion.h.in',
  output: 'gstpluginsbaseversion.h',
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/pbutils'),
  configuration: pbconf)

pbutils_headers = files([
  'pbutils.h',
  'pbutils-prelude.h',
  'codec-utils.h',
  'descriptions.h',
  'encoding-profile.h',
  'encoding-target.h',
  'install-plugins.h',
  'missing-plugins.h',
  'gstdiscoverer.h',
  'gstaudiovisualizer.h',
])
install_headers(pbutils_headers, subdir : 'gstreamer-1.0/gst/pbutils/')

pbutils_mkenum_headers = pbutils_headers

pbutils_enums = gnome.mkenums_simple('pbutils-enumtypes',
  sources : pbutils_mkenum_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/pbutils/pbutils-prelude.h>',
  decorator : 'GST_PBUTILS_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/pbutils'))
gstpbutils_c = pbutils_enums[0]
gstpbutils_h = pbutils_enums[1]

gstpbutils_deps = [video_dep, audio_dep, tag_dep]
pbutils = library('gstpbutils-@0@'.format(api_version),
  pbutils_sources, gstpbutils_c, gstpbutils_h,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_PBUTILS', '-DG_LOG_DOMAIN="GStreamer-PBUtils"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstpbutils_deps,
)

library_def = {'lib': pbutils}
pkg_name = 'gstreamer-pbutils-1.0'
pkgconfig.generate(pbutils,
  libraries : [gst_dep, video_dep, audio_dep, gst_base_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'General utility functions',
)

pbutils_gen_sources = [gstpbutils_h, gst_pbutils_version_h]
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/pbutils/pbutils.h' ]
  gir = {
    'sources' : pbutils_sources + pbutils_headers + [gstpbutils_h, gst_pbutils_version_h],
    'namespace' : 'GstPbutils',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstAudio-1.0', 'GstVideo-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : gstpbutils_deps
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    pbutils_gir = gnome.generate_gir(pbutils, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [pbutils_gir]}
    pbutils_gen_sources += [pbutils_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

pbutils_dep = declare_dependency(link_with : pbutils,
  include_directories : [libsinc],
  dependencies : gstpbutils_deps,
  sources : pbutils_gen_sources)

meson.override_dependency(pkg_name, pbutils_dep)
