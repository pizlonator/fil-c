gstgl_dep = dependency('', required : false)
gstglproto_dep = dependency('', required : false)
gstglx11_dep = dependency('', required : false)
gstglwayland_dep = dependency('', required : false)
gstglegl_dep = dependency('', required : false)
gstglviv_fb_dep = dependency('', required : false)

if get_option('gl').disabled()
  message('GStreamer OpenGL integration disabled via options.')
  build_gstgl = false
  subdir_done()
endif

gl_sources = files([
  'gstglapi.c',
  'gstglbasefilter.c',
  'gstglbasememory.c',
  'gstglbasemixer.c',
  'gstglbasesrc.c',
  'gstglcolorconvert.c',
  'gstglbuffer.c',
  'gstglbufferpool.c',
  'gstglcontext.c',
  'gstglcontextconfig.c',
  'gstglcontextquirks.c',
  'gstgldebug.c',
  'gstgldisplay.c',
  'gstglfeature.c',
  'gstglfilter.c',
  'gstglformat.c',
  'gstglframebuffer.c',
  'gstglmemory.c',
  'gstglmemorypbo.c',
  'gstglmixer.c',
  'gstgloverlaycompositor.c',
  'gstglquery.c',
  'gstglrenderbuffer.c',
  'gstglshader.c',
  'gstglshaderstrings.c',
  'gstglsl.c',
  'gstglslstage.c',
  'gstglsyncmeta.c',
  'gstglupload.c',
  'gstglutils.c',
  'gstglviewconvert.c',
  'gstglwindow.c',
])

gl_priv_sources = []

gir_gl_headers = files([
  'gl.h',
  'gl-prelude.h',
  'gstgl_enums.h',
  'gstgl_fwd.h',
  'gstglapi.h',
  'gstglbasefilter.h',
  'gstglbasememory.h',
  'gstglbasemixer.h',
  'gstglbasesrc.h',
  'gstglbuffer.h',
  'gstglbufferpool.h',
  'gstglcolorconvert.h',
  'gstglcontext.h',
  'gstglcontextconfig.h',
  'gstgldebug.h',
  'gstgldisplay.h',
  'gstglfeature.h',
  'gstglfilter.h',
  'gstglformat.h',
  'gstglframebuffer.h',
  'gstglmemory.h',
  'gstglmemorypbo.h',
  'gstglmixer.h',
  'gstgloverlaycompositor.h',
  'gstglquery.h',
  'gstglrenderbuffer.h',
  'gstglshader.h',
  'gstglshaderstrings.h',
  'gstglsl.h',
  'gstglslstage.h',
  'gstglsyncmeta.h',
  'gstglupload.h',
  'gstglutils.h',
  'gstglviewconvert.h',
  'gstglwindow.h',
])

gl_headers = gir_gl_headers + files([
  'gstglfuncs.h',
])

gl_prototype_headers = files([
  'glprototypes/all_functions.h',
  'glprototypes/base.h',
  'glprototypes/buffer_storage.h',
  'glprototypes/blending.h',
  'glprototypes/buffers.h',
  'glprototypes/debug.h',
  'glprototypes/eglimage.h',
  'glprototypes/fbo.h',
  'glprototypes/fixedfunction.h',
  'glprototypes/gles.h',
  'glprototypes/gstgl_compat.h',
  'glprototypes/gstgl_gles2compat.h',
  'glprototypes/opengl.h',
  'glprototypes/query.h',
  'glprototypes/shaders.h',
  'glprototypes/sync.h',
  'glprototypes/vao.h',
])

gl_x11_sources = []
gl_x11_headers = []
gl_wayland_sources = []
gl_wayland_headers = []
gl_egl_sources = []
gl_egl_headers = []
gl_viv_fb_sources = []
gl_viv_fb_headers = []

glconf = configuration_data()
glconf_options = [
    'GST_GL_HAVE_OPENGL',
    'GST_GL_HAVE_GLES2',
    'GST_GL_HAVE_GLES3',
    'GST_GL_HAVE_GLES3EXT3_H',

    'GST_GL_HAVE_WINDOW_X11',
    'GST_GL_HAVE_WINDOW_COCOA',
    'GST_GL_HAVE_WINDOW_WIN32',
    'GST_GL_HAVE_WINDOW_WINRT',
    'GST_GL_HAVE_WINDOW_WAYLAND',
    'GST_GL_HAVE_WINDOW_ANDROID',
    'GST_GL_HAVE_WINDOW_DISPMANX',
    'GST_GL_HAVE_WINDOW_EAGL',
    'GST_GL_HAVE_WINDOW_VIV_FB',
    'GST_GL_HAVE_WINDOW_GBM',

    'GST_GL_HAVE_PLATFORM_EGL',
    'GST_GL_HAVE_PLATFORM_GLX',
    'GST_GL_HAVE_PLATFORM_WGL',
    'GST_GL_HAVE_PLATFORM_CGL',
    'GST_GL_HAVE_PLATFORM_EAGL',

    'GST_GL_HAVE_DMABUF',
    'GST_GL_HAVE_VIV_DIRECTVIV',

    'GST_GL_HAVE_GLEGLIMAGEOES',
    'GST_GL_HAVE_GLCHAR',
    'GST_GL_HAVE_GLSIZEIPTR',
    'GST_GL_HAVE_GLINTPTR',
    'GST_GL_HAVE_GLSYNC',
    'GST_GL_HAVE_GLUINT64',
    'GST_GL_HAVE_GLINT64',
    'GST_GL_HAVE_EGLATTRIB',
    'GST_GL_HAVE_EGLUINT64KHR',
]

foreach option : glconf_options
  glconf.set(option, 0)
endforeach

unneeded_dep = dependency('', required : false)
if unneeded_dep.found()
  error ('Found unfindable dependency')
endif

# OpenGL/GLES2 libraries
gl_lib_deps = []
# GL platform - EGL, GLX, CGL, WGL, etc
gl_platform_deps = []
# GL winsys - wayland, X11, Cocoa, win32, etc
gl_winsys_deps = []
# other things we need.
gl_misc_deps = []
# Other preprocessor arguments
gl_cpp_args = []
gl_includes = []
gl_objc_args = []

enabled_gl_apis = []
enabled_gl_platforms = []
enabled_gl_winsys = []

