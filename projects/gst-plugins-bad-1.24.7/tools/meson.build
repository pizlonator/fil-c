gst_tools = {
  'gst-transcoder': {
    'files': files('gst-transcoder.c', 'utils.c'),
    'deps': [gst_dep, gstpbutils_dep, gst_transcoder_dep],
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
      c_args: data.get('extra_c_args', []) + ['-DG_LOG_DOMAIN="@0@"'.format(exe_name)],
    )

    if data.has_key('man_page')
      install_man(data.get('man_page'))
    endif
  endforeach
endif