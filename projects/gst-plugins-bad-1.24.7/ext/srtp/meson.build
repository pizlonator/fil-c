srtp_sources = [
  'gstsrtp.c',
  'gstsrtpelement.c',
  'gstsrtpplugin.c',
  'gstsrtpdec.c',
  'gstsrtpenc.c',
]

srtp_cargs = []
srtp_option = get_option('srtp').enable_if(get_option('webrtc').enabled(), error_message: 'webrtc option is enabled')
if srtp_option.disabled()
  srtp_dep = dependency('', required : false)
  subdir_done()
endif

srtp_dep = dependency('libsrtp2', version : '>= 2.1.0', required : false, allow_fallback: true)
if srtp_dep.found()
  srtp_cargs += ['-DHAVE_SRTP2']
else
  srtp_dep = dependency('libsrtp', version: '>= 1.6.0', required : false)
  if not srtp_dep.found() and cc.has_header_symbol('srtp/srtp.h', 'crypto_policy_set_aes_gcm_128_16_auth')
    srtp_dep = cc.find_library('srtp', required : false)
  endif
endif
if not srtp_dep.found() and srtp_option.enabled()
  error('srtp plugin enabled but libsrtp not found')
endif

if srtp_dep.found()
  gstsrtp_enums = gnome.mkenums_simple('gstsrtp-enumtypes',
    sources : ['gstsrtpenums.h'],
    decorator : 'G_GNUC_INTERNAL',
    install_header: false)

  gstsrtp = library('gstsrtp',
    srtp_sources, gstsrtp_enums,
    c_args : gst_plugins_bad_args + srtp_cargs,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstrtp_dep, gstvideo_dep, srtp_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstsrtp]
endif
