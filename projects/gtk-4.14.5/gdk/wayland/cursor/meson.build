wayland_cursor_sources = files([
  'wayland-cursor.c',
  'xcursor.c',
  'os-compatibility.c'
])

libwayland_cursor = static_library('wayland+cursor',
  sources: wayland_cursor_sources,
  include_directories: [ confinc, ],
  dependencies: [ glib_dep, wlclientdep, ],
  c_args: common_cflags,
)
