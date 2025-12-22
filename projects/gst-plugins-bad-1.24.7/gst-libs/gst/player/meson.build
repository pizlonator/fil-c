gstplayer_sources = files([
  'gstplayer.c',
  'gstplayer-signal-dispatcher.c',
  'gstplayer-video-renderer.c',
  'gstplayer-media-info.c',
  'gstplayer-g-main-context-signal-dispatcher.c',
  'gstplayer-video-overlay-video-renderer.c',
  'gstplayer-wrapped-video-renderer.c',
  'gstplayer-visualization.c',
])

gstplayer_headers = files([
  'player.h',
  'player-prelude.h',
  'gstplayer.h',
  'gstplayer-types.h',
  'gstplayer-signal-dispatcher.h',
  'gstplayer-video-renderer.h',
  'gstplayer-media-info.h',
  'gstplayer-g-main-context-signal-dispatcher.h',
  'gstplayer-video-overlay-video-renderer.h',
  'gstplayer-visualization.h',
])

install_headers(gstplayer_headers, subdir : 'gstreamer-' + api_version + '/gst/player/')

gstplayer = library('gstplayer-' + api_version,
  gstplayer_sources,
  c_args : gst_plugins_bad_args + ['-DBUILDING_GST_PLAYER', '-DG_LOG_DOMAIN="GStreamer-Player"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep, gstplay_dep,
                  gsttag_dep, gstpbutils_dep],
)

library_def = {'lib': gstplayer}
pkg_name = 'gstreamer-player-1.0'
pkgconfig.generate(gstplayer,
  libraries : [gst_dep, gstvideo_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : 'gstreamer-player-1.0',
  description : 'GStreamer Player convenience library',
)

gen_sources = []
if build_gir
  gir = {
    'sources' : gstplayer_sources + gstplayer_headers,
    'namespace' : 'GstPlayer',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstPbutils-1.0', 'GstBase-1.0', 'GstVideo-1.0',
      'GstAudio-1.0', 'GstTag-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'] + ['--c-include=gst/player/player.h'],
    'dependencies' : [gstbase_dep, gstvideo_dep, gstaudio_dep, gstplay_dep,
                      gsttag_dep, gstpbutils_dep]
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    player_gir = gnome.generate_gir(gstplayer, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [player_gir]}
    gen_sources += player_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstplayer_dep = declare_dependency(link_with : gstplayer,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep, gstplay_dep,
                  gsttag_dep, gstpbutils_dep])

meson.override_dependency(pkg_name, gstplayer_dep)
