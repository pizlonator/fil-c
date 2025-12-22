siren_sources = [
  'gstsiren.c',
  'gstsirendec.c',
  'gstsirenenc.c',
  'common.c',
  'dct4.c',
  'decoder.c',
  'encoder.c',
  'huffman.c',
  'rmlt.c',
]

gstsiren = library('gstsiren',
  siren_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstsiren]
