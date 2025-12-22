freeverb_sources = [
  'gstfreeverb.c',
]

gstfreeverb = library('gstfreeverb',
  freeverb_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstfreeverb]

install_data('GstFreeverb.prs', install_dir: presetdir)

env = environment()
env.prepend('GST_PRESET_PATH', meson.current_source_dir())
meson.add_devenv(env)
