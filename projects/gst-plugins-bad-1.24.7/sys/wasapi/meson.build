wasapi_sources = [
  'gstmmdeviceenumerator.cpp',
  'gstwasapi.c',
  'gstwasapisrc.c',
  'gstwasapisink.c',
  'gstwasapiutil.c',
  'gstwasapidevice.c',
]

if host_system != 'windows'
  if get_option('wasapi').disabled()
    subdir_done()
  elif get_option('wasapi').enabled()
    error('Cannot build wasapi plugin when not building for Windows')
  endif
endif

ole32_dep = cc.find_library('ole32', required : get_option('wasapi'))
ksuser_dep = cc.find_library('ksuser', required : get_option('wasapi'))
have_audioclient_h = cc.has_header('audioclient.h')
if not have_audioclient_h and get_option('wasapi').enabled()
  error('wasapi plugin enabled but audioclient.h not found')
endif

if ole32_dep.found() and ksuser_dep.found() and have_audioclient_h
  wasapi_args = ['-DCOBJMACROS']
  if cc.has_header_symbol('audioclient.h', 'AUDCLNT_STREAMOPTIONS_NONE')
    wasapi_args += ['-DHAVE_AUDCLNT_STREAMOPTIONS']
  endif

  # MinGW defines some of these GUIDs in headers, whereas with MSVC we're
  # expected to define them in the code. Check which ones we need to provide.
  if cc.get_id() != 'msvc'
    if cc.has_header_symbol('audioclient.h', 'IID_IAudioClient')
      wasapi_args += ['-DHAVE_AUDCLNT_GUIDS']
    endif

    if cc.has_header_symbol('audioclient.h', 'IID_IAudioClient3')
      wasapi_args += ['-DHAVE_AUDCLNT3_GUID']
    endif
  endif

  gstwasapi = library('gstwasapi',
    wasapi_sources,
    c_args : gst_plugins_bad_args + wasapi_args,
    cpp_args: gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, ole32_dep, ksuser_dep],
    install : true,
    install_dir : plugins_install_dir)
  plugins += [gstwasapi]
endif
