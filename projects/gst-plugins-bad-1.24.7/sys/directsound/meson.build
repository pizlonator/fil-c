directsoundsrc_sources = [
  'gstdirectsoundsrc.c',
  'gstdirectsounddevice.c',
  'gstdirectsoundplugin.c',
]

directsoundsrc_device_flags = [
  '-DGstDirectSoundDeviceProvider=GstDirectSoundSrcDeviceProvider',
  '-DGstDirectSoundDeviceProviderClass=GstDirectSoundSrcDeviceProviderClass',
  '-DGstDirectSoundDevice=GstDirectSoundSrcDevice',
  '-DGstDirectSoundDeviceClass=GstDirectSoundSrcDeviceClass',
  '-DGST_DIRECTSOUND_SRC_DEVICE_PROVIDER=1'
]

if host_system != 'windows' or get_option('directsound').disabled()
  subdir_done()
endif

dsound_dep = cc.find_library('dsound', required : get_option('directsound'))
winmm_dep = cc.find_library('winmm', required : get_option('directsound'))
ole32_dep = cc.find_library('ole32', required : get_option('directsound'))
have_dsound_h = cc.has_header('dsound.h')
if not have_dsound_h and get_option('directsound').enabled()
  error('directsound plugin enabled but dsound.h not found')
endif

if dsound_dep.found() and winmm_dep.found() and ole32_dep.found() and have_dsound_h
  gstdirectsoundsrc = library('gstdirectsoundsrc',
    directsoundsrc_sources,
    c_args : gst_plugins_bad_args + directsoundsrc_device_flags,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, dsound_dep, winmm_dep, ole32_dep],
    install : true,
    install_dir : plugins_install_dir)
  plugins += [gstdirectsoundsrc]
endif
