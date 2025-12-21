video_sources = files([
  'colorbalance.c',
  'colorbalancechannel.c',
  'convertframe.c',
  'gstvideoaffinetransformationmeta.c',
  'gstvideocodecalphameta.c',
  'gstvideoaggregator.c',
  'gstvideodecoder.c',
  'gstvideoencoder.c',
  'gstvideofilter.c',
  'gstvideometa.c',
  'gstvideopool.c',
  'gstvideosink.c',
  'gstvideotimecode.c',
  'gstvideoutils.c',
  'gstvideoutilsprivate.c',
  'navigation.c',
  'video.c',
  'video-anc.c',
  'video-blend.c',
  'video-chroma.c',
  'video-color.c',
  'video-converter.c',
  'video-dither.c',
  'video-event.c',
  'video-format.c',
  'video-frame.c',
  'video-hdr.c',
  'video-info.c',
  'video-info-dma.c',
  'video-multiview.c',
  'video-resampler.c',
  'video-scaler.c',
  'video-sei.c',
  'video-tile.c',
  'video-overlay-composition.c',
  'videodirection.c',
  'videoorientation.c',
  'videooverlay.c',
])

video_headers = files([
  'colorbalance.h',
  'colorbalancechannel.h',
  'gstvideoaffinetransformationmeta.h',
  'gstvideocodecalphameta.h',
  'gstvideoaggregator.h',
  'gstvideodecoder.h',
  'gstvideoencoder.h',
  'gstvideofilter.h',
  'gstvideometa.h',
  'gstvideopool.h',
  'gstvideosink.h',
  'gstvideotimecode.h',
  'gstvideoutils.h',
  'navigation.h',
  'video.h',
  'video-anc.h',
  'video-event.h',
  'video-format.h',
  'video-chroma.h',
  'video-color.h',
  'video-converter.h',
  'video-dither.h',
  'video-hdr.h',
  'video-info.h',
  'video-info-dma.h',
  'video-frame.h',
  'video-prelude.h',
  'video-scaler.h',
  'video-tile.h',
  'videodirection.h',
  'videoorientation.h',
  'videooverlay.h',
  'video-resampler.h',
  'video-blend.h',
  'video-overlay-composition.h',
  'video-multiview.h',
  'video-sei.h',
])
install_headers(video_headers, subdir : 'gstreamer-1.0/gst/video/')

video_mkenum_headers = [
  'video.h',
  'video-anc.h',
  'video-format.h',
  'video-frame.h',
  'video-chroma.h',
  'video-color.h',
  'video-converter.h',
  'video-dither.h',
  'video-info.h',
  'video-overlay-composition.h',
  'video-resampler.h',
  'video-scaler.h',
  'video-tile.h',
  'gstvideometa.h',
  'gstvideotimecode.h',
  'gstvideoutils.h',
  'gstvideoencoder.h',
  'gstvideodecoder.h',
  'colorbalance.h',
  'navigation.h',
]

video_enums = gnome.mkenums_simple('video-enumtypes',
  sources : video_mkenum_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/video/video-prelude.h>',
  decorator : 'GST_VIDEO_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/video'))
gstvideo_c = video_enums[0]
gstvideo_h = video_enums[1]
video_gen_sources = [gstvideo_h]

orcsrc = 'video-orc'
gstvideo_deps = [gst_base_dep, libm]
if have_orcc
  gstvideo_deps += [orc_dep]
  orc_h = custom_target(orcsrc + '.h',
    input : orcsrc + '.orc',
    output : orcsrc + '.h',
    command : orcc_args + ['--header', '-o', '@OUTPUT@', '@INPUT@'])
  orc_c = custom_target(orcsrc + '.c',
    input : orcsrc + '.orc',
    output : orcsrc + '.c',
    command : orcc_args + ['--implementation', '-o', '@OUTPUT@', '@INPUT@'])
  orc_targets += {'name': orcsrc, 'orc-source': files(orcsrc + '.orc'), 'header': orc_h, 'source': orc_c}
else
  orc_h = configure_file(input : orcsrc + '-dist.h',
    output : orcsrc + '.h',
    copy : true)
  orc_c = configure_file(input : orcsrc + '-dist.c',
    output : orcsrc + '.c',
    copy : true)
endif

gstvideo = library('gstvideo-@0@'.format(api_version),
  video_sources, gstvideo_h, gstvideo_c, orc_c, orc_h,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_VIDEO', '-DG_LOG_DOMAIN="GStreamer-Video"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstvideo_deps,
)

library_def = {'lib': gstvideo}
pkg_name = 'gstreamer-video-1.0'
pkgconfig.generate(gstvideo,
  libraries : [gst_dep, gst_base_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Video base classes and helper functions',
)

if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/video/video.h' ]
  gir = {
    'sources' : video_sources + video_headers + [gstvideo_c] + [gstvideo_h],
    'namespace' : 'GstVideo',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0','GstBase-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : gstvideo_deps
  }
  library_def += {'gir': [gir]}
  if not static_build
    video_gir = gnome.generate_gir(gstvideo, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [video_gir]}
    video_gen_sources += [video_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

video_dep = declare_dependency(link_with : gstvideo,
  include_directories : [libsinc],
  dependencies : gstvideo_deps,
  sources : video_gen_sources)

meson.override_dependency(pkg_name, video_dep)
