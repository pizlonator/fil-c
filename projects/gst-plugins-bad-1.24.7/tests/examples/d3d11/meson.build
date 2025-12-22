if host_system != 'windows'
  subdir_done()
endif

executable('d3d11videosink',
  ['d3d11videosink.c', '../key-handler.c'],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  install: false,
)

executable('d3d11screencapturesrc',
  ['d3d11screencapturesrc.cpp'],
  c_args : gst_plugins_bad_args,
  cpp_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  install: false,
)

d3d11_lib = cc.find_library('d3d11', required : false)
dxgi_lib = cc.find_library('dxgi', required : false)
d3dcompiler_lib = cc.find_library('d3dcompiler', required: false)
have_d3d11_h = cc.has_header('d3d11.h')
have_dxgi_h = cc.has_header('dxgi1_2.h')
have_d3d11compiler_h = cc.has_header('d3dcompiler.h')

d3d9_dep = cc.find_library('d3d9', required : false)
have_d3d9_h = cc.has_header('d3d9.h')

d2d_dep = cc.find_library('d2d1', required: false)
have_d2d_h = cc.has_header('d2d1.h', required: false)

dwrite_dep = cc.find_library('dwrite', required: false)
have_dwrite_h = cc.has_header('dwrite.h')

if d3d11_lib.found() and dxgi_lib.found() and d3dcompiler_lib.found() and have_d3d11_h and have_dxgi_h and have_d3d11compiler_h
  executable('d3d11videosink-shared-texture',
    ['d3d11videosink-shared-texture.cpp', 'd3d11device.cpp'],
    c_args : gst_plugins_bad_args,
    cpp_args : gst_plugins_bad_args,
    include_directories : [configinc, libsinc],
    dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, d3dcompiler_lib],
    install: false,
  )

  if gstd3d11_dep.found ()
    executable('d3d11converter', ['d3d11converter.cpp', '../key-handler.c'],
      c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      include_directories : [configinc, libsinc],
      dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, gstd3d11_dep, gstapp_dep],
      install: false,
    )

    executable('d3d11decoder-appsink', ['d3d11decoder-appsink.cpp'],
      c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      include_directories : [configinc, libsinc],
      dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, gstd3d11_dep, gstapp_dep],
      install: false,
    )

    executable('d3d11videosink-appsrc', ['d3d11videosink-appsrc.cpp'],
      c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
      include_directories : [configinc, libsinc],
      dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, gstd3d11_dep, gstapp_dep],
      install: false,
    )

    if d2d_dep.found() and have_d2d_h and dwrite_dep.found() and have_dwrite_h
      executable('d3d11videosink-present', ['d3d11videosink-present.cpp'],
        c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        include_directories : [configinc, libsinc],
        dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, gstd3d11_dep, d2d_dep, dwrite_dep],
        install: false,
      )

      executable('d3d11overlay', ['d3d11overlay.cpp'],
        c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        include_directories : [configinc, libsinc],
        dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, gstd3d11_dep, d2d_dep, dwrite_dep],
        install: false,
      )
    endif
  endif

  if d3d_dep.found() and have_d3d9_h
    executable('d3d11videosink-shared-texture-d3d9ex',
      ['d3d11videosink-shared-texture-d3d9ex.cpp', 'd3d11device.cpp'],
      c_args : gst_plugins_bad_args,
      cpp_args : gst_plugins_bad_args,
      include_directories : [configinc, libsinc],
      dependencies: [gst_dep, gstbase_dep, gstvideo_dep, d3d11_lib, dxgi_lib, d3dcompiler_lib, d3d9_dep],
      install: false,
    )
  endif
endif
