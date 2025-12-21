audio_src = files([
  'audio.c',
  'audio-buffer.c',
  'audio-channel-mixer.c',
  'audio-channels.c',
  'audio-converter.c',
  'audio-format.c',
  'audio-info.c',
  'audio-quantize.c',
  'audio-resampler.c',
  'gstaudioaggregator.c',
  'gstaudiobasesink.c',
  'gstaudiobasesrc.c',
  'gstaudiocdsrc.c',
  'gstaudioclock.c',
  'gstaudiodecoder.c',
  'gstaudioencoder.c',
  'gstaudiofilter.c',
  'gstaudioiec61937.c',
  'gstaudiometa.c',
  'gstaudioringbuffer.c',
  'gstaudiosink.c',
  'gstaudiosrc.c',
  'gstaudioutilsprivate.c',
  'streamvolume.c',
  'gstaudiostreamalign.c',
  'gstdsd.c',
  'gstdsdformat.c',
])

audio_mkenum_headers = files([
  'audio.h',
  'audio-format.h',
  'audio-channels.h',
  'audio-channel-mixer.h',
  'audio-converter.h',
  'audio-info.h',
  'audio-quantize.h',
  'audio-resampler.h',
  'gstaudioaggregator.h',
  'gstaudioringbuffer.h',
  'gstaudiobasesrc.h',
  'gstaudiocdsrc.h',
  'gstaudiobasesink.h',
  'gstaudiostreamalign.h',
  'gstdsd.h',
  'gstdsdformat.h',
])

# FIXME: check headers
audio_headers = audio_mkenum_headers + files([
  'audio-prelude.h',
  'audio-buffer.h',
  'gstaudiobasesink.h',
  'gstaudiobasesrc.h',
  'gstaudiocdsrc.h',
  'gstaudioclock.h',
  'gstaudiodecoder.h',
  'gstaudioencoder.h',
  'gstaudiofilter.h',
  'gstaudioiec61937.h',
  'gstaudiometa.h',
  'gstaudiosink.h',
  'gstaudiosrc.h',
  'streamvolume.h',
])
install_headers(audio_headers, subdir : 'gstreamer-1.0/gst/audio/')

audio_enums = gnome.mkenums_simple('audio-enumtypes',
  sources : audio_mkenum_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/audio/audio-prelude.h>',
  decorator : 'GST_AUDIO_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/audio'))
gstaudio_c = audio_enums[0]
gstaudio_h = audio_enums[1]

audio_gen_sources = [gstaudio_h]

gstaudio_deps = [tag_dep, gst_base_dep, libm]
orcsrc = 'gstaudiopack'
if have_orcc
  gstaudio_deps += [orc_dep]
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

simd_cargs = []
simd_dependencies = []

if have_sse
  audio_resampler_sse = static_library('audio_resampler_sse',
    ['audio-resampler-x86-sse.c', gstaudio_h],
    c_args : gst_plugins_base_args + [sse_args],
    include_directories : [configinc, libsinc],
    dependencies : [gst_base_dep],
    pic : true,
    install : false
  )
  simd_cargs += ['-DHAVE_SSE']
  simd_dependencies += audio_resampler_sse
endif

if have_sse2
  audio_resampler_sse2 = static_library('audio_resampler_sse2',
    ['audio-resampler-x86-sse2.c', gstaudio_h],
    c_args : gst_plugins_base_args + [sse2_args],
    include_directories : [configinc, libsinc],
    dependencies : [gst_base_dep],
    pic : true,
    install : false
  )

  simd_cargs += ['-DHAVE_SSE2']
  simd_dependencies += audio_resampler_sse2
endif

if have_sse41
  audio_resampler_sse41 = static_library('audio_resampler_sse41',
    ['audio-resampler-x86-sse41.c', gstaudio_h],
    c_args : gst_plugins_base_args + [sse41_args],
    include_directories : [configinc, libsinc],
    dependencies : [gst_base_dep],
    pic : true,
    install : false
  )

  simd_cargs += ['-DHAVE_SSE41']
  simd_dependencies += audio_resampler_sse41
endif

gstaudio = library('gstaudio-@0@'.format(api_version),
  audio_src, gstaudio_h, gstaudio_c, orc_c, orc_h,
  c_args : gst_plugins_base_args + simd_cargs + ['-DBUILDING_GST_AUDIO', '-DG_LOG_DOMAIN="GStreamer-Audio"'],
  include_directories: [configinc, libsinc],
  link_with : simd_dependencies,
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstaudio_deps,
)
pkg_name = 'gstreamer-audio-1.0'
pkgconfig.generate(gstaudio,
  libraries : [gst_dep, gst_base_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'Audio helper functions and base classes',
)

library_def = {'lib': gstaudio}
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/audio/audio.h' ]
  gir = {
    'sources' : audio_src + audio_headers + [gstaudio_c] + [gstaudio_h],
    'namespace' : 'GstAudio',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0', 'GLib-2.0', 'GObject-2.0', 'GModule-2.0' ],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : gstaudio_deps
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    audio_gir = gnome.generate_gir(gstaudio, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [audio_gir]}
    audio_gen_sources += [audio_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

audio_dep = declare_dependency(link_with : gstaudio,
  include_directories : [libsinc],
  dependencies : gstaudio_deps,
  sources : audio_gen_sources)

meson.override_dependency(pkg_name, audio_dep)
