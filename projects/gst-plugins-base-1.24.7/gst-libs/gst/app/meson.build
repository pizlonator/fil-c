app_sources = files(['gstappsrc.c', 'gstappsink.c', 'gstapputils.c'])

app_mkenum_headers = files([
  'gstappsrc.h',
])

app_headers = app_mkenum_headers + files([ 'app.h', 'app-prelude.h', 'gstappsrc.h', 'gstappsink.h' ])
install_headers(app_headers, subdir : 'gstreamer-1.0/gst/app/')

app_enums = gnome.mkenums_simple('app-enumtypes',
  sources : app_mkenum_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/app/app-prelude.h>',
  decorator : 'GST_APP_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/app'))
gstapp_c = app_enums[0]
gstapp_h = app_enums[1]

app_gen_sources = [gstapp_h]

gstapp = library('gstapp-@0@'.format(api_version),
  app_sources, gstapp_h, gstapp_c,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_APP', '-DG_LOG_DOMAIN="GStreamer-App"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gst_base_dep],
)

pkg_name = 'gstreamer-app-1.0'
pkgconfig.generate(gstapp,
  libraries : [gst_dep, gst_base_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Helper functions and base classes for application integration',
)

library_def = {'lib': gstapp}
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/app/app.h' ]
  gir = {
    'sources' : app_sources + app_headers + [gstapp_c] + [gstapp_h],
    'namespace' : 'GstApp',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : [gst_dep, gst_base_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    app_gir = gnome.generate_gir(gstapp, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [app_gir]}
    app_gen_sources += app_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

app_dep = declare_dependency(link_with: gstapp,
  include_directories : [libsinc],
  dependencies : [gst_base_dep],
  sources : app_gen_sources)

meson.override_dependency(pkg_name, app_dep)