# parse provided options
libegl_module_name = get_option('egl_module_name')
if libegl_module_name != ''
  gl_cpp_args += ['-DGST_GL_LIBEGL_MODULE_NAME="@0@"'.format(libegl_module_name)]
endif
libgles2_module_name = get_option('gles2_module_name')
if libgles2_module_name != ''
  gl_cpp_args += ['-DGST_GL_LIBGLESV2_MODULE_NAME="@0@"'.format(libgles2_module_name)]
endif
libgl_module_name = get_option('opengl_module_name')
if libgl_module_name != ''
  gl_cpp_args += ['-DGST_GL_LIBGL_MODULE_NAME="@0@"'.format(libgl_module_name)]
endif

gl_apis = get_option('gl_api')
if gl_apis.contains('auto')
  need_api_opengl = 'auto'
  need_api_gles2 = 'auto'
else
  need_api_opengl = 'no'
  need_api_gles2 = 'no'
  foreach api : gl_apis
    if api == 'opengl'
      need_api_opengl = 'yes'
    elif api == 'gles2'
      need_api_gles2 = 'yes'
    else
      error('Unsupported GL api provided ' + api)
    endif
  endforeach
endif

gl_platforms = get_option('gl_platform')
if gl_platforms.contains('auto')
  need_platform_egl = 'auto'
  need_platform_glx = 'auto'
  need_platform_cgl = 'auto'
  need_platform_wgl = 'auto'
  need_platform_eagl = 'auto'
else
  need_platform_egl = 'no'
  need_platform_glx = 'no'
  need_platform_cgl = 'no'
  need_platform_wgl = 'no'
  need_platform_eagl = 'no'
  foreach platform : gl_platforms
    if platform == 'egl'
      need_platform_egl = 'yes'
    elif platform == 'glx'
      need_platform_glx = 'yes'
    elif platform == 'cgl'
      need_platform_cgl = 'yes'
    elif platform == 'wgl'
      need_platform_wgl = 'yes'
    elif platform == 'eagl'
      need_platform_eagl = 'yes'
    else
      error('Unsupported GL platform provided ' + platform)
    endif
  endforeach
endif

gl_winsys = get_option('gl_winsys')
if gl_winsys.contains('auto')
  need_win_x11 = 'auto'
  need_win_wayland = 'auto'
  need_win_win32 = 'auto'
  need_win_winrt = 'auto'
  need_win_cocoa = 'auto'
  need_win_egl = 'auto'
  need_win_surfaceless = 'auto'
  need_win_eagl = 'auto'
  need_win_dispmanx = 'auto'
  need_win_viv_fb = 'auto'
  need_win_gbm = 'auto'
  need_win_android = 'auto'
else
  need_win_x11 = 'no'
  need_win_wayland = 'no'
  need_win_win32 = 'no'
  need_win_winrt = 'no'
  need_win_cocoa = 'no'
  need_win_egl = 'no'
  need_win_surfaceless = 'no'
  need_win_eagl = 'no'
  need_win_dispmanx = 'no'
  need_win_viv_fb = 'no'
  need_win_gbm = 'no'
  need_win_android = 'no'
  foreach winsys : gl_winsys
    if winsys == 'x11'
      need_win_x11 = 'yes'
    elif winsys == 'wayland'
      need_win_wayland = 'yes'
    elif winsys == 'win32'
      need_win_win32 = 'yes'
    elif winsys == 'winrt'
      need_win_winrt = 'yes'
    elif winsys == 'cocoa'
      need_win_cocoa = 'yes'
    elif winsys == 'egl'
      need_win_egl = 'yes'
    elif winsys == 'surfaceless'
      need_win_surfaceless = 'yes'
    elif winsys == 'eagl'
      need_win_eagl = 'yes'
    elif winsys == 'dispmanx'
      need_win_dispmanx = 'yes'
    elif winsys == 'viv-fb'
      need_win_viv_fb = 'yes'
    elif winsys == 'gbm'
      need_win_gbm = 'yes'
    elif winsys == 'android'
      need_win_android = 'yes'
    else
      error('Unsupported GL winsys provided ' + winsys)
    endif
  endforeach
endif

gl_include_header = '''
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
'''

# convoluted way of getting at the subproject taking into account the wrap-mode
# so we don't download a subproject unless allowed
gl_header_dep = dependency('', fallback : ['gl-headers', 'gl_headers_dummy_dep'],
    required : false)
if gl_header_dep.type_name() == 'internal'
  # this will only contain the includes of headers that are not found
  compat_includes = subproject('gl-headers').get_variable('compatibility_includes')
else
  compat_includes = []
endif

# Desktop OpenGL checks
gl_dep = unneeded_dep
glx_dep = unneeded_dep

# (GLVND) OpenGL (without GLX) headers and interface
if need_api_opengl != 'no'
  gl_dep = dependency('opengl', method: 'pkg-config', required : false)
endif

# non-GLVND aka Legacy OpenGL
if need_api_opengl != 'no' and not gl_dep.found()
  if host_system == 'darwin'
    gl_dep = dependency('appleframeworks', modules : ['OpenGL'], required : false)
  else
    # override meson's braindead gl detection on osx/windows/etc by forcing pkg-config
    gl_dep = dependency('gl', method: 'pkg-config', required : false)
  endif

  if not gl_dep.found()
    if host_system == 'windows'
      gl_dep = cc.find_library('opengl32', required : false)
    else
      gl_dep = cc.find_library('GL', required : false)
    endif

    if not cc.has_header('GL/gl.h', include_directories : compat_includes)
      gl_dep = unneeded_dep
    endif

    if not gl_dep.found() and need_api_opengl == 'yes'
      error ('Could not find requested OpenGL library')
    endif

    if gl_dep.found()
      gl_includes += [compat_includes]
    endif
  endif
endif

if need_platform_glx != 'no'
  # (GLVND) GLX library and headers.
  glx_dep = dependency('glx', method: 'pkg-config', required : false)

  if not glx_dep.found()
    if host_system == 'darwin'
      glx_dep = cc.find_library('GL', required : false)
    elif gl_dep.found()
      glx_dep = gl_dep
    elif need_platform_glx == 'yes'
      error ('Could not find requested GLX library')
    endif
  endif
