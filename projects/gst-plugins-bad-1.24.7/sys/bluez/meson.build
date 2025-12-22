bluez_sources = [
  'bluez-plugin.c',
  'gstbluezelement.c',
  'gsta2dpsink.c',
  'gstavdtpsink.c',
  'gstavdtpsrc.c',
  'gstavdtputil.c',
  'gstavrcputil.c',
]

if host_system == 'windows'
  subdir_done()
endif

bluez_dep = dependency('bluez', version : '>= 5.0', required : get_option('bluez'))
gio_unix_dep = dependency('gio-unix-2.0', required : get_option('bluez'))

if bluez_dep.found() and gio_unix_dep.found()
  bluez_ch = gnome.gdbus_codegen('bluez',
      sources : 'org.bluez.xml',
      interface_prefix : 'org.bluez',
      namespace : 'Bluez')

  gstbluez = library('gstbluez',
    bluez_sources, bluez_ch,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, gstrtp_dep, gstbase_dep, gio_unix_dep, bluez_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstbluez]
endif
