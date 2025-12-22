gstplay_sources = files(
  'gstplay.c',
  'gstplay-signal-adapter.c',
  'gstplay-video-renderer.c',
  'gstplay-media-info.c',
  'gstplay-visualization.c',
  'gstplay-video-overlay-video-renderer.c',
)

gstplay_headers = files(
  'play.h',
  'play-prelude.h',
  'gstplay.h',
  'gstplay-types.h',
  'gstplay-signal-adapter.h',
  'gstplay-video-renderer.h',
  'gstplay-media-info.h',
  'gstplay-video-overlay-video-renderer.h',
  'gstplay-visualization.h',
)

install_headers(gstplay_headers, subdir : 'gstreamer-' + api_version + '/gst/play/')

gstplay = library('gstplay-' + api_version,
  gstplay_sources,
  c_args : gst_plugins_bad_args + ['-DBUILDING_GST_PLAY', '-DG_LOG_DOMAIN="GStreamer-Play"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep,
                  gsttag_dep, gstpbutils_dep],
)

pkg_name = 'gstreamer-play-1.0'
pkgconfig.generate(gstplay,
  libraries : [gst_dep, gstvideo_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'GStreamer Player convenience library',
)

library_def = {'lib': gstplay}
gen_sources = []
if build_gir
  gir = {
    'sources' : gstplay_sources + gstplay_headers,
    'namespace' : 'GstPlay',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstPbutils-1.0', 'GstBase-1.0', 'GstVideo-1.0',
      'GstAudio-1.0', 'GstTag-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'] + ['--c-include=gst/play/play.h'],
    'dependencies' : [gstbase_dep, gstvideo_dep, gstaudio_dep,
                    gsttag_dep, gstpbutils_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    play_gir = gnome.generate_gir(gstplay, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [play_gir]}
    gen_sources += play_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstplay_dep = declare_dependency(link_with : gstplay,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep,
                  gsttag_dep, gstpbutils_dep])

meson.override_dependency(pkg_name, gstplay_dep)
