subdir('base')

if gnutls_dep.found()
  subdir('gnutls')
endif

if backends.contains('openssl')
  if ['darwin', 'ios'].contains(host_system)
    security_dep = dependency('appleframeworks', modules : ['Security'])
  elif ['windows'].contains(host_system)
    crypt32_dep = cc.find_library('crypt32')
  endif

  subdir('openssl')
endif

subdir('tests')
