avtp_sources = [
  'gstavtp.c',
  'gstavtpaafdepay.c',
  'gstavtpaafpay.c',
  'gstavtpcvfdepay.c',
  'gstavtprvfdepay.c',
  'gstavtpvfdepaybase.c',
  'gstavtpcvfpay.c',
  'gstavtprvfpay.c',
  'gstavtpvfpaybase.c',
  'gstavtpbasedepayload.c',
  'gstavtpbasepayload.c',
  'gstavtpsink.c',
  'gstavtpsrc.c',
  'gstavtpcrfutil.c',
  'gstavtpcrfbase.c',
  'gstavtpcrfsync.c',
  'gstavtpcrfcheck.c',
]

avtp_dep = dependency('', required: false)
avtp_option = get_option('avtp')

if host_machine.system() != 'linux' or not cc.has_type('struct sock_txtime', prefix : '#include <linux/net_tstamp.h>')
  if avtp_option.enabled()
    error('avtp plugin enabled but host is not supported')
  else
    subdir_done()
  endif
endif

avtp_dep = dependency('avtp', required: avtp_option, version : '>= 0.2.0',
                      fallback: ['avtp', 'avtp_dep'])

if avtp_dep.found()
  gstavtp = library('gstavtp',
    avtp_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, gstvideo_dep, avtp_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstavtp]
endif
