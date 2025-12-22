shm_sources = [
  'shmpipe.c',
  'shmalloc.c',
  'gstshm.c',
  'gstshmsrc.c',
  'gstshmsink.c',
]

shm_deps = []
shm_enabled = false
if get_option('shm').disabled()
  subdir_done()
endif

# NetBSD has shm_* in librt
if ['darwin', 'ios', 'freebsd', 'openbsd'].contains(host_system)
  rt_dep = []
  shm_enabled = true
else
  rt_dep = cc.find_library ('rt', required: false)
  shm_enabled = rt_dep.found()
endif

if shm_enabled
  shm_enabled = cc.has_header('sys/socket.h')
elif get_option('shm').enabled()
  error('shm plugin enabled but librt not found')
endif

if not shm_enabled and get_option('shm').enabled()
  error('shm plugin enabled but socket.h not found')
endif

if shm_enabled
  gstshm = library('gstshm',
    shm_sources,
    c_args : gst_plugins_bad_args + ['-DSHM_PIPE_USE_GLIB'],
    include_directories : [configinc],
    dependencies : [gstbase_dep, rt_dep] + network_deps,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstshm]
endif
