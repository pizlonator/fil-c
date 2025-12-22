win32ipc_sources = [
  'protocol/win32ipcmmf.cpp',
  'protocol/win32ipcpipeclient.cpp',
  'protocol/win32ipcpipeserver.cpp',
  'protocol/win32ipcprotocol.cpp',
  'protocol/win32ipcutils.cpp',
  'gstwin32ipcbufferpool.cpp',
  'gstwin32ipcmemory.cpp',
  'gstwin32ipcutils.cpp',
  'gstwin32ipcvideosink.cpp',
  'gstwin32ipcvideosrc.cpp',
  'plugin.cpp',
]

if host_system != 'windows' or get_option('win32ipc').disabled()
  subdir_done()
endif

code = '''
#include <windows.h>
#if !(WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP))
#error "Not building for UWP"
#endif'''
if cc.compiles(code, name : 'building for UWP')
  if get_option('win32ipc').enabled()
    error('win32ipc plugin does not support UWP')
  else
    subdir_done()
  endif
endif

gstwin32ipc = library('gstwin32ipc',
  win32ipc_sources,
  c_args : gst_plugins_bad_args,
  cpp_args: gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstwin32ipc]
