if host_system == 'darwin'
  appkit_dep = dependency('appleframeworks', modules : ['AppKit'], required : true)

  executable('cocoa-videooverlay', 'cocoa-videooverlay.m',
    objc_args : [gst_plugins_base_args],
    include_directories: [configinc],
    dependencies : [gstgl_dep, corefoundation_dep, appkit_dep],
    install: false)
endif
