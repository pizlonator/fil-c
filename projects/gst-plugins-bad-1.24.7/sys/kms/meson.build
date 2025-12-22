kmssink_sources = [
  'gstkmsallocator.c',
  'gstkmsbufferpool.c',
  'gstkmssink.c',
  'gstkmsutils.c',
]
extra_deps = []

if host_system != 'linux'
  subdir_done()
endif

libdrm_dep = dependency('libdrm', version : '>= 2.4.98',
                        required : get_option('kms'))
libdrm_hdr_dep = dependency('libdrm', version : '>= 2.4.104',
                            required : false)
mathlib = cc.find_library('m', required : false)

if libdrm_hdr_dep.found() and mathlib.found()
  cdata.set('HAVE_DRM_HDR', 1)
  kmssink_sources += 'gstkmsedid.c'
endif

if libdrm_dep.found()
  gstkmssink = library('gstkms',
    kmssink_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep, libdrm_dep, mathlib],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstkmssink]
endif
