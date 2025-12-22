decklink_sources = [
  'gstdecklinkplugin.cpp',
  'gstdecklink.cpp',
  'gstdecklinkaudiosink.cpp',
  'gstdecklinkvideosink.cpp',
  'gstdecklinkaudiosrc.cpp',
  'gstdecklinkvideosrc.cpp',
  'gstdecklinkdeviceprovider.cpp',
]

if get_option('decklink').disabled()
  subdir_done()
endif

build_decklink = false
decklink_ldflags = []
decklink_libs = []

if host_system == 'windows'
  decklink_sources += ['win/DeckLinkAPIDispatch.cpp', 'win/DeckLinkAPI_i.c']
  if cxx.get_id() == 'msvc'
    # FIXME: Use commsuppwd.lib for debug builds?
    comutil_dep = cxx.find_library('comsuppw', required : get_option('decklink'))
    if comutil_dep.found()
      build_decklink = true
      decklink_libs = [comutil_dep]
    endif
  else
    build_decklink = true
  endif
else
  libdl = cc.find_library('dl', required: get_option('decklink'))
  have_pthread_h = cdata.has('HAVE_PTHREAD_H')
  if libdl.found() and have_pthread_h
    decklink_libs = [libm, libdl, dependency('threads')]
    if host_system == 'linux'
      decklink_sources += ['linux/DeckLinkAPIDispatch.cpp']
      build_decklink = true
    elif ['darwin', 'ios'].contains(host_system)
      decklink_sources += ['osx/DeckLinkAPIDispatch.cpp']
      decklink_ldflags = ['-Wl,-framework,CoreFoundation']
      build_decklink = true
    endif
  endif
endif

if build_decklink
  message('Building decklink plugin')
  decklink = library('gstdecklink',
    decklink_sources,
    cpp_args : gst_plugins_bad_args + [ '-DGST_USE_UNSTABLE_API' ],
    link_args : decklink_ldflags + noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, gstaudio_dep, gstbase_dep, gst_dep] + decklink_libs,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [decklink]
else
  message('Not building decklink plugin for "@0@'.format(host_machine.system()))
endif
