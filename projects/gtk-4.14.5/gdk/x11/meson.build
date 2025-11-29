# A list of sources that contain public API; these are the sources that
# will be introspected for the GdkX11 namespace.
#
# Note: gdksettings.c gets included from xsettings-client.c, do not add here
gdk_x11_public_sources = files([
  'gdkcursor-x11.c',
  'gdkdevice-xi2.c',
  'gdkdevicemanager-x11.c',
  'gdkdevicemanager-xi2.c',
  'gdkdisplay-x11.c',
  'gdkglcontext-egl.c',
  'gdkglcontext-glx.c',
  'gdkglcontext-x11.c',
  'gdkkeys-x11.c',
  'gdkmonitor-x11.c',
  'gdkproperty-x11.c',
  'gdkscreen-x11.c',
  'gdkselection-x11.c',
  'gdksurface-x11.c',
  'gdkxid.c',
  'xsettings-client.c',
])

# All sources
gdk_x11_sources = gdk_x11_public_sources + [
  'gdkapplaunchcontext-x11.c',
  'gdkasync.c',
  'gdkcairocontext-x11.c',
  'gdkclipboard-x11.c',
  'gdkdrag-x11.c',
  'gdkdrop-x11.c',
  'gdkeventsource.c',
  'gdkeventtranslator.c',
  'gdkmain-x11.c',
  'gdkselectioninputstream-x11.c',
  'gdkselectionoutputstream-x11.c',
  'gdktextlistconverter-x11.c',
  'gdkvulkancontext-x11.c',
  'gdkxftdefaults.c',
]

# Public headers; will be installed and introspected
gdk_x11_public_headers = files([
  'gdkx-autocleanups.h',
  'gdkx11applaunchcontext.h',
  'gdkx11device-xi2.h',
  'gdkx11device.h',
  'gdkx11devicemanager-xi2.h',
  'gdkx11devicemanager.h',
  'gdkx11display.h',
  'gdkx11dnd.h',
  'gdkx11glcontext.h',
  'gdkx11monitor.h',
  'gdkx11property.h',
  'gdkx11screen.h',
  'gdkx11selection.h',
  'gdkx11utils.h',
  'gdkx11surface.h',
])

install_headers(gdk_x11_public_headers, 'gdkx.h', subdir: 'gtk-4.0/gdk/x11/')

gdk_x11_deps = [
  xrender_dep,
  xi_dep,
  xext_dep,
  x11_dep,
  xcursor_dep,
  xdamage_dep,
  xfixes_dep,
  xrandr_dep,
  xinerama_dep,
]

libgdk_x11 = static_library('gdk-x11',
  sources: [ gdk_gen_headers, gdk_x11_sources ],
  include_directories: [ confinc, gdkinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  dependencies: [ gdk_deps, gdk_x11_deps, ],
)

x11_public_deps = [x11_dep]
