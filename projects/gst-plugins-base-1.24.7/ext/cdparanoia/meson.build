cdparanoia_deps = []
cdparanoia_found = false
cdparanoia_option = get_option('cdparanoia')
if cdparanoia_option.disabled()
  subdir_done()
endif

# cdparanoia upstream has a pkg-config file only in post-10.2 SVN so far, no release yet
cdparanoia_dep = dependency('cdparanoia-3', version : '>=10.2', required : false)
if cdparanoia_dep.found()
  cdparanoia_deps = [cdparanoia_dep]
  cdparanoia_found = true
else
  cdparanoia_dep = cc.find_library('cdda_paranoia', required : cdparanoia_option)
  cdinterface_dep = cc.find_library('cdda_interface', required : cdparanoia_option)
  if cdparanoia_dep.found() and cdinterface_dep.found()
    cdparanoia_deps = [cdparanoia_dep, cdinterface_dep]
    cdparanoia_found = true
    if cc.has_header_symbol('cdda/cdda_interface.h', 'cdda_open')
      core_conf.set('CDPARANOIA_HEADERS_IN_DIR', true)
    elif cc.has_header_symbol('cdda_interface.h', 'cdda_open')
      core_conf.set('CDPARANOIA_HEADERS_IN_DIR', false)
    endif
  endif
endif

if not cdparanoia_found and cdparanoia_option.enabled()
  error('cdparanoia plugin enabled but library not found')
endif

if cdparanoia_found
  # The cdda/cdda_interface.h header triggers GCC 12+ warnings which
  # then trickle down when we build the plugin with -Werror.
  # This wouldn't be needed usually, but cdparanoia's last release
  # was in 2008.
  extra_args = cc.get_supported_arguments([
      '-Wno-discarded-qualifiers',
      '-Wno-incompatible-pointer-types-discards-qualifiers',
      '-Wno-unused-variable'
  ])

  gstcdparanoia = library('gstcdparanoia',
    ['gstcdparanoiasrc.c'],
    include_directories: [configinc, libsinc],
    c_args : gst_plugins_base_args + extra_args,
    dependencies : cdparanoia_deps + [audio_dep, gst_dep, gst_base_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstcdparanoia]
endif
