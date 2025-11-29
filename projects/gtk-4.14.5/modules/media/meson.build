media_subdir = 'gtk-4.0/@0@/media'.format(gtk_binary_version)
media_install_dir = join_paths(get_option('libdir'), media_subdir)
media_backends = []

extra_c_args = ['-DGTK_COMPILATION']
extra_c_args += common_cflags

gstplayer_dep = dependency('gstreamer-player-1.0', version: '>= 1.12.3',
                           required: get_option('media-gstreamer'))
gstgl_dep = dependency('gstreamer-gl-1.0', version: '>= 1.12.3',
                       required: get_option('media-gstreamer'))
gstdrm_dep = dependency('gstreamer-allocators-1.0', version: '>= 1.23.1',
                        required: false)

if gstplayer_dep.found() and gstgl_dep.found()
  extra_win_cflags = []

  if host_machine.system() == 'windows' and gstgl_dep.version().version_compare('>=1.19.1')
    message('libgstgl has gst_gl_display_new_with_type()')
    extra_win_cflags += '-DHAVE_GST_GL_DISPLAY_NEW_WITH_TYPE'
  endif

  media_backends += 'gstreamer'
  cdata.set('HAVE_GSTREAMER', 1)
  media_gst_deps = [ libm, libgtk_dep, gstplayer_dep, gstgl_dep ]

  if libdrm_dep.found() and gstdrm_dep.found()
    cdata.set('HAVE_GSTREAMER_DRM', 1)
    media_gst_deps += [ gstdrm_dep ]
  endif

  shared_module('media-gstreamer',
    sources: [
      'gtkgstmediafile.c',
      'gtkgstpaintable.c',
      'gtkgstsink.c',
    ],
    c_args: extra_c_args + extra_win_cflags,
    dependencies: media_gst_deps,
    name_suffix: module_suffix,
    install_dir: media_install_dir,
    install: true,
  )
endif

if media_backends.length() > 0
  gio_module_dirs += media_install_dir
endif
