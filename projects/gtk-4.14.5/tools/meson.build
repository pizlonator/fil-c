# Installed tools

extra_update_icon_cache_objs = []
if win32_enabled
  gen_uac_manifest = find_program('generate-uac-manifest.py')

  uac_exe_pkg = 'gtk4'
  uac_exe_name = '@0@-update-icon-cache'.format(uac_exe_pkg)

  # Well, we have to forgo the xxx.exe.manifest in the output listing, since
  # compile_resources doesn't like to consume targets with multiple outputs,
  # and the xxx.exe.manifest and xxx.rc are tied together
  uac_rc = custom_target(
    'tools/@0@.rc'.format(uac_exe_name),
    output: ['@0@.rc'.format(uac_exe_name)],
    command: [gen_uac_manifest,
              '-p=@0@'.format(uac_exe_pkg),
              '-n=@0@'.format(uac_exe_name),
              '--pkg-version=@0@'.format(meson.project_version()),
              '--output-dir=@OUTDIR@'],
  )
  extra_update_icon_cache_objs = import('windows').compile_resources(uac_rc)
endif

gtk_tools = [
  ['gtk4-path-tool', ['gtk-path-tool.c',
                      'gtk-path-tool-decompose.c',
                      'gtk-path-tool-info.c',
                      'gtk-path-tool-render.c',
                      'gtk-path-tool-restrict.c',
                      'gtk-path-tool-reverse.c',
                      'gtk-path-tool-show.c',
                      'gtk-path-tool-utils.c',
                      'path-view.c'], [libgtk_dep]],
  ['gtk4-query-settings', ['gtk-query-settings.c'], [libgtk_dep]],
  ['gtk4-builder-tool', ['gtk-builder-tool.c',
                         'gtk-builder-tool-simplify.c',
                         'gtk-builder-tool-validate.c',
                         'gtk-builder-tool-enumerate.c',
                         'gtk-builder-tool-screenshot.c',
                         'gtk-builder-tool-preview.c',
                         'fake-scope.c'], [libgtk_dep] ],
  ['gtk4-rendernode-tool', ['gtk-rendernode-tool.c',
                        'gtk-rendernode-tool-benchmark.c',
                        'gtk-rendernode-tool-compare.c',
                        'gtk-rendernode-tool-info.c',
                        'gtk-rendernode-tool-render.c',
                        'gtk-rendernode-tool-show.c',
                        'gtk-rendernode-tool-utils.c',
                        '../testsuite/reftests/reftest-compare.c'], [libgtk_dep] ],
  ['gtk4-update-icon-cache', ['updateiconcache.c', '../gtk/gtkiconcachevalidator.c' ] + extra_update_icon_cache_objs, [ libgtk_dep ] ],
  ['gtk4-encode-symbolic-svg', ['encodesymbolic.c'], [ libgtk_static_dep ] ],
]

if os_unix
  gtk_tools += [['gtk4-launch', ['gtk-launch.c'], [libgtk_dep]]]
endif

foreach tool: gtk_tools
  tool_name = tool.get(0)
  tool_srcs = tool.get(1)
  tool_deps = tool.get(2)

  exe = executable(tool_name,
    sources: tool_srcs,
    include_directories: [confinc],
    c_args: common_cflags + [ '-DBUILD_TOOLS' ],
    dependencies: tool_deps,
    install: true,
  )

  set_variable(tool_name.underscorify(), exe) # used in testsuites
  meson.override_find_program(tool_name, exe)
endforeach

# Data to install
install_data('gtk4builder.rng', install_dir: gtk_datadir / 'gtk-4.0')

install_data([
    'gtk4builder.loc',
    'gtk4builder.its',
  ],
  install_dir: gtk_datadir / 'gettext/its',
)
