openh264_sources = [
  'gstopenh264dec.cpp',
  'gstopenh264enc.cpp',
  'gstopenh264plugin.c',
]

openh264_dep = dependency('openh264', version : '>= 1.3.0',
                          required : get_option('openh264'),
                          fallback: ['openh264', 'openh264_dep'])

if openh264_dep.found()
  gstopenh264 = library('gstopenh264',
    openh264_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, openh264_dep, gstpbutils_dep, ],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopenh264]
endif