endif

if need_api_opengl != 'no'
  opengl_includes = ''
  if host_system == 'darwin'
    opengl_includes += '''
#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED >= 1014
# define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
# include <OpenGL/gl3.h>
#endif
'''
  else
    opengl_includes += '''
#if _MSC_VER
# include <windows.h>
#endif
#include <GL/gl.h>
#if __WIN32__ || _WIN32
# include <GL/glext.h>
#endif
'''
  endif
endif

bcm_host_dep = unneeded_dep
if need_win_dispmanx != 'no'

  # Try pkg-config for bcm_host then fallback to find_library to also
  # support older distribution
  bcm_host_dep = dependency('bcm_host', required : false)
  if not bcm_host_dep.found()
    bcm_host_dep = cc.find_library('bcm_host', required : false)
  endif
endif

# GLES2 checks
gles2_dep = unneeded_dep
gles3_h = false
gles3ext3_h = false
if need_api_gles2 != 'no'
  if host_system == 'ios'
    gles2_dep = dependency('appleframeworks', modules : ['OpenGLES'], required : false)
  else
    if bcm_host_dep.found()
      gles2_dep = dependency('brcmglesv2', required : false)
    endif
    if not gles2_dep.found()
      gles2_dep = dependency('glesv2', required : false)
    endif
  endif

  if not gles2_dep.found()
#    if host_system == 'windows'
#    elif host_system == ios
#      gles2_dep = cc.find_library('GLESv2', required : false)
#    else
      gles2_dep = cc.find_library('GLESv2', required : false)
#    endif

    if not cc.has_header('GLES2/gl2.h', include_directories : compat_includes)
      gles2_dep = unneeded_dep
    endif

    if not gles2_dep.found() and need_api_gles2 == 'yes'
      error ('Could not find requested OpenGL ES library')
    endif

    if gles2_dep.found()
      gl_includes += [compat_includes]
    endif
  endif

  gles3_h = gles2_dep.found() and cc.has_header('GLES3/gl3.h', dependencies : gles2_dep, include_directories : compat_includes)

  gles_includes = ''
  if host_system == 'ios'
    gles_includes += '''
# include <OpenGLES/ES2/gl.h>
# include <OpenGLES/ES2/glext.h>
'''
  else
    if gles3_h
      gl_includes += [compat_includes]
      gles3ext3_h = gles3_h and cc.has_header('GLES3/gl3ext.h', dependencies : gles2_dep, include_directories : compat_includes)
      gles_includes += '''
# include <GLES3/gl3.h>
# include <GLES2/gl2ext.h>'''
      if gles3ext3_h
        gles_includes += '''
# include <GLES3/gl3ext.h>'''
      endif
    else
      gles_includes += '''
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>'''
    endif
  endif
endif

# can we include both gles2 and opengl headers?
if gles2_dep.found() and gl_dep.found()
  gl_include_block = gl_include_header + gles_includes + opengl_includes
  # TODO: Revert to passing gl_include_block via prefix: once
  # https://github.com/mesonbuild/meson/issues/2364 is fixed
  if not cc.compiles(gl_include_block + '\n' + 'void f (void) {}',dependencies : [gles2_dep, gl_dep], include_directories : compat_includes)
    message ('Cannot include both OpenGL and OpenGL ES headers')
    if need_api_gles2 != 'yes'
      gles2_dep = unneeded_dep
    elif need_api_opengl != 'yes'
      gl_dep = unneeded_dep
    else
      error('Both OpenGL and OpenGL ES were requested but cannot be included together')
    endif
  endif
endif
gl_include_block = gl_include_header
if gles2_dep.found()
  gl_include_block += gles_includes
endif
if gl_dep.found()
  gl_include_block += opengl_includes
endif

if gles2_dep.found()
  gl_lib_deps += gles2_dep
  glconf.set('GST_GL_HAVE_GLES2', 1)
  if gles3_h
    glconf.set('GST_GL_HAVE_GLES3',1)
    if gles3ext3_h
      glconf.set('GST_GL_HAVE_GLES3EXT3_H', 1)
    endif
  endif
  enabled_gl_apis += 'gles2'
endif

if gl_dep.found()
  gl_lib_deps += gl_dep
  glconf.set('GST_GL_HAVE_OPENGL', 1)
  enabled_gl_apis += 'gl'
endif

# EGL checks
egl_dep = unneeded_dep
if need_platform_egl != 'no'
  if bcm_host_dep.found()
    egl_dep = dependency('brcmegl', required : false)
  endif
  if not egl_dep.found()
    egl_dep = dependency('egl', required : false)
  endif
  if not egl_dep.found()
    egl_dep = cc.find_library('EGL', required : false)
  endif

  if egl_dep.found() and not cc.has_header('EGL/egl.h',
                                           dependencies : egl_dep,
                                           include_directories : compat_includes)
    egl_dep = unneeded_dep
  endif
  if egl_dep.found() and not cc.has_header('EGL/eglext.h',
                                           dependencies : egl_dep,
                                           include_directories : compat_includes)
    egl_dep = unneeded_dep
  endif

  if egl_dep.found()
    gl_egl_sources += files([
      'egl/gstegl.c',
      'egl/gsteglimage.c',
      'egl/gstglcontext_egl.c',
      'egl/gstgldisplay_egl.c',
      'egl/gstglmemoryegl.c',
      'egl/gstgldisplay_egl_device.c',
    ])
    gl_egl_headers += files([
      'egl/egl.h',
      'egl/gstegl.h',
      'egl/gsteglimage.h',
      'egl/gstgldisplay_egl.h',
      'egl/gstglmemoryegl.h',
      'egl/gstgldisplay_egl_device.h',
    ])
    gl_platform_deps += egl_dep
    glconf.set('GST_GL_HAVE_PLATFORM_EGL', 1)

    if cc.has_header('libdrm/drm_fourcc.h')
      gl_misc_deps += allocators_dep
      glconf.set('GST_GL_HAVE_DMABUF', 1)
    endif

    egl_includes = '''
#include <EGL/egl.h>
#include <EGL/eglext.h>
'''
    enabled_gl_platforms += 'egl'
  elif need_platform_egl == 'yes'
    error ('Could not find requested EGL library')
  endif
