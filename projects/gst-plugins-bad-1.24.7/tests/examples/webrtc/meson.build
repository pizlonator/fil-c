examples = ['webrtc', 'webrtcbidirectional', 'webrtcswap', 'webrtctransceiver', 'webrtcrenego']

foreach example : examples
  exe_name = example
  src_file = '@0@.c'.format(example)

  executable(exe_name,
    src_file,
    install: false,
    include_directories : [configinc],
    dependencies : [gst_dep, gstwebrtc_dep],
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  )
endforeach

