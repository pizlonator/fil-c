openexr_dep = dependency('OpenEXR', modules: ['OpenEXR::IlmImf'], required: get_option('openexr'))

if openexr_dep.found()
  openexr_override_options = []
  # Older versions of openexr fail to build with -Werror when using GCC >= 9.2
  # and Clang >= 6 because it uses deprecated C++98 syntax. Explicitly pass
  # -std=c++98 in those cases. Just checking the openexr version is not enough
  # because distros (such as Ubuntu 18.04) have backported patches due to which
  # older openexr versions now require C++11.
  if openexr_dep.version().version_compare('< 2.4.0')
    # Check whether using the openexr headers with -Werror causes an error
    if cxx.has_argument('-Werror') and cxx.check_header('ImfRgbaFile.h', dependencies: openexr_dep)
      if not cxx.check_header('ImfRgbaFile.h', dependencies: openexr_dep, args: '-Werror')
        # If setting -std to c++98 fixes it, use that! Else, warn.
        if cxx.check_header('ImfRgbaFile.h', dependencies: openexr_dep, args: ['-Werror', '-std=c++98'])
          openexr_override_options = ['cpp_std=c++98']
        else
          warning('openexr headers can\'t be included with \'-Werror\', and no workaround found')
        endif
      endif
    endif
  endif

  gstopenexr = library('gstopenexr',
    'gstopenexr.c',
    'gstopenexrdec.cpp',
    c_args: gst_plugins_bad_args,
    cpp_args: gst_plugins_bad_args,
    link_args: noseh_link_args,
    include_directories: [configinc, libsinc],
    dependencies: [gstvideo_dep, openexr_dep],
    override_options: openexr_override_options,
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstopenexr]
endif
