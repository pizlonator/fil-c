msdk_sources = [
  'gstmsdk.c',
  'gstmsdkallocator.c',
  'gstmsdkcaps.c',
  'gstmsdkcontext.c',
  'gstmsdkcontextutil.c',
  'gstmsdkdec.c',
  'gstmsdkdecproputil.c',
  'gstmsdkenc.c',
  'gstmsdkh264dec.c',
  'gstmsdkh264enc.c',
  'gstmsdkh265dec.c',
  'gstmsdkh265enc.c',
  'gstmsdkmjpegdec.c',
  'gstmsdkmjpegenc.c',
  'gstmsdkmpeg2dec.c',
  'gstmsdkmpeg2enc.c',
  'gstmsdkvc1dec.c',
  'gstmsdkvp8dec.c',
  'gstmsdkvpp.c',
  'gstmsdkvpputil.c',
  'msdk-enums.c',
  'msdk.c',
]

have_msdk = false
msdk_dep = []
use_msdk = false
use_onevpl = false
onevpl_extra_args = []

extra_args = [
  '-DGST_USE_UNSTABLE_API',
  '-DMFX_DEPRECATED_OFF'
]

msdk_option = get_option('msdk')
if msdk_option.disabled()
  subdir_done()
endif

if host_machine.system() == 'windows'
  msdk_sources += ['msdk_d3d.c', 'gstmsdkallocator_d3d.c' ]
  if gstd3d11_dep.found()
    extra_args += ['-DCOBJMACROS']
  else
    if msdk_option.enabled()
      error('The msdk plugin was enabled explicity, but required d3d11 dependencies were not found.')
    endif
    subdir_done()
  endif

else
  if not gstva_dep.found()
    if msdk_option.enabled()
      error('The msdk plugin was enabled explicity, but required va dependencies were not found.')
    endif
    subdir_done()
  endif
  msdk_sources += ['msdk_libva.c', 'gstmsdkallocator_libva.c']
endif

mfx_api = get_option('mfx_api')

if mfx_api != 'oneVPL'
  mfx_dep = dependency('libmfx', version: ['>= 1.0', '<= 1.99'], required: false)

  if mfx_dep.found()
    mfx_incdir = mfx_dep.get_variable('includedir')
    mfx_inc = []
    use_msdk = true
  else
    # Old versions of MediaSDK don't provide a pkg-config file
    mfx_root = run_command(python3, '-c', 'import os; print(os.environ.get("INTELMEDIASDKROOT", os.environ.get("MFX_HOME", "")))', check: false).stdout().strip()

    if mfx_root != ''
      mfx_libdir = [mfx_root + '/lib/lin_x64', mfx_root + '/lib/x64', mfx_root + '/lib64', mfx_root + '/lib']
      if host_machine.system() == 'windows'
        if host_machine.cpu_family() == 'x86'
          mfx_libdir = [mfx_root + '/lib/win32']
        else
          mfx_libdir = [mfx_root + '/lib/x64']
        endif
      endif
      mfx_incdir = join_paths([mfx_root, 'include'])
      mfx_lib = cxx.find_library('mfx', dirs: mfx_libdir, required: msdk_option)
      mfx_inc = include_directories(mfx_incdir)
      mfx_dep = declare_dependency(include_directories: mfx_inc, dependencies: mfx_lib)
      use_msdk = true
    endif
  endif
endif

if not use_msdk and mfx_api != 'MSDK'
  mfx_dep = dependency('vpl', version: '>= 2.2', required: false)

  if mfx_dep.found()
    mfx_incdir = mfx_dep.get_variable('includedir')
    mfx_inc = []
    use_onevpl = true
  endif
endif

if not use_msdk and not use_onevpl
  if msdk_option.enabled()
    error('msdk plugin enabled but the Intel Media SDK or the oneVPL SDK not found: consider setting PKG_CONFIG_PATH, INTELMEDIASDKROOT or MFX_HOME')
  else
    subdir_done()
  endif
endif

# Check oneVPL firstly
if use_onevpl
  onevpl_extra_args += ['-DONEVPL_EXPERIMENTAL']
  mfx_incdir = join_paths([mfx_incdir, 'vpl'])
  mfx_inc = include_directories(mfx_incdir)