endif

# winsys_egl checks
if need_win_egl != 'no'
  if need_platform_egl == 'no'
    if need_win_egl == 'yes'
      error ('Impossible situation requested: Cannot use Winsys egl without EGL support')
    endif
  elif not egl_dep.found()
    if need_win_egl == 'yes'
      error ('Could not find EGL libraries for Winsys egl')
    else
      message ('Could not find EGL libraries for Winsys egl')
    endif
  else
    enabled_gl_winsys += 'egl'
  endif
endif

# winsys_surfaceless checks
if need_win_surfaceless != 'no'
  if need_platform_egl == 'no'
    if need_win_surfaceless == 'yes'
      error ('Impossible situation requested: Cannot use Winsys surfaceless without EGL support')
    endif
  elif not egl_dep.found()
    if need_win_surfaceless == 'yes'
      error ('Could not find EGL libraries for Winsys surfaceless')
    else
      message ('Could not find EGL libraries for Winsys surfaceless')
    endif
  else
    enabled_gl_winsys += 'surfaceless'
  endif
endif

# wayland checks
wayland_client_dep = unneeded_dep
wayland_cursor_dep = unneeded_dep
wayland_egl_dep = unneeded_dep
if need_win_wayland != 'no'
  if need_win_wayland == 'yes'
    if need_platform_egl == 'no'
      error('Impossible situation requested: Cannot use Wayland without EGL support')
    endif
  endif
  if not egl_dep.found()
    if need_win_wayland == 'yes'
      error ('Could not find EGL libraries for wayland')
    else
      message ('Could not find EGL libraries for wayland')
    endif
  else
    wayland_client_dep = dependency('wayland-client', version : '>= 1.11', required : false)
    wayland_cursor_dep = dependency('wayland-cursor', version : '>= 1.0', required : false)
    wayland_egl_dep = dependency('wayland-egl', version : '>= 1.0', required : false)
    wayland_protocols_dep = dependency('wayland-protocols', version : '>= 1.15', required : false)
    wayland_scanner = find_program('wayland-scanner', required: false)

    if wayland_client_dep.found() and wayland_cursor_dep.found() and wayland_egl_dep.found() and wayland_protocols_dep.found() and wayland_scanner.found()
      # Generate the XDG shell interface
      wayland_protocols_basedir = wayland_protocols_dep.get_variable('pkgdatadir')
      xdg_shell_xml_spec = join_paths(wayland_protocols_basedir, 'stable', 'xdg-shell', 'xdg-shell.xml')
      xdg_shell_header = custom_target('xdg-shell-client-header',
        command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ],
        input: xdg_shell_xml_spec,
        output: 'xdg-shell-client-protocol.h',
      )
      xdg_shell_code = custom_target('xdg-shell-client-code',
        command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ],
        input: xdg_shell_xml_spec,
        output: 'xdg-shell-client-protocol.c',
      )

      gl_wayland_sources += files([
        'wayland/gstgldisplay_wayland.c',
      ])
      gl_priv_sources += [
        'wayland/gstglwindow_wayland_egl.c',
        'wayland/wayland_event_source.c',
        xdg_shell_header,
        xdg_shell_code,
      ]
      gl_wayland_headers += files([
        'wayland/wayland.h',
        'wayland/gstgldisplay_wayland.h'
      ])
      glconf.set('GST_GL_HAVE_WINDOW_WAYLAND', 1)
      gl_winsys_deps += [wayland_client_dep, wayland_cursor_dep, wayland_egl_dep]
      enabled_gl_winsys += 'wayland'
    else
      if need_win_wayland == 'yes'
        error ('Could not find requested Wayland libraries')
      endif
      wayland_client_dep = unneeded_dep
      wayland_cursor_dep = unneeded_dep
      wayland_egl_dep = unneeded_dep
      wayland_protocols_dep = unneeded_dep
    endif
  endif
endif

# X11 checks
if need_platform_glx == 'yes'
  if need_win_x11 == 'no'
    error('Impossible situation requested: Cannot use GLX without X11 support')
  elif need_api_opengl == 'no'
    error('Impossible situation requested: Cannot use GLX without the OpenGL library')
  endif
endif

xcb_dep = unneeded_dep
if need_win_x11 != 'no'
  xcb_dep = dependency('x11-xcb', required : false)
  if x11_dep.found() and xcb_dep.found()
    gl_x11_sources += files([
      'x11/gstgldisplay_x11.c',
    ])
    gl_priv_sources += [
      'x11/gstglwindow_x11.c',
      'x11/xcb_event_source.c',
    ]
    gl_x11_headers += files([
      'x11/x11.h',
      'x11/gstgldisplay_x11.h',
    ])
    glconf.set('GST_GL_HAVE_WINDOW_X11', 1)
    gl_winsys_deps += [x11_dep, xcb_dep]
    enabled_gl_winsys += 'x11'

    if need_platform_glx != 'no' and glx_dep.found() and cc.has_function ('glXMakeCurrent', dependencies : glx_dep)
      glconf.set('GST_GL_HAVE_PLATFORM_GLX', 1)
      gl_priv_sources += [
        'x11/gstglcontext_glx.c',
      ]
      # GLX is in the opengl library on linux
      gl_platform_deps += glx_dep
      enabled_gl_platforms += 'glx'
    endif
  elif need_win_x11 == 'yes'
    error ('Could not find requested X11 libraries')
  endif
endif

if need_win_dispmanx != 'no'
  have_dispmanx_window_t = cc.has_type(
      'EGL_DISPMANX_WINDOW_T',
      prefix: '''
#include <bcm_host.h>
#include <EGL/egl.h>
''',
      dependencies: [gl_lib_deps, egl_dep, bcm_host_dep],
      include_directories: gl_includes)

  if have_dispmanx_window_t and bcm_host_dep.found()
    if not egl_dep.found()
      error('dispmanx requires the use of egl')
    endif

    gl_priv_sources += [
      'dispmanx/gstglwindow_dispmanx_egl.c'
    ]

    glconf.set('GST_GL_HAVE_WINDOW_DISPMANX', 1)
    gl_winsys_deps += bcm_host_dep
    enabled_gl_winsys += 'dispmanx'
    gl_cpp_args += ['-DUSE_EGL_RPI']
  elif need_win_dispmanx == 'yes'
    error('Could not find dispmanx libraries')
  endif
