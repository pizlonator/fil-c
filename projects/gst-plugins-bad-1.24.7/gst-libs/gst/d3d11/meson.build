d3d11_sources = [
  'gstd3d11bufferpool.cpp',
  'gstd3d11compile.cpp',
  'gstd3d11converter.cpp',
  'gstd3d11converter-builder.cpp',
  'gstd3d11converter-helper.cpp',
  'gstd3d11device.cpp',
  'gstd3d11format.cpp',
  'gstd3d11memory.cpp',
  'gstd3d11shadercache.cpp',
  'gstd3d11utils.cpp',
]

d3d11_headers = [
  'd3d11-prelude.h',
  'gstd3d11_fwd.h',
  'gstd3d11.h',
  'gstd3d11bufferpool.h',
  'gstd3d11compile.h',
  'gstd3d11converter.h',
  'gstd3d11device.h',
  'gstd3d11format.h',
  'gstd3d11memory.h',
  'gstd3d11utils.h',
]

gstd3d11_dep = dependency('', required : false)
directxmath_dep = dependency('', required : false)
d3d11_opt = get_option('d3d11')

if host_system != 'windows' or d3d11_opt.disabled()
  subdir_done()
endif

extra_c_args = [
  '-DCOBJMACROS',
]
extra_comm_args = [
  '-DGST_USE_UNSTABLE_API',
  '-DBUILDING_GST_D3D11',
  '-DG_LOG_DOMAIN="GStreamer-D3D11"',
]

have_d3d11sdk_h = false
have_dxgidebug_h = false
d3d11_winapi_app = false
d3d11_conf = configuration_data()

d3d11_lib = cc.find_library('d3d11', required: d3d11_opt)
dxgi_lib = cc.find_library('dxgi', required: d3d11_opt)
d3dcompiler_lib = cc.find_library('d3dcompiler', required: false)
runtimeobject_lib = cc.find_library('runtimeobject', required: false)

if not d3d11_lib.found() or not dxgi_lib.found()
  subdir_done()
endif

sdk_headers = [
  'd3d11_4.h',
  'dxgi1_6.h',
  'd3dcompiler.h'
]

foreach h : sdk_headers
  if not cc.has_header(h, required: d3d11_opt)
    subdir_done()
  endif
endforeach

d3d11_winapi_desktop = cxx.compiles('''#include <winapifamily.h>
    #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    #error "not win32"
    #endif''',
    dependencies: [d3d11_lib, dxgi_lib],
    name: 'building for Win32')

if runtimeobject_lib.found() and d3dcompiler_lib.found()
  d3d11_winapi_app = cxx.compiles('''#include <winapifamily.h>
      #include <windows.applicationmodel.core.h>
      #include <wrl.h>
      #include <wrl/wrappers/corewrappers.h>
      #include <d3d11_4.h>
      #include <dxgi1_6.h>
      #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
      #error "not winrt"
      #endif
      #if (WINVER < 0x0A00)
      #error "Windows 10 API is not guaranteed"
      #endif''',
      dependencies: [d3d11_lib, dxgi_lib, runtimeobject_lib],
      name: 'building for WinRT')
endif

if not d3d11_winapi_desktop and not d3d11_winapi_app
  if d3d11_opt.enabled()
    error('Not building for win32 or winRT?')
  endif
  subdir_done()
endif

extra_deps = []
d3d11_winapi_only_app = d3d11_winapi_app and not d3d11_winapi_desktop
d3d11_conf.set10('GST_D3D11_WINAPI_ONLY_APP', d3d11_winapi_only_app)
d3d11_conf.set10('GST_D3D11_WINAPI_APP', d3d11_winapi_app)

# for enabling debug layer
# NOTE: Disable d3d11/dxgi debug layer in case of [UWP build + release CRT]
# WACK (Windows App Certification Kit) doesn't seem to be happy with
# the DXGIGetDebugInterface1 symbol.

# FIXME: Probably DXGIGetDebugInterface1 might be used on UWP app for development
# purpose. So, I suspect one possible reason why WACK is complaining about
# DXGIGetDebugInterface1 is that debugging APIs couldn't be used for
# Windows store app, but couldn't find any reference about that.
#
# [IDXGIDebug1]
# https://docs.microsoft.com/en-us/windows/win32/api/dxgidebug/nn-dxgidebug-idxgidebug1
# is saying that the IDXGIDebug1 interface is available for both desktop app and
# UWP. And then the *DXGIGetDebugInterface1* method need to be called to obtain
# the IDXGIDebug1 interface.
#
# [DXGIGetDebugInterface1]
# https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-dxgigetdebuginterface1
# is mentioning that DXGIGetDebugInterface1 is desktop app only.
#
# PLEASE LET US KNOW A CORRECT WAY TO OBTAIN IDXGIDebug1 ON UWP, MICROSOFT
if not gst_debug_disabled and get_option('debug') and not (d3d11_winapi_only_app and get_option('b_vscrt') == 'md')
  d3d11_debug_libs = [
    ['d3d11sdklayers.h', 'ID3D11Debug', 'ID3D11InfoQueue', 'have_d3d11sdk_h'],
    ['dxgidebug.h', 'IDXGIDebug', 'IDXGIInfoQueue', 'have_dxgidebug_h'],
  ]

  foreach f : d3d11_debug_libs
    header = f.get(0)
    debug_obj = f.get(1)
    info_obj = f.get(2)
    compile_code = '''
                   #include <d3d11.h>
                   #include <dxgi.h>
                   #include <@0@>
                   int main(int arc, char ** argv) {
                     @1@ *debug = NULL;
                     @2@ *info_queue = NULL;
                     return 0;
                   }'''.format(header, debug_obj, info_obj)
    if cc.compiles(compile_code, dependencies: [d3d11_lib, dxgi_lib], name: debug_obj)
      set_variable(f.get(3), true)
    endif
  endforeach
