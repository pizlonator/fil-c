if get_option('tinyalsa').disabled()
  subdir_done()
endif

have_tinyalsa = cc.has_header ('tinyalsa/asoundlib.h')
if have_tinyalsa
  tinyalsa_dep = cc.find_library('tinyalsa', required: false)
  have_tinyalsa = tinyalsa_dep.found()
endif

if not have_tinyalsa
  tinyalsa_dep = dependency('tinyalsa', required: get_option('tinyalsa'),
                            fallback: ['tinyalsa', 'tinyalsa_dep'])
  have_tinyalsa = tinyalsa_dep.found()
endif

if have_tinyalsa
  gsttinyalsa = library('gsttinyalsa',
    'tinyalsa.c', 'tinyalsasink.c',
    c_args: gst_plugins_bad_args,
    include_directories: [configinc],
    dependencies : [gstbase_dep, gstaudio_dep, tinyalsa_dep],
    install: true,
    install_dir: plugins_install_dir
  )
  plugins += [gsttinyalsa]
elif get_option('tinyalsa').enabled()
  error('tinyalsa plugin enabled but TinyALSA library or headers not found')
endif