endif

# win32 checks
if need_platform_wgl == 'yes'
  if need_win_win32 == 'no'
    error('Impossible situation requested: Cannot use WGL without the win32 window system')
  endif
endif

if host_system == 'windows' and need_win_win32 != 'no'
  gdi_dep = cc.find_library('gdi32', required : false)
  if gdi_dep.found()
    have_wgl = false
    have_egl_win32 = false
    if need_platform_wgl != 'no'
      wglext_h = cc.has_header('GL/wglext.h',
                               prefix : '''#include <windows.h>
                                           #include <GL/gl.h>''',
                               include_directories : compat_includes)
      have_wgl = wglext_h and gl_dep.found()
    endif

    have_egl_win32 = enabled_gl_platforms.contains('egl') and gles2_dep.found()
    if have_wgl or have_egl_win32
      gl_includes += [compat_includes]
      gl_platform_deps += gdi_dep
      gl_priv_sources += [
        'win32/gstglwindow_win32.c',
      ]
      enabled_gl_winsys += 'win32'
      gl_winsys_deps += gdi_dep
      glconf.set('GST_GL_HAVE_WINDOW_WIN32', 1)
    endif

    if have_wgl
      gl_priv_sources += [
        'wgl/gstglcontext_wgl.c',
      ]
      enabled_gl_platforms += 'wgl'
      glconf.set('GST_GL_HAVE_PLATFORM_WGL', 1)
    endif
  endif
endif

# WinRT ANGLE checks
if need_win_winrt != 'no' and host_system == 'windows'
  if need_win_winrt == 'yes' and not enabled_gl_platforms.contains('egl')
    error('Impossible situation requested: Cannot use WinRT ANGLE without EGL support')
  endif

  if egl_dep.found()
    windows_graphics_h = cc.has_header('windows.graphics.h')
    windows_app_dep = cc.find_library('WindowsApp', required: false)
    runtimeobject_lib = cc.find_library('runtimeobject', required: false)

    if windows_graphics_h and windows_app_dep.found() and runtimeobject_lib.found()
      enabled_gl_winsys += 'winrt'
      glconf.set('GST_GL_HAVE_WINDOW_WINRT', 1)
      gl_priv_sources += [
        'winrt/gstglwindow_winrt_egl.cpp'
      ]
      gl_winsys_deps += runtimeobject_lib
    elif need_win_winrt == 'yes'
      error('WinRT is enabled, but headers/libraries were not found')
    endif
  elif need_win_winrt == 'yes'
    error('WinRT ANGLE is enabled, but EGL was not found')
  else
    message('WinRT ANGLE disabled because EGL was not found')
  endif
endif

if ['darwin', 'ios'].contains(host_system)
  if not have_objc
    error('No ObjC compiler found')
  endif

  objc = meson.get_compiler('objc')
  if not objc.has_argument('-fobjc-arc')
    error('ARC is required for building')
  endif

  gl_objc_args += ['-fobjc-arc']
endif

# OSX check
if need_platform_cgl == 'yes'
  if need_win_cocoa == 'no'
    error('Impossible situation requested: Cannot use CGL without Cocoa support')
  elif need_api_opengl == 'no'
    error('Impossible situation requested: Cannot use CGL without the OpenGL library')
  endif
elif need_platform_cgl == 'no' and need_win_cocoa == 'yes'
  error('Impossible situation requested: Cannot use Cocoa without CGL support')
endif

if host_system == 'darwin'
  foundation_dep = dependency('appleframeworks', modules : ['Foundation'], required : false)
  quartzcore_dep = dependency('appleframeworks', modules : ['QuartzCore'], required : false)
  corefoundation_dep = dependency('appleframeworks', modules : ['CoreFoundation'], required : false)

  if need_platform_cgl != 'no'
    if foundation_dep.found() and quartzcore_dep.found() and corefoundation_dep.found()
      gl_platform_deps += [quartzcore_dep, corefoundation_dep, foundation_dep]
      enabled_gl_platforms += 'cgl'
      glconf.set('GST_GL_HAVE_PLATFORM_CGL', 1)

      if need_win_cocoa != 'no'
        cocoa_dep = dependency('appleframeworks', modules : ['Cocoa'], required : false)
        if cocoa_dep.found()
          gl_priv_sources += [
            'cocoa/gstglcaopengllayer.m',
            'cocoa/gstglcontext_cocoa.m',
            'cocoa/gstgldisplay_cocoa.m',
            'cocoa/gstglwindow_cocoa.m'
          ]
          gl_winsys_deps += cocoa_dep
          enabled_gl_winsys += 'cocoa'
          glconf.set('GST_GL_HAVE_WINDOW_COCOA', 1)
        elif need_win_cocoa == 'yes'
          error('Could not find Cocoa')
        endif
      endif
    elif need_platform_cgl == 'yes'
      error('Could not find CGL dependencies')
    endif
  endif
endif

if need_platform_eagl == 'yes'
  if host_system != 'ios'
    error('Can only use EAGL on iOS')
  endif
  if need_win_eagl == 'no'
    error('Impossible situation requested: Cannot use EAGL platform without the EAGL window system')
  elif need_api_gles == 'no'
    error('Impossible situation requested: Cannot use EAGL platform without the GLES library')
  endif
elif need_platform_eagl == 'no' and need_win_eagl == 'yes'
  error('Impossible situation requested: Cannot use EAGL window system without the EAGL platform')
endif

if host_system == 'ios' and need_platform_eagl != 'no' and need_win_eagl != 'no'
  foundation_dep = dependency('appleframeworks', modules : ['Foundation'], required : false)
  corefoundation_dep = dependency('appleframeworks', modules : ['CoreFoundation'], required : false)
  coregraphics_dep = dependency('appleframeworks', modules : ['CoreGraphics'], required : false)
  quartzcore_dep = dependency('appleframeworks', modules : ['QuartzCore'], required : false)
  uikit_dep = dependency('appleframeworks', modules : ['UIkit'], required : false)

  if foundation_dep.found() and corefoundation_dep.found() and coregraphics_dep.found() and quartzcore_dep.found() and uikit_dep.found()
    gl_platform_deps += [
      corefoundation_dep,
      foundation_dep,
      coregraphics_dep,
      quartzcore_dep,
      uikit_dep,
    ]
    gl_priv_sources += [
      'eagl/gstglcontext_eagl.m',
      'eagl/gstglwindow_eagl.m',
    ]
    enabled_gl_winsys += 'eagl'
    enabled_gl_platforms += 'eagl'
    glconf.set('GST_GL_HAVE_WINDOW_EAGL', 1)
    glconf.set('GST_GL_HAVE_PLATFORM_EAGL', 1)
  elif need_platform_eagl == 'yes' or need_win_eagl == 'yes'
    error('Could not find dependencies for EAGL')
  endif
