opensles_sources = [
  'openslescommon.c',
  'openslesringbuffer.c',
  'openslessink.c',
  'openslessrc.c',
  'opensles.c',
  'openslesplugin.c',
]

opensles_option = get_option('opensles')
if opensles_option.disabled()
  subdir_done()
endif

have_opensles_h = cc.has_header('SLES/OpenSLES.h')
if not have_opensles_h and opensles_option.enabled()
  error('opensles plugin enabled but SLES/OpenSLES.h not found')
endif
opensles_dep = cc.find_library('OpenSLES', required : opensles_option)

if have_opensles_h and opensles_dep.found()
  gstopensles = library('gstopensles',
    opensles_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, opensles_dep],
    install : true,
    install_dir : plugins_install_dir)
  plugins += [gstopensles]
endif
