subenc_sources = [
  'gstdvbsubenc.c',
  'gstdvbsubenc-util.c',
]

libimagequant_sources = [
  'libimagequant/blur.c',
  'libimagequant/libimagequant.c',
  'libimagequant/mediancut.c',
  'libimagequant/mempool.c',
  'libimagequant/nearest.c',
  'libimagequant/pam.c',
  'libimagequant/viter.c'
]

local_c_args = ['-DLIQ_PRIVATE='] # GStreamer already has the right hidden symbol visibility
if cc.has_argument ('-Wno-unknown-pragmas')
  local_c_args += ['-Wno-unknown-pragmas']
endif

gstdvbsubenc = library('gstdvbsubenc',
  subenc_sources + libimagequant_sources,
  c_args : gst_plugins_bad_args + local_c_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gstvideo_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdvbsubenc]
