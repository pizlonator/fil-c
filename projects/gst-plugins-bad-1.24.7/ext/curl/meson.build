curl_sources = [
  'gstcurlbasesink.c',
  'gstcurl.c',
  'gstcurlelement.c',
  'gstcurlfilesink.c',
  'gstcurlftpsink.c',
  'gstcurlhttpsink.c',
  'gstcurlsftpsink.c',
  'gstcurlsmtpsink.c',
  'gstcurlsshsink.c',
  'gstcurltlssink.c',
  'gstcurlhttpsrc.c',
  'gstcurlqueue.c',
]

curl_dep = dependency('libcurl', version : '>= 7.55.0', required : get_option('curl'))

if curl_dep.found()
  ssh_dep = dependency('libssh2', version : '>= 1.4.3', required : get_option('curl-ssh2'))
  cdata.set('HAVE_SSH2', ssh_dep.found())

  gstcurl = library('gstcurl',
    curl_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstbase_dep, curl_dep] + winsock2 + network_deps,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstcurl]
endif
