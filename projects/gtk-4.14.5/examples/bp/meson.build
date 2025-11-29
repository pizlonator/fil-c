bp_resources = gnome.compile_resources('bloatpad_resources',
                                       'bloatpad.gresources.xml',
                                       source_dir: meson.current_source_dir())

executable('bloatpad', 'bloatpad.c', bp_resources, dependencies: libgtk_dep, c_args: common_cflags)
