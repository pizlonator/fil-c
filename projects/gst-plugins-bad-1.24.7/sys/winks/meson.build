winks_sources = [
  'gstksclock.c',
  'gstksvideodevice.c',
  'gstksvideosrc.c',
  'ksdeviceprovider.c',
  'kshelpers.c',
  'ksvideohelpers.c',
]

winks_option = get_option('winks')
if host_system == 'windows' and not winks_option.disabled()
  # FIXME: Add a way to skip this library when these deps are not found. Not
  # urgent because these are always available when building for Windows.
  winks_dep = [cc.find_library('ksuser', required : winks_option),
               cc.find_library('uuid', required : winks_option),
               cc.find_library('strmiids', required : winks_option),
               cc.find_library('dxguid', required : winks_option),
               cc.find_library('setupapi', required : winks_option),
               cc.find_library('ole32', required : winks_option)]

  gstwinks = library('gstwinks',
    winks_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep] + winks_dep,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstwinks]
endif
