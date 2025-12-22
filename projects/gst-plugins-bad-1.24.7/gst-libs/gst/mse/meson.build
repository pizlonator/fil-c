gstmse_headers_private = files(
  'gstappendpipeline-private.h',
  'gstmediasourcesamplemap-private.h',
  'gstmediasourcetrackbuffer-private.h',
  'gstmediasourcetrack-private.h',
  'gstmseeventqueue-private.h',
  'gstmselogging-private.h',
  'gstmsemediatype-private.h',
)

gstmse_sources_private = files(
  'gstappendpipeline.c',
  'gstmediasourcesamplemap.c',
  'gstmediasourcetrackbuffer.c',
  'gstmediasourcetrack.c',
  'gstmseeventqueue.c',
  'gstmselogging.c',
  'gstmsemediatype.c',
)

gstmse_headers_public = files(
  'gstmediasource.h',
  'gstsourcebuffer.h',
  'gstsourcebufferlist.h',
  'gstmsesrc.h',
  'mse.h',
  'mse-prelude.h',
)

gstmse_sources_public = files(
  'gstmediasource.c',
  'gstmsesrc.c',
  'gstsourcebuffer.c',
  'gstsourcebufferlist.c',
)

gstmse_header_dir = 'gstreamer-' + api_version + '/gst/mse/'

gstmse_enums_private = gnome.mkenums_simple('mse-enumtypes-private',
  sources : gstmse_headers_private,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/mse/mse-prelude.h>',
  decorator: 'GST_MSE_PRIVATE',
  install_header: false,
)

gstmse_enums = gnome.mkenums_simple('mse-enumtypes',
  sources : gstmse_headers_public,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/mse/mse-prelude.h>',
  decorator: 'GST_MSE_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), gstmse_header_dir),
)

gstmse_deps = [gstbase_dep, gstapp_dep]

gstmse_enums_private_c = gstmse_enums_private[0]
gstmse_enums_private_h = gstmse_enums_private[1]

gstmse_enums_c = gstmse_enums[0]
gstmse_enums_h = gstmse_enums[1]

gstmse_c_args = gst_plugins_bad_args + [
  '-DBUILDING_GST_MSE',
  '-DGST_USE_UNSTABLE_API',
  '-DG_LOG_DOMAIN="GStreamer-MSE"',
]

gstmse_sources_all = [
  gstmse_sources_private,
  gstmse_sources_public,
  gstmse_enums_c,
  gstmse_enums_h,
  gstmse_enums_private_c,
  gstmse_enums_private_h,
]

gstmse_private_test = library('gstmse-private-test',
  gstmse_sources_all,
  c_args : [gstmse_c_args, '-DBUILDING_GST_MSE_TEST'],
  include_directories : [configinc, libsinc],
  dependencies : gstmse_deps,
)

gstmse_private_test_dep = declare_dependency(
  compile_args : [gstmse_c_args, '-DBUILDING_GST_MSE_TEST'],
  link_with : [gstmse_private_test],
  include_directories : [libsinc],
  sources: gstmse_headers_public + [gstmse_enums_h],
)

gstmse = library('gstmse-' + api_version,
  gstmse_sources_all,
  c_args : gstmse_c_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstmse_deps,
)

pkg_name = 'gstreamer-mse-' + api_version
gst_libraries += [[pkg_name, {'lib': gstmse}]]
pkgconfig.generate(gstmse,
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'GStreamer Support for W3C Media Source Extensions',
)

library_def = {'lib': gstmse}
gen_sources = []
if build_gir
  gir = {
    'sources' : [
      gstmse_sources_public,
      gstmse_headers_public,
      gstmse_enums_h,
      gstmse_enums_c,
    ],
    'namespace' : 'GstMse',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'] + ['--c-include=gst/mse/mse.h'],
    'dependencies' : gstmse_deps,
  }
  library_def += {'gir': [gir]}
  if not static_build
    mse_gir = gnome.generate_gir(gstmse, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [mse_gir]}
    gen_sources += mse_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstmse_dep = declare_dependency(
  link_with : [gstmse],
  include_directories : [libsinc],
  dependencies : gstmse_deps,
  sources: gstmse_headers_public + [gstmse_enums_h],
)
install_headers(gstmse_headers_public, subdir: gstmse_header_dir)
meson.override_dependency(pkg_name, gstmse_dep)
