midi_sources = [
  'midi.c',
  'midiparse.c',
]

gstmidi = library('gstmidi',
  midi_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gsttag_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstmidi]
