if get_option('tests').disabled() or static_build
  subdir_done()
endif

if gstcheck_dep.found()
  subdir('check')
  subdir('interactive')
  subdir('validate')
endif
if not get_option('examples').disabled()
  subdir('examples')
endif
