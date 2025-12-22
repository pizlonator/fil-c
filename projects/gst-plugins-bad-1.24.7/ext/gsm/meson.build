if get_option('gsm').disabled()
  subdir_done()
endif

gsm_cargs = []

gsm_dep = cc.find_library('gsm', required : get_option('gsm'))
have_gsm_create = cc.has_header_symbol('gsm.h', 'gsm_create')
if not have_gsm_create
  have_gsm_create = cc.has_header_symbol('gsm/gsm.h', 'gsm_create')
  if not have_gsm_create and get_option('gsm').enabled()
    error('GSM plugin is enabled: found libgsm but no headers')
  else
    gsm_cargs += ['-DGSM_HEADER_IN_SUBDIR']
  endif
endif

if gsm_dep.found() and have_gsm_create
  gstgsm = library('gstgsm',
    ['gstgsm.c', 'gstgsmenc.c', 'gstgsmdec.c'],
    c_args : gst_plugins_bad_args + gsm_cargs,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, gsm_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstgsm]
endif
