va_sources = files(
  'gstvaallocator.c',
  'gstvadisplay.c',
  'gstvadisplay_wrapped.c',
  'gstvapool.c',
  'gstvautils.c',
)

va_sources_priv = files(
  'gstvasurfacecopy.c',
  'gstvavideoformat.c',
  'vasurfaceimage.c',
)

va_headers = files(
  'gstva.h',
  'gstvaallocator.h',
  'gstvadisplay.h',
  'gstvadisplay_wrapped.h',
  'gstvapool.h',
  'gstvautils.h',
  'va-prelude.h',
  'va_fwd.h',
)

va_linux_sources = files(
  'gstvadisplay_drm.c',
)

va_linux_headers = files(
  'gstvadisplay_drm.h',
)

va_win32_sources = files(
  'gstvadisplay_win32.cpp',
)

va_win32_headers = files(
  'gstvadisplay_win32.h',
)

gstva_dep = dependency('', required : false)
platform_deps = []
extra_args = ['-DGST_USE_UNSTABLE_API',
              '-DBUILDING_GST_VA',
              '-DG_LOG_DOMAIN="GStreamer-VA"']

if host_system not in ['linux', 'windows']
  subdir_done()
endif

va_opt = get_option('va')
if host_system == 'linux'
  msdk_opt = get_option('msdk')
  qsv_opt = get_option('qsv')
  if va_opt.disabled() and msdk_opt.disabled() and qsv_opt.disabled()
    subdir_done()
  endif
  va_required = va_opt.enabled() or msdk_opt.enabled() or qsv_opt.enabled()
else
  if va_opt.disabled()
    subdir_done()
  endif
  va_required = va_opt
endif

libva_req = ['>= 1.12']
if host_system == 'windows'
  libva_req = ['>= 1.18']
endif

libva_dep = dependency('libva', version: libva_req, required: va_required,
    fallback: ['libva', 'libva_dep'])
if not libva_dep.found()
  subdir_done()
endif

if host_system == 'linux'
  libva_drm_dep = dependency('libva-drm', version: libva_req, required: va_required,
      fallback: ['libva', 'libva_drm_dep'])
  if not libva_drm_dep.found()
    subdir_done()
  endif
  platform_deps += [libva_drm_dep]

  libdrm_dep = dependency('libdrm', version: '>=2.4', required: get_option('drm'))
  if libdrm_dep.found()
    platform_deps += [libdrm_dep]
    extra_args += ['-DHAVE_LIBDRM']
  endif

  va_sources += va_linux_sources
  va_headers += va_linux_headers
else
  libva_win32_dep = dependency('libva-win32', version: libva_req, required: va_required,
      fallback: ['libva', 'libva_win32_dep'])
  if not libva_win32_dep.found()
    subdir_done()
  endif

  dxgi_lib = cc.find_library('dxgi', required: va_required)
  if not dxgi_lib.found()
    subdir_done()
  endif

  platform_deps += [libva_win32_dep, dxgi_lib]

  va_sources += va_win32_sources
  va_headers += va_win32_headers
endif

va_enums = gnome.mkenums_simple('va-enumtypes',
  sources: ['gstva.h'],
  decorator: 'GST_VA_API',
  header_prefix: '#include <gst/va/va-prelude.h>',
  body_prefix: '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/va'),
)

va_enumtypes_h = va_enums[1]
va_gen_sources = [va_enumtypes_h]

gstva = library('gstva-' + api_version,
  va_sources, va_sources_priv, va_enums,
  c_args : gst_plugins_bad_args + extra_args,
  cpp_args : gst_plugins_bad_args + extra_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gst_dep, gstbase_dep, gstvideo_dep, gstallocators_dep, libva_dep] + platform_deps,
)

library_def = {'lib': gstva}
pkg_name = 'gstreamer-va-' + api_version

pkgconfig.generate(gstva,
  libraries : [gst_dep, gstvideo_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'GStreamer VA support',
)

if build_gir
  gir = {
    'sources' : [va_sources, va_headers, va_enums],
    'namespace' : 'GstVa',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    'dependencies' : [gst_dep, gstbase_dep, gstvideo_dep, gstallocators_dep, libva_dep] + platform_deps
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    va_gir = gnome.generate_gir(gstva, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [va_gir]}
    va_gen_sources += va_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

install_headers(va_headers, subdir : 'gstreamer-1.0/gst/va/')

gstva_dep = declare_dependency(link_with : gstva,
  include_directories : [libsinc],
  dependencies : [gst_dep, gstvideo_dep, gstallocators_dep, libva_dep] + platform_deps,
  sources : va_gen_sources)

meson.override_dependency(pkg_name, gstva_dep)