elif cxx.has_header('mfx/mfxdefs.h', args: '-I' + mfx_incdir)
  mfx_incdir = join_paths([mfx_incdir, 'mfx'])
  mfx_inc = include_directories(mfx_incdir)
endif

if use_onevpl or cxx.has_header('mfxvp9.h', args: '-I' + mfx_incdir)
  msdk_sources += [ 'gstmsdkvp9dec.c' ]
  cdata.set('USE_MSDK_VP9_DEC', 1)
endif

# Usually MFX_VERSION 1026+ is required to support raw VP9 stream, however Fedora 30 has MFX_VERSION==1026
# but without support for raw VP9 stream, so mfxExtVP9Param is checked as well.
# See https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/merge_requests/988#note_408093
mfx_ver126_check_code = '''
#include <mfxdefs.h>
#include <mfxstructures.h>
#if MFX_VERSION < 1026
#error "The current version of mfx doesn't support raw vp9 stream"
#endif
mfxExtVP9Param ext;
'''

have_mfx_ver126 = cc.compiles(mfx_ver126_check_code,
                include_directories : [configinc, mfx_inc])

if have_mfx_ver126
  msdk_sources += [ 'gstmsdkvp9enc.c' ]
  cdata.set('USE_MSDK_VP9_ENC', 1)
endif

mfx_ver134_check_code = '''
#include <mfxdefs.h>
#if MFX_VERSION < 1034
#error "The current version of mfx doesn't support AV1 decoding"
#endif
'''

have_mfx_ver134 = cc.compiles(mfx_ver134_check_code,
                include_directories : [configinc, mfx_inc])

if have_mfx_ver134
  msdk_sources += [ 'gstmsdkav1dec.c' ]
  cdata.set('USE_MSDK_AV1_DEC', 1)
endif

mfx_ver205_check_code = '''
#include <mfxdefs.h>
#if MFX_VERSION < 2005
#error "The current verison of mfx doesn't support AV1 encoding"
#endif
'''

have_mfx_ver205 = cc.compiles(mfx_ver205_check_code,
                include_directories: [configinc, mfx_inc])

if use_onevpl and have_mfx_ver205
  msdk_sources += [ 'gstmsdkav1enc.c' ]
  cdata.set10('USE_MSDK_AV1_ENC', 1)
endif

if host_machine.system() == 'windows'
  if cc.get_id() != 'msvc' and msdk_option.enabled()
    error('msdk plugin can only be built with MSVC')
  endif
  legacy_stdio_dep = cc.find_library('legacy_stdio_definitions', required: get_option('msdk'))
  msdk_deps = declare_dependency(dependencies: [gstd3d11_dep, legacy_stdio_dep])
  msdk_deps_found = gstd3d11_dep.found() and legacy_stdio_dep.found() and cc.get_id() == 'msvc'
else
  libdl_dep = cc.find_library('dl', required: get_option('msdk'))
  libgudev_dep = dependency('gudev-1.0', required: get_option('msdk'), allow_fallback: true)
  libdrm_dep = dependency('libdrm', required: get_option('msdk'))
  msdk_deps = declare_dependency(dependencies: [gstva_dep, libdl_dep, libgudev_dep, libdrm_dep])
  msdk_deps_found = gstva_dep.found() and libdl_dep.found() and libgudev_dep.found() and libdrm_dep.found()
endif

if msdk_deps_found
  if host_machine.system() != 'windows'
    driverdir = libva_dep.get_variable('driverdir', default_value: '')
    if driverdir == ''
      driverdir = join_paths(get_option('prefix'), get_option('libdir'), 'dri')
    endif
    cdata.set_quoted('VA_DRIVERS_PATH', '@0@'.format(driverdir))
  endif

  gstmsdktag = library('gstmsdk',
    msdk_sources,
    c_args : gst_plugins_bad_args + extra_args + onevpl_extra_args,
    include_directories : [configinc, mfx_inc],
    dependencies : [gstbase_dep, gstvideo_dep, gstpbutils_dep, gstallocators_dep, gstcodecparsers_dep, mfx_dep, msdk_deps],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstmsdktag]
  have_msdk = true
endif