endif

# GBM Checks
gbm_gudev_dep = unneeded_dep
gbm_libdrm_dep = unneeded_dep
gbm_dep = unneeded_dep
if need_win_gbm != 'no'
  if need_win_gbm == 'yes'
    if need_platform_egl == 'no'
      error('Impossible situation requested: Cannot use GBM without EGL support')
    endif
  endif

  gbm_gudev_dep = dependency('gudev-1.0', version : '>=147', required : false, allow_fallback: true)
  gbm_libdrm_dep = dependency('libdrm', version : '>= 2.4.55', required : false)
  gbm_dep = dependency('gbm', required : false)
  if egl_dep.found() and gbm_gudev_dep.found() and gbm_libdrm_dep.found() and gbm_dep.found()
    gl_priv_sources += [
      'gbm/gstgldisplay_gbm.c',
      'gbm/gstgl_gbm_utils.c',
      'gbm/gstglwindow_gbm_egl.c',
    ]
    enabled_gl_winsys += 'gbm'
    gl_winsys_deps += [gbm_gudev_dep, gbm_libdrm_dep, gbm_dep]
    glconf.set('GST_GL_HAVE_WINDOW_GBM', 1)
  else
    if need_win_gbm == 'yes'
      error ('Could not find requested GBM libraries')
    endif
    gbm_gudev_dep = unneeded_dep
    gbm_libdrm_dep = unneeded_dep
    gbm_dep = unneeded_dep
  endif
endif

if cc.has_function ('glTexDirectVIV', dependencies : gles2_dep)
  glconf.set('GST_GL_HAVE_VIV_DIRECTVIV', 1)
endif

if need_platform_egl != 'no' and need_win_viv_fb != 'no'
  if egl_dep.found() and cc.has_function ('fbGetDisplay', dependencies : egl_dep)
    enabled_gl_winsys += 'viv-fb'
    glconf.set('GST_GL_HAVE_WINDOW_VIV_FB', 1)
    gl_priv_sources += files([
      'viv-fb/gstgldisplay_viv_fb.c',
      'viv-fb/gstglwindow_viv_fb_egl.c',
    ])
    gl_viv_fb_sources += files([
      'viv-fb/gstgldisplay_viv_fb.c',
    ])

    gl_cpp_args += ['-DEGL_API_FB']
    gl_viv_fb_headers = files([
      'viv-fb/gstglviv-fb.h',
      'viv-fb/gstgldisplay_viv_fb.h',
    ])
  endif
endif

if need_win_android == 'yes'
  if need_platform_egl == 'no'
    error('Impossible situation requested: Cannot build for Android without EGL')
  elif need_api_gles2 == 'no'
    error('Impossible situation requested: Cannot build for Android without GLES2 support')
  elif host_system != 'android'
    error('Impossible situation requested: Cannot build for Android without an android system')
  endif
endif

if host_system == 'android' and need_win_android != 'no' and need_platform_egl != 'no'
  if gles2_dep.found() and egl_dep.found()
    enabled_gl_winsys += ['android']
    glconf.set('GST_GL_HAVE_WINDOW_ANDROID', 1)
    gl_priv_sources += [
      'android/gstglwindow_android_egl.c'
    ]
  endif
endif

gl_nvmm = false
nvbuf_utils_dep = unneeded_dep
if egl_dep.found()
  # XXX: provide options for this?
  # c_args and c_link_args can also cover this case just fine e.g.:
  # -Dc_args='-I/usr/src/jetson_multimedia_api/include' -Dc_link_args='-L/usr/lib/aarch64-linux-gnu/tegra/'
  nvbuf_utils_dep = cc.find_library('nvbuf_utils', required : false)
  nvbuf_utils_h = cc.has_header('nvbuf_utils.h')
  if nvbuf_utils_dep.found() and nvbuf_utils_h
    gl_misc_deps += [nvbuf_utils_dep]
    gl_nvmm = true
    gl_cpp_args += ['-DHAVE_NVMM']
  endif
endif

build_gstgl = true
if enabled_gl_apis.length() == 0
  message('No OpenGL API libraries found or requested')
  build_gstgl = false
endif
if enabled_gl_platforms.length() == 0
  message('No OpenGL Platforms found or requested')
  build_gstgl = false
endif
if enabled_gl_winsys.length() == 0
  message('No OpenGL Window systems found or requested')
  build_gstgl = false
endif

