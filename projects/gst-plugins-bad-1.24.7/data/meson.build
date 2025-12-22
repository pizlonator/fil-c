encoding_targetsdir = join_paths(get_option('datadir'),
    'gstreamer-' + api_version, 'encoding-profiles')

encoding_targets = [
  ['file-extension', ['targets/file-extension/ogv.gep',
                      'targets/file-extension/oga.gep',
                      'targets/file-extension/mkv.gep',
                      'targets/file-extension/mp3.gep',
                      'targets/file-extension/webm.gep',
                      'targets/file-extension/flv.gep',
                      'targets/file-extension/mp4.gep',
                      'targets/file-extension/ts.gep',
                      'targets/file-extension/avi.gep',],
  ],
  ['online-services', ['targets/online-service/youtube.gep',]],
  ['device', ['targets/device/dvd.gep',]],
]

srcdirs = []

foreach path_targets : encoding_targets
  dir = join_paths(encoding_targetsdir, path_targets.get(0))
  etargets = path_targets.get(1)
  install_data(sources: etargets, install_dir: dir)
  srcdirs += meson.current_source_dir() / 'targets' / path_targets.get(0)
endforeach

env = environment()
env.prepend('GST_ENCODING_TARGET_PATH', srcdirs)
meson.add_devenv(env)
