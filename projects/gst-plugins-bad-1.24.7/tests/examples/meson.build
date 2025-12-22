subdir('audiomixmatrix')
subdir('avsamplesink')
subdir('camerabin2')
subdir('codecparsers')
subdir('codecs')
subdir('d3d11')
subdir('d3d12')
subdir('directfb')
subdir('gtk')
subdir('ipcpipeline')
subdir('mediafoundation')
subdir('mpegts')
subdir('msdk')
subdir('mxf')
subdir('nvcodec')
subdir('opencv', if_found: opencv_dep)
subdir('qsv')
subdir('qt6d3d11')
subdir('uvch264')
subdir('va')
subdir('waylandsink')
subdir('webrtc')
subdir('wpe')

executable('playout',
  'playout.c',
  install: true,
  dependencies : [gstbase_dep, gstvideo_dep],
  c_args : ['-DGST_USE_UNSTABLE_API', ],
)
