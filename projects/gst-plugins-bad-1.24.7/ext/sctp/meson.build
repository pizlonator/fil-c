sctp_sources = [
  'gstsctpdec.c',
  'gstsctpenc.c',
  'gstsctpplugin.c',
  'sctpassociation.c'
]

sctp_option = get_option('sctp').enable_if(get_option('webrtc').enabled(), error_message: 'webrtc option is enabled')
if sctp_option.disabled()
  subdir_done()
endif

sctp_platform_deps = []

found_system_usrsctp = false

if not get_option('sctp-internal-usrsctp').enabled()
  sctp_dep = cc.find_library('usrsctp', required: false)
  sctp_header = cc.has_header('usrsctp.h')
  if host_system == 'windows'
    sctp_platform_deps += [cc.find_library('ws2_32')]
  endif

  found_system_usrsctp = sctp_dep.found() and sctp_header

  if get_option('sctp-internal-usrsctp').disabled() and not found_system_usrsctp
    if sctp_option.enabled()
      error('sctp plugin enabled but could not find libusrsctp or usrsctp.h, and internal libusrsctp disabled')
    else
      message('Could not find libusrsctp or usrsctp.h, and internal libusrsctp disabled - not building sctp plugin')
      subdir_done()
    endif
  endif
endif

if not found_system_usrsctp
  message('Using internal libusrsctp')
  subdir('usrsctp')
  sctp_dep = usrsctp_dep
  sctp_header = true
  if sctp_option.enabled() and not sctp_dep.found()
    error('sctp plugin enabled but could not find system libusrsctp or configure internal libusrsctp')
  endif
endif

if not gst_debug_disabled
  sctp_args = ['-DSCTP_DEBUG']
else
  sctp_args = []
endif

if sctp_dep.found() and sctp_header
  gstsctp = library('gstsctp',
    sctp_sources,
    c_args : gst_plugins_bad_args + sctp_args,
    include_directories : [configinc],
    dependencies : [sctp_dep, gst_dep, gstbase_dep, gstsctp_dep, sctp_platform_deps],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstsctp]
endif
