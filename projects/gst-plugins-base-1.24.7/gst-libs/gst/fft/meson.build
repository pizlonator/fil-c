fft_sources = [
  'gstfft.c',
  'gstffts16.c',
  'gstffts32.c',
  'gstfftf32.c',
  'gstfftf64.c',
  'kiss_fft_s16.c',
  'kiss_fft_s32.c',
  'kiss_fft_f32.c',
  'kiss_fft_f64.c',
  'kiss_fftr_s16.c',
  'kiss_fftr_s32.c',
  'kiss_fftr_f32.c',
  'kiss_fftr_f64.c'
]

fft_headers = [
  'fft.h',
  'fft-prelude.h',
  'gstfftf32.h',
  'gstfftf64.h',
  'gstfft.h',
  'gstffts16.h',
  'gstffts32.h',
]
install_headers(fft_headers, subdir : 'gstreamer-1.0/gst/fft/')

gstfft = library('gstfft-@0@'.format(api_version),
  fft_sources,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_FFT', '-DG_LOG_DOMAIN="GStreamer-FFT"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gst_dep, libm],
)
pkg_name = 'gstreamer-fft-1.0'
pkgconfig.generate(gstfft,
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'FFT implementation',
)
gst_libraries += [[pkg_name, {'lib': gstfft}]]

fft_dep = declare_dependency(link_with: gstfft,
  include_directories : [libsinc],
  dependencies : [gst_dep])

meson.override_dependency(pkg_name, fft_dep)