else
  message('Disable D3D11Debug and DXGIDebug layers')
endif

# don't need to be defined in gstd3d11config.h since it's gstd3d11device internal
if have_d3d11sdk_h
  extra_comm_args += ['-DHAVE_D3D11SDKLAYERS_H']
endif

if have_dxgidebug_h
  extra_comm_args += ['-DHAVE_DXGIDEBUG_H']
endif

if d3d11_winapi_only_app
  extra_deps += [d3dcompiler_lib, runtimeobject_lib]
endif

# MinGW 32bits compiler seems to be complaining about redundant-decls
# when ComPtr is in use. Let's just disable the warning
if cc.get_id() != 'msvc'
  extra_args = cc.get_supported_arguments([
    '-Wno-redundant-decls',
  ])

  extra_comm_args += extra_args
endif

hlsl_precompiled = []
fxc = find_program ('fxc', required : get_option ('d3d11-hlsl-precompile'))
if cc.get_id() == 'msvc' and fxc.found()
  subdir('hlsl')
  extra_comm_args += ['-DHLSL_PRECOMPILED']
endif

have_dx_math = cxx.compiles('''
    #include <windows.h>
    #include <DirectXMath.h>
    using namespace DirectX;
    int main(int argc, char ** argv) {
      XMMATRIX matrix;
      XMFLOAT4X4 dump;
      matrix = XMMatrixIdentity ();
      XMStoreFloat4x4 (&dump, matrix);
      return 0;
    }
    ''',
    name: 'DirectXMath support in Windows SDK')

if not have_dx_math
  directxmath_dep = dependency('DirectXMath', 'directxmath',
      allow_fallback: true,
      version: '>= 3.1.9',
      required: d3d11_opt)
  if not directxmath_dep.found()
    subdir_done()
  endif
  extra_deps += [directxmath_dep]
endif

# https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-internals#windows-sse-versus-sse2
# x86 with Windows 7 or older may not support SSE2
if host_machine.cpu_family() != 'x86'
  have_dx_math_simd = cxx.compiles('''
      #include <windows.h>
      #include <DirectXMath.h>
      using namespace DirectX;
      int main(int argc, char ** argv) {
        XMVerifyCPUSupport ();
        return 0;
      }
      ''',
      name: 'DirectXMath SIMD support',
      dependencies: directxmath_dep)
  if have_dx_math_simd
    extra_comm_args += ['-DHAVE_DIRECTX_MATH_SIMD']
  elif get_option('d3d11-math').enabled()
    error('Usable DirectXMath not found')
  endif
endif

configure_file(
  input : 'gstd3d11config.h.meson',
  output : 'gstd3d11config.h',
  install_dir : get_option('libdir') + '/gstreamer-1.0/include/gst/d3d11',
  install_tag : 'devel',
  configuration: d3d11_conf,
)

pkg_name = 'gstreamer-d3d11-' + api_version
gstd3d11 = library('gstd3d11-' + api_version,
  d3d11_sources + hlsl_precompiled,
  c_args : gst_plugins_bad_args + extra_c_args + extra_comm_args,
  cpp_args : gst_plugins_bad_args + extra_comm_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gstbase_dep, gstvideo_dep, gmodule_dep, d3d11_lib, dxgi_lib] + extra_deps
)

pkgconfig.generate(gstd3d11,
  libraries : [gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  extra_cflags : ['-I${libdir}/gstreamer-1.0/include'],
  name : pkg_name,
  description : 'GStreamer Direct3D11 library',
)

library_def = {'lib': gstd3d11}
if build_gir
  gir_includes = ['Gst-1.0', 'GstBase-1.0', 'GstVideo-1.0']
  gir = {
    'sources' : d3d11_sources + d3d11_headers,
    'namespace' : 'GstD3D11',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : gir_includes,
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    'dependencies' : [gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib],
  }
  if not static_build
    d3d11_gir = gnome.generate_gir(gstd3d11, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [d3d11_gir]}
    gen_sources += d3d11_gir
  endif

  library_def += {'gir': [gir]}
endif
gst_libraries += [[pkg_name, library_def]]

install_headers(d3d11_headers, subdir : 'gstreamer-1.0/gst/d3d11')

gstd3d11_dep = declare_dependency(link_with : gstd3d11,
  include_directories : [libsinc],
  dependencies : [gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib],
  sources : gen_sources)

meson.override_dependency(pkg_name, gstd3d11_dep)