if build_gstgl
  # find some types that may or may not be defined
  if cc.has_type('GLeglImageOES', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLEGLIMAGEOES', 1)
  endif
  if cc.has_type('GLchar', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLCHAR', 1)
  endif
  if cc.has_type('GLsizeiptr', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLSIZEIPTR', 1)
  endif
  if cc.has_type('GLintptr', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLINTPTR', 1)
  endif
  if cc.has_type('GLsync', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLSYNC', 1)
  endif
  if cc.has_type('GLuint64', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLUINT64', 1)
  endif
  if cc.has_type('GLint64', prefix : gl_include_block, dependencies : gl_lib_deps, include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_GLINT64', 1)
  endif
  if egl_dep.found() and cc.has_type('EGLAttrib', prefix : gl_include_block + egl_includes, dependencies : gl_lib_deps + [egl_dep], include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_EGLATTRIB', 1)
  endif
  if egl_dep.found() and cc.has_type('EGLuint64KHR', prefix : gl_include_block + egl_includes, dependencies : gl_lib_deps + [egl_dep], include_directories : gl_includes)
    glconf.set('GST_GL_HAVE_EGLUINT64KHR', 1)
  endif

  summary('GL api', enabled_gl_apis, list_sep: ', ')
  summary('GL platform', enabled_gl_platforms, list_sep: ', ')
  summary('GL winsys', enabled_gl_winsys, list_sep: ', ')

  install_headers(gl_headers, subdir : 'gstreamer-1.0/gst/gl')
  install_headers(gl_prototype_headers, subdir : 'gstreamer-1.0/gst/gl/glprototypes')

  configure_file(input : 'gstglconfig.h.meson',
    output : 'gstglconfig.h',
    install_dir : get_option('libdir') + '/gstreamer-1.0/include/gst/gl',
    install_tag : 'devel',
    configuration : glconf)

  glib_mkenums = find_program('glib-mkenums')
  mkenums = find_program('gl_mkenum.py')
  gl_enumtypes_h = custom_target('gstglenumtypes_h',
    output : 'gl-enumtypes.h',
    input : gir_gl_headers,
    install : true,
    install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/gl/'),
    command : [mkenums, glib_mkenums, '@OUTPUT@', '@INPUT@'])

  gl_enumtypes_c = custom_target('gstglenumtypes_c',
    output : 'gl-enumtypes.c',
    input : gir_gl_headers,
    depends : [gl_enumtypes_h],
    command : [mkenums, glib_mkenums, '@OUTPUT@', '@INPUT@'])
  gen_sources = [gl_enumtypes_h]

  common_args = gst_plugins_base_args + gl_cpp_args + ['-DBUILDING_GST_GL', '-DG_LOG_DOMAIN="GStreamer-GL"']

  # We have custom_target() that generate headers in the current build dir,
  # but with implicit_include_directories: false, meson >= 0.58.0 won't include
  # it by default. We cannot use include_directories('.') here because it would
  # also include current source dir which is what we want to avoid because
  # case-insensitive FS would include gst-libs/gl/egl/egl.h as EGL/egl.h.
  common_args += '-I@0@'.format(meson.current_build_dir())

  gstgl = library('gstgl-' + api_version,
    gl_sources, gl_egl_sources, gl_x11_sources, gl_wayland_sources, gl_viv_fb_sources, gl_priv_sources, gl_enumtypes_c, gl_enumtypes_h,
    c_args : common_args,
    cpp_args : common_args,
    objc_args : common_args + gl_objc_args,
    include_directories : [configinc, libsinc, gl_includes],
    version : libversion,
    soversion : soversion,
    darwin_versions : osxversion,
    install : true,
    dependencies : [gst_base_dep, video_dep, allocators_dep, gmodule_dep,
                    gl_lib_deps, gl_platform_deps, gl_winsys_deps, gl_misc_deps],
    # don't confuse EGL/egl.h with gst-libs/gl/egl/egl.h on case-insensitive file systems
    implicit_include_directories : false)

  pkgconfig_gl_variables = [
    'gl_platforms=' + ' '.join(enabled_gl_platforms),
    'gl_winsys=' + ' '.join(enabled_gl_winsys),
    'gl_apis=' + ' '.join(enabled_gl_apis),
  ]
  pkgconfig_installed_variables = [
    'libinc=-I${libdir}/gstreamer-1.0/include',
  ]

  library_def = {'lib': gstgl}
  pkg_name = 'gstreamer-gl-1.0'
  pkgconfig.generate(gstgl,
    libraries : [gstvideo, gst_base_dep, gst_dep],
    variables : pkgconfig_variables + pkgconfig_gl_variables + pkgconfig_installed_variables,
    uninstalled_variables : pkgconfig_gl_variables,
    subdirs : pkgconfig_subdirs,
    extra_cflags : ['${libinc}'],
    name : pkg_name,
    description : 'Streaming media framework, OpenGL plugins libraries',
  )

  # Desperate times, desperate measures... fix up escaping of our variables
  run_command(meson_pkg_config_file_fixup_script,
    'gstreamer-gl-1.0', 'gl_platforms', 'gl_winsys', 'gl_apis',
    check: true)

  pkgconfig.generate(
    libraries : [gstgl, gl_lib_deps],
    subdirs : pkgconfig_subdirs,
    name : 'gstreamer-gl-prototypes-1.0',
    description : 'Streaming media framework, OpenGL plugins libraries (OpenGL Prototypes)',
  )

  if build_gir
    gir = {
        'sources' : gl_sources + [gl_enumtypes_h] + [gl_enumtypes_c] + gir_gl_headers,
        'namespace' : 'GstGL',
        'nsversion' : api_version,
        'identifier_prefix' : 'Gst',
        'symbol_prefix' : 'gst',
        'export_packages' : pkg_name,
        'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
        'install' : true,
        'extra_args' : gir_init_section + ['--c-include=gst/gl/gl.h'],
        'dependencies' : [video_dep, gst_dep, gst_base_dep],
    }
    library_def += {'gir': [gir]}
    if not static_build
      gl_gir = gnome.generate_gir(gstgl, kwargs: gir)
      library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gl_gir]}
      library_def += {'gir': [gir]}
      gen_sources += [gl_gir]
    endif
  endif
  gst_libraries += [[pkg_name, library_def]]
  gstgl_dep = declare_dependency(link_with : gstgl,
    include_directories : [libsinc, compat_includes],
    sources: gen_sources,
    dependencies : [video_dep, gst_base_dep, gl_winsys_deps],
    variables: pkgconfig_gl_variables)
  gstglproto_dep = declare_dependency(
    dependencies : [gstgl_dep] + gl_lib_deps,
    sources: gen_sources
  )
  meson.override_dependency('gstreamer-gl-1.0', gstgl_dep)
  meson.override_dependency('gstreamer-gl-prototypes-1.0', gstglproto_dep)

  if gl_x11_headers.length() > 0
    install_headers(gl_x11_headers, subdir : 'gstreamer-1.0/gst/gl/x11')
    pkg_name = 'gstreamer-gl-x11-1.0'
    pkgconfig.generate(
      libraries : [gstgl],
      requires: ['x11-xcb'],
      subdirs : pkgconfig_subdirs,
      name : pkg_name,
      description : 'Streaming media framework, OpenGL plugins libraries (X11 specifics)',
    )
    gl_x11_gir = []
    if build_gir
      gir = {
        'sources' : gl_x11_sources + gl_x11_headers,
        'namespace' : 'GstGLX11',
        'nsversion' : api_version,
        'identifier_prefix' : 'Gst',
        'symbol_prefix' : 'gst',
        'export_packages' : 'gstreamer-gl-x11-1.0',
        'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
        'install' : true,
        'extra_args' : gir_init_section + ['--c-include=gst/gl/x11/x11.h'],
        'dependencies' : [video_dep, gst_dep, gst_base_dep]
      }
      gir_dict = gir + {'includes': gir['includes'] + ['GstGL-1.0']}
      gst_libraries += [[pkg_name, {'gir': gir_dict}]]
      if not static_build
        gir += {'includes': gir['includes'] + [gl_gir[0]]}
        gl_x11_gir = gnome.generate_gir(gstgl, kwargs: gir)
        library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gl_x11_gir]}
      endif
    endif
    gstglx11_dep = declare_dependency(dependencies : [gstgl_dep, x11_dep, xcb_dep],
      sources : gl_x11_gir)
    meson.override_dependency('gstreamer-gl-x11-1.0', gstglx11_dep)
  endif

  if gl_wayland_headers.length() > 0
    install_headers(gl_wayland_headers, subdir : 'gstreamer-1.0/gst/gl/wayland')
    pkg_name = 'gstreamer-gl-wayland-1.0'
    pkgconfig.generate(
      libraries : [gstgl],
      requires: ['wayland-egl', 'wayland-client'],
      subdirs : pkgconfig_subdirs,
      name : pkg_name,
      description : 'Streaming media framework, OpenGL plugins libraries (Wayland specifics)',
    )
    gl_wayland_gir = []
    if build_gir
      gir = {
        'sources' : gl_wayland_sources + gl_wayland_headers,
        'namespace' : 'GstGLWayland',
        'nsversion' : api_version,
        'identifier_prefix' : 'Gst',
        'symbol_prefix' : 'gst',
        'export_packages' : 'gstreamer-gl-wayland-1.0',
        'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
        'install' : true,
        'extra_args' : gir_init_section + ['--c-include=gst/gl/wayland/wayland.h'],
        'dependencies' : [video_dep, gst_dep, gst_base_dep]
      }

      gir_dict = gir + {'includes': gir['includes'] + ['GstGL-1.0']}
      gst_libraries += [[pkg_name, {'gir': gir_dict}]]
      if not static_build
        gir += {'includes': gir['includes'] + [gl_gir[0]]}
        gl_wayland_gir = gnome.generate_gir(gstgl, kwargs: gir)
        library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gl_wayland_gir]}
      endif
    endif
    gstglwayland_dep = declare_dependency(dependencies : [gstgl_dep],
      sources : gl_wayland_gir)
    meson.override_dependency(pkg_name, gstglwayland_dep)
  endif

  if gl_egl_headers.length() > 0
    install_headers(gl_egl_headers, subdir : 'gstreamer-1.0/gst/gl/egl')
    pkg_name = 'gstreamer-gl-egl-1.0'
    pkgconfig.generate(
      libraries : [gstgl, egl_dep],
      subdirs : pkgconfig_subdirs,
      name : pkg_name,
      description : 'Streaming media framework, OpenGL plugins libraries (EGL specifics)',
    )
    gl_egl_gir = []
    if build_gir
      gir = {
        'sources' : gl_egl_sources + gl_egl_headers,
        'namespace' : 'GstGLEGL',
        'nsversion' : api_version,
        'identifier_prefix' : 'Gst',
        'symbol_prefix' : 'gst',
        'export_packages' : pkg_name,
        'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
        'install' : true,
        'extra_args' : gir_init_section + ['--c-include=gst/gl/egl/egl.h'],
        'dependencies' : [video_dep, gst_dep, gst_base_dep]
      }
      gir_dict = gir + {'includes': gir['includes'] + ['GstGL-1.0']}
      gst_libraries += [[pkg_name, {'gir': gir_dict}]]
      if not static_build
        gir += {'includes': gir['includes'] + [gl_gir[0]]}
        gl_egl_gir = gnome.generate_gir(gstgl, kwargs: gir)
        library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gl_egl_gir]}
      endif
    endif
    gstglegl_dep = declare_dependency(dependencies : [gstgl_dep],
      sources : gl_egl_gir)
    meson.override_dependency(pkg_name, gstglegl_dep)
  endif

  if gl_viv_fb_headers.length() > 0
    install_headers(gl_viv_fb_headers, subdir : 'gstreamer-1.0/gst/gl/viv-fb')
    pkg_name = 'gstreamer-gl-viv-fb-1.0'
    pkgconfig.generate(
      libraries : [gstgl],
      subdirs : pkgconfig_subdirs,
      name : pkg_name,
      description : 'Streaming media framework, OpenGL plugins libraries (Vivante Framebuffer specifics)',
    )
    gl_viv_fb_gir = []
    if build_gir
      gir = {
        'sources' : gl_viv_fb_sources + gl_viv_fb_headers,
        'namespace' : 'GstGLVivFB',
        'nsversion' : api_version,
        'identifier_prefix' : 'Gst',
        'symbol_prefix' : 'gst',
        'export_packages' : pkg_name,
        'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0'],
        'install' : true,
        'extra_args' : gir_init_section + ['--c-include=gst/gl/viv-fb/gstglviv-fb.h'],
        'dependencies' : [video_dep, gst_dep, gst_base_dep]
      }
      gir_dict = gir + {'includes': gir['includes'] + ['GstGL-1.0']}
      gst_libraries += [[pkg_name, {'gir': gir_dict}]]
      if not static_build
        gir += {'includes': gir['includes'] + [gl_gir[0]]}
        gl_viv_fb_gir = gnome.generate_gir(gstgl, kwargs: gir)
        library_def += {'gir_targets':  library_def.get('gir_targets', []) + [gl_viv_fb_gir]}
      endif
    endif
    gstglviv_fb_dep = declare_dependency(dependencies : [gstgl_dep],
      sources : gl_viv_fb_gir)
    meson.override_dependency(pkg_name, gstglviv_fb_dep)
  endif

elif get_option('gl').enabled()
  error('GStreamer OpenGL integration required via options, but needed dependencies not found.')
endif
