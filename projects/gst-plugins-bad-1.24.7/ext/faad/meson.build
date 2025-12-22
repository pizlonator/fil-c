faad_opt = get_option('faad').require(gpl_allowed, error_message: '''
  Plugin faad explicitly required via options but GPL-licensed plugins disabled via options.
  Pass option -Dgpl=enabled to Meson to allow GPL-licensed plugins to be built.
  ''')

if faad_opt.disabled()
  faad_dep = dependency('', required: false)
  subdir_done()
endif

faad_args = [ ]

have_faad = cc.has_header_symbol('neaacdec.h', 'NeAACDecOpen')
have_faad_2_7 = have_faad and cc.has_header_symbol('neaacdec.h', 'LATM')
if have_faad and not have_faad_2_7 and faad_opt.enabled()
  message('Found faad2, but too old (< v2.7.0)')
endif

faad_dep = cc.find_library('faad', required : faad_opt)

if faad_dep.found() and have_faad_2_7
  gstfaad = library('gstfaad',
    'gstfaad.c',
    c_args : gst_plugins_bad_args + [ '-DFAAD2_MINOR_VERSION=7', '-DFAAD_IS_NEAAC' ],
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, gstpbutils_dep, gsttag_dep, faad_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstfaad]
endif
