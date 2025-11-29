

gdkversionmacros_h = custom_target(
  input: 'gdkversionmacros.h.in',
  output: 'gdkversionmacros.h',
  command: [gen_visibility_macros, meson.project_version(), 'versions-macros', '@INPUT@', '@OUTPUT@'],
  install: true,
  install_dir: gtk_includedir / 'gtk-4.0/gdk/version',
  # FIXME: Not needed with Meson >= 0.64.0
  install_tag: 'devel',
)

gdk_visibility_h = custom_target(
  output: 'gdk-visibility.h',
  command: [gen_visibility_macros, meson.project_version(), 'visibility-macros', 'GDK', '@OUTPUT@'],
  install: true,
  install_dir: gtk_includedir / 'gtk-4.0/gdk/version',
  # FIXME: Not needed with Meson >= 0.64.0
  install_tag: 'devel',
)

