camerabin_sources = [
  'gstdigitalzoom.c',
  'gstviewfinderbin.c',
  'camerabingeneral.c',
  'gstwrappercamerabinsrc.c',
  'gstcamerabin2.c',
  'gstplugin.c',
]

gstcamerabin = library('gstcamerabin',
  camerabin_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  include_directories : [configinc, libsinc],
  link_with : gstbasecamerabin,
  dependencies : [gstbasecamerabin_dep, gstphotography_dep, gsttag_dep, gstapp_dep, gstpbutils_dep, gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstcamerabin]
