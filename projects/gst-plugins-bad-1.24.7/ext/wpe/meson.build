building_wpe = false
wpe_feat = get_option('wpe').require(gstgl_dep.found(),
  error_message : 'wpe plugin enabled but GL support was not detected')

if not wpe_feat.allowed()
  subdir_done()
endif

# Version checks copied from Cog, licensed under MIT.
# https://github.com/Igalia/cog. The difference is that for the 2.0 API we
# require version 2.40.1, the first one without strict Application ID
# requirement for the WebProcess sandbox.
wpe_target_api_version_required = {
  '2.0': '>=2.40.1',
  '1.1': '>=2.33.1',
  '1.0': '>=2.28.0',
}

wpe_target_api = get_option('wpe_api')
if wpe_target_api == 'auto'
  foreach try_api : ['2.0', '1.1', '1.0']
    wpewebkit_dep = dependency('wpe-webkit-' + try_api,
                               version: wpe_target_api_version_required[try_api],
                               required: false)
    if wpewebkit_dep.found()
      wpe_target_api = try_api
      break
    endif
  endforeach
else
  wpewebkit_dep = dependency('wpe-webkit-' + wpe_target_api,
                             version: wpe_target_api_version_required[wpe_target_api])
endif
if wpe_target_api == 'auto' or not wpewebkit_dep.found()
  subdir_done()
endif

use_wpe2 = 0
if wpe_target_api == '2.0'
  use_wpe2 = 1
endif
gst_wpe_c_args = ['-DUSE_WPE2=@0@'.format(use_wpe2)]

wpe_fdo_dep = dependency('wpebackend-fdo-1.0', version : '>= 1.8', required : wpe_feat)
egl_dep = dependency('egl', required : wpe_feat)
xkbcommon_dep = dependency('xkbcommon', version : '>= 0.8', required : wpe_feat)
wl_server_dep = dependency('wayland-server', required : wpe_feat)

if not (wpe_fdo_dep.found() and egl_dep.found() and xkbcommon_dep.found())
  subdir_done()
endif

giounix_dep = dependency('gio-unix-2.0', required: false)

wpe_extension_install_dir = get_option('prefix') / get_option('libdir') / meson.project_name() / 'wpe-extension'

building_wpe = true
gstwpe = library('gstwpe',
  ['gstwpethreadedview.cpp', 'gstwpe.cpp', 'gstwpevideosrc.cpp', 'gstwpesrcbin.cpp'],
  dependencies : [egl_dep, wpewebkit_dep, wpe_fdo_dep, gstallocators_dep, gstaudio_dep, gstvideo_dep,
    gstbase_dep, gstgl_dep, xkbcommon_dep, wl_server_dep, giounix_dep],
  cpp_args : gst_plugins_bad_args + ['-DHAVE_CONFIG_H=1',
    '-DWPE_EXTENSION_INSTALL_DIR=' + wpe_extension_install_dir] + gst_wpe_c_args,
  include_directories : [configinc],
  install : true,
  install_dir : plugins_install_dir)

if giounix_dep.found()
  subdir('wpe-extension')
endif

plugins += [gstwpe]
