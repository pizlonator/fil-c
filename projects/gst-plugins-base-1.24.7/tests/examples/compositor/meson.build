executable('crossfade', 'crossfade.c',
  include_directories: [configinc],
  c_args: ['-DHAVE_CONFIG_H'],
  dependencies: [gst_controller_dep, gst_dep],
  install: false)

executable('mosaic', 'mosaic.c',
  include_directories: [configinc],
  c_args: ['-DHAVE_CONFIG_H'],
  dependencies: [video_dep, gst_dep],
  install: false)

executable('signals', 'signals.c',
  include_directories: [configinc],
  c_args: ['-DHAVE_CONFIG_H'],
  dependencies: [gst_dep, gst_base_dep],
  install: false)
