foreach fname : ['ts-parser.c', 'ts-section-writer.c', 'ts-scte-writer.c', 'tsmux-prog-map.c']
  exe_name = fname.split('.').get(0).underscorify()

  executable(exe_name,
    fname,
    install: false,
    include_directories : [configinc],
    dependencies : [gstmpegts_dep],
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  )
endforeach
