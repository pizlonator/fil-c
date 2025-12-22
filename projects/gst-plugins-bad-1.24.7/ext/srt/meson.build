srt_sources = [
  'gstsrt.c',
  'gstsrtelement.c',
  'gstsrtplugin.c',
  'gstsrtobject.c',
  'gstsrtsink.c',
  'gstsrtsrc.c'
]
srt_option = get_option('srt')
if srt_option.disabled()
  srt_dep = dependency('', required : false)
  subdir_done()
endif

srt_dep = dependency('srt', version : '>=1.3.0', required : false)
if not srt_dep.found() and cc.has_header_symbol('srt/srt.h', 'srt_startup')
  srt_dep = cc.find_library('srt', required : false)
endif
if not srt_dep.found() and srt_option.enabled()
  error('srt plugin enabled, but srt library not found')
endif

if srt_dep.found()
  gstsrt_enums = gnome.mkenums_simple('gstsrt-enumtypes',
    sources: ['gstsrt-enums.h'],
    decorator : 'G_GNUC_INTERNAL',
    install_header: false)

  gstsrt = library('gstsrt',
    srt_sources, gstsrt_enums,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstbase_dep, gio_dep, srt_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstsrt]
endif
