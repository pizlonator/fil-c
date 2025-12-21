sdl_dep = dependency('sdl2', version : '>=2.0', required : get_option('examples'))
have_sdl_gl = build_gstgl and (
    enabled_gl_apis.contains('gl') and
    (enabled_gl_platforms.contains('glx') or enabled_gl_platforms.contains('wgl')) and
    (enabled_gl_winsys.contains('x11') or enabled_gl_winsys.contains('win32'))
)
if sdl_dep.found() and have_sdl_gl
  executable('sdlshare',
    'sdlshare.c',
    install: false,
    dependencies : [gst_base_dep, gstgl_dep, sdl_dep, gl_dep, glx_dep])

  executable('sdlshare2',
    'sdlshare2.c',
    install: false,
    dependencies : [gst_base_dep, gstgl_dep, sdl_dep, gl_dep, glx_dep])
endif
