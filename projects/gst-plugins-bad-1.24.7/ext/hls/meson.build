hls_sources = [
  'gsthlsdemux.c',
  'gsthlsdemux-util.c',
  'gsthlselement.c',
  'gsthlsplugin.c',
  'gsthlssink.c',
  'gsthlssink2.c',
  'gstm3u8playlist.c',
  'm3u8.c',
]

hls_cargs = ['-DGST_USE_UNSTABLE_API']

hls_crypto = get_option('hls-crypto')
hls_option = get_option('hls')
hls_crypto_dep = dependency('', required : false)
# used for unit test
hls_dep = dependency('', required : false)

if hls_option.disabled()
  subdir_done()
endif

if ['auto', 'nettle'].contains(hls_crypto)
  hls_crypto_dep = dependency('nettle', version : '>= 3.0', required : false)
  if hls_crypto_dep.found()
    hls_cargs += ['-DHAVE_NETTLE']
  endif
endif

if not hls_crypto_dep.found() and ['auto', 'libgcrypt'].contains(hls_crypto)
  hls_crypto_dep = cc.find_library('gcrypt', required : false)
  if hls_crypto_dep.found()
    hls_cargs += ['-DHAVE_LIBGCRYPT']
  endif
endif

if not hls_crypto_dep.found() and ['auto', 'openssl'].contains(hls_crypto)
  hls_crypto_dep = dependency('openssl', required : false)
  if hls_crypto_dep.found()
    hls_cargs += ['-DHAVE_OPENSSL']
  endif
endif

if not hls_crypto_dep.found()
  if hls_crypto == 'auto'
    message('Enable HLS plugin enable without crypto')
  elif hls_option.enabled()
    error('HLS plugin enabled with crypto, but crypto library "@0@" not found'.format(hls_crypto))
  else
    subdir_done()
  endif
endif

gsthls = library('gsthls',
  hls_sources,
  c_args : gst_plugins_bad_args + hls_cargs,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstpbutils_dep, gsttag_dep, gstvideo_dep,
                  gstadaptivedemux_dep, gsturidownloader_dep,
                  hls_crypto_dep, gio_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gsthls]
hls_dep = declare_dependency(include_directories : include_directories('.'))
