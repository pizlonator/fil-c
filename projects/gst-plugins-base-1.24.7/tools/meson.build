tool_deps = [pbutils_dep, audio_dep, video_dep, tag_dep, gst_dep, gst_base_dep, gmodule_dep]

extra_args = []
extra_deps = []

if host_system == 'windows'
  # Check whether we're building for UWP apps, and if so, will not link winmm
  # of which APIs are for WIN32 desktop
  building_for_uwp = false
  code = '''
  #include <windows.h>
  #if !(WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP))
  #error "Not building for UWP"
  #endif'''
  if cc.compiles(code, name : 'building for UWP')
    building_for_uwp = true
  endif

  if not building_for_uwp
    winmm_lib = cc.find_library('winmm', required: false)
    if winmm_lib.found() and cc.has_header('mmsystem.h')
      extra_args += ['-DHAVE_WINMM']
      extra_deps += [winmm_lib]
    endif
  endif
endif

  gst_tools = {
    'gst-device-monitor': {
      'files': files('gst-device-monitor.c'),
      'deps': tool_deps,
      'man_page': files('gst-device-monitor-@0@.1'.format(api_version))
    },
    'gst-discoverer': {
      'files': files('gst-discoverer.c'),
      'deps': tool_deps,
      'man_page': files('gst-discoverer-@0@.1'.format(api_version))
    },
    'gst-play': {
      'files': files('gst-play.c', 'gst-play-kb.c'),
      'deps': tool_deps + extra_deps,
      'man_page': files('gst-play-@0@.1'.format(api_version)),
      'extra_c_args': extra_args
    },
  }

if not get_option('tools').disabled() and not static_build
  foreach tool, data: gst_tools
    exe_name = '@0@-@1@'.format(tool, api_version)
    executable(exe_name,
      data.get('files'),
      install: data.get('install', true),
      install_tag: 'bin',
      include_directories : data.get('include_directories', [configinc]),
      dependencies : data.get('deps'),
      c_args: data.get('extra_c_args', []) + gst_plugins_base_args + ['-DG_LOG_DOMAIN="@0@"'.format(exe_name)],
    )

    if data.has_key('man_page')
      install_man(data.get('man_page'))
    endif
  endforeach

endif
