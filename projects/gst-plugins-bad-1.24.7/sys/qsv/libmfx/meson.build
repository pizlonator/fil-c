mfx_win32_sources = [
  'dispatcher/windows/main.cpp',
  'dispatcher/windows/mfx_critical_section.cpp',
  'dispatcher/windows/mfx_dispatcher_log.cpp',
  'dispatcher/windows/mfx_dispatcher.cpp',
  'dispatcher/windows/mfx_driver_store_loader.cpp',
  'dispatcher/windows/mfx_dxva2_device.cpp',
  'dispatcher/windows/mfx_function_table.cpp',
  'dispatcher/windows/mfx_library_iterator.cpp',
  'dispatcher/windows/mfx_load_dll.cpp',
  'dispatcher/windows/mfx_win_reg_key.cpp',
]

mfx_linux_sources = [
  'dispatcher/linux/mfxloader.cpp',
]

vpl_sources = [
  'dispatcher/vpl/mfx_dispatcher_vpl_config.cpp',
  'dispatcher/vpl/mfx_dispatcher_vpl_loader.cpp',
  'dispatcher/vpl/mfx_dispatcher_vpl_log.cpp',
  'dispatcher/vpl/mfx_dispatcher_vpl_lowlatency.cpp',
  'dispatcher/vpl/mfx_dispatcher_vpl_msdk.cpp',
  'dispatcher/vpl/mfx_dispatcher_vpl.cpp',
]

libmfx_extra_args = []
libmfx_extra_deps = []

libmfx_sources = vpl_sources
if host_system == 'windows'
  libmfx_sources += mfx_win32_sources
elif host_system == 'linux'
  libmfx_sources += mfx_linux_sources

  # Unlike Windows (libmfxhw64.dll is part of driver so it's system library),
  # user can build/install libmfx on Linux, so we need to define
  # "MFX_MODULES_DIR" for dispatcher to be able to search libmfx from
  # additional search path.
  libmfx_modules_dir = get_option('mfx-modules-dir')
  if libmfx_modules_dir == ''
    # This "libdir" will be likely wrong but may be fine since libmfx library
    # will be installed in the distro default library path as part of libmfx package
    # and dispatcher will try to load library from the distro default library path first
    libmfx_modules_dir = join_paths(prefix, get_option('libdir'))
  endif

  libmfx_extra_args += ['-DMFX_MODULES_DIR="@0@"'.format(libmfx_modules_dir)]
  libmfx_extra_deps += [
    cc.find_library('dl'),
    cc.find_library('pthread'),
  ]
else
  error('Only Windows or Linux build is supported')
endif

# suppress build warnings
if cc.get_id() == 'msvc'
  libmfx_extra_args += cc.get_supported_arguments([
    '/wd4189', # local variable is initialized but not referenced
  ])
else
  libmfx_extra_args += cc.get_supported_arguments([
    '-Wno-missing-declarations',
    '-Wno-deprecated-declarations',
    '-Wno-redundant-decls',
    '-Wno-unused-but-set-variable',
    '-Wno-unused-variable',
    # clang complains
    '-Wno-missing-braces',
    '-Wno-format-nonliteral',
  ])
endif

libmfx_incl = include_directories('dispatcher', 'api')

libmfx_static = static_library('libmfx-static',
  libmfx_sources,
  c_args : libmfx_extra_args,
  cpp_args : libmfx_extra_args,
  dependencies : libmfx_extra_deps + [gst_dep],
  include_directories : libmfx_incl,
  override_options: ['werror=false'],
)

libmfx_internal_dep = declare_dependency(
  link_with : libmfx_static,
  include_directories: [libmfx_incl, include_directories('api/vpl')]
)
