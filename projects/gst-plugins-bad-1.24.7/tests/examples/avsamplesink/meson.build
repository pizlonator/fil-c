if ['darwin', 'ios'].contains(host_system)
  avfoundation_dep = dependency('appleframeworks', modules : ['AVFoundation'], required : get_option('examples'))
  quartzcore_dep = dependency('appleframeworks', modules : ['QuartzCore'], required : get_option('examples'))
  corefoundation_dep = dependency('appleframeworks', modules : ['CoreFoundation'], required : get_option('examples'))
  cocoa_dep = dependency('appleframeworks', modules : ['Cocoa'], required : get_option('examples'))

  if quartzcore_dep.found() and avfoundation_dep.found() and corefoundation_dep.found() and cocoa_dep.found()
    executable('avsamplesink', 'main.m',
      include_directories : [configinc],
      dependencies : [gst_dep, avfoundation_dep, quartzcore_dep, corefoundation_dep, cocoa_dep],
      c_args : gst_plugins_bad_args,
      install: false)
  endif
endif
