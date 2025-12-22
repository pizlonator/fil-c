fbdevsink_sources = [
  'gstfbdevsink.c',
]

if get_option('fbdev').disabled()
  subdir_done()
endif

have_fb_h = cc.has_header('linux/fb.h')
if not have_fb_h and get_option('fbdev').enabled()
  error('fbdev plugin enabled but fb.h not found')
endif

if have_fb_h
  gstfbdevsink = library('gstfbdevsink',
    fbdevsink_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstfbdevsink]
endif
