gsk_private_gpu_include_shaders = files([
  'blendmode.glsl',
  'color.glsl',
  'common.glsl',
  'common-gl.glsl',
  'common-vulkan.glsl',
  'ellipse.glsl',
  'enums.glsl',
  'gradient.glsl',
  'pattern.glsl',
  'rect.glsl',
  'roundedrect.glsl',
])

gsk_private_gpu_shaders = files([
  'gskgpublendmode.glsl',
  'gskgpublur.glsl',
  'gskgpuborder.glsl',
  'gskgpuboxshadow.glsl',
  'gskgpucolor.glsl',
  'gskgpucolorize.glsl',
  'gskgpucolormatrix.glsl',
  'gskgpuconicgradient.glsl',
  'gskgpucrossfade.glsl',
  'gskgpulineargradient.glsl',
  'gskgpumask.glsl',
  'gskgpuradialgradient.glsl',
  'gskgpuroundedcolor.glsl',
  'gskgpustraightalpha.glsl',
  'gskgputexture.glsl',
  'gskgpuuber.glsl',
])

gsk_private_gpu_shader_headers = []
gsk_private_gpu_gl_shaders = []
gsk_private_gpu_vulkan_shaders = []

generate_header = find_program('generate-header.py')
process_includes = find_program('process-glsl-includes.py')

foreach shader: gsk_private_gpu_shaders
  instance = fs.name (fs.replace_suffix (shader, '')) + 'instance.h'
  shader_header = custom_target(instance,
                                output: instance,
                                input: shader,
                                command: [
                                  generate_header,
                                  '@INPUT@',
                                ],
                                capture: true)
  gsk_private_gpu_shader_headers += shader_header

  gl_shader_name = fs.name (shader)
  gl_shader = custom_target (gl_shader_name,
                             output: gl_shader_name,
                             input: shader,
                             depend_files: gsk_private_gpu_include_shaders,
                             command: [
                               process_includes,
                               '@INPUT@',
                             ],
                             capture: true)
  gsk_private_gpu_gl_shaders += gl_shader

  if (have_vulkan)
    glslc_options = [
      [ fs.name (fs.replace_suffix (shader, '')) + '.1.2.vert.spv',
        [ '--target-env=vulkan1.2',
          '-fshader-stage=vertex',
          '-DGSK_VERTEX_SHADER=1',
          '-DHAVE_VULKAN_1_2=1'
        ]
      ],
      [ fs.name (fs.replace_suffix (shader, '')) + '.1.2.frag.spv',
        [ '--target-env=vulkan1.2',
          '-fshader-stage=fragment',
          '-DGSK_FRAGMENT_SHADER=1',
          '-DHAVE_VULKAN_1_2=1'
        ]
      ],
      [ fs.name (fs.replace_suffix (shader, '')) + '.1.0.vert.spv',
        [ '--target-env=vulkan1.0',
          '-fshader-stage=vertex',
          '-DGSK_VERTEX_SHADER=1',
        ]
      ],
      [ fs.name (fs.replace_suffix (shader, '')) + '.1.0.frag.spv',
        [ '--target-env=vulkan1.0',
          '-fshader-stage=fragment',
          '-DGSK_FRAGMENT_SHADER=1',
        ]
      ],
    ]
    foreach option: glslc_options
      target = custom_target(option.get(0),
                             input: shader,
                             output: option.get(0),
                             depend_files: gsk_private_gpu_include_shaders,
                             command: [
                               glslc,
                               '-std=450',
                               option.get(1),
                               '@INPUT@',
                               '-o', '@OUTPUT@'
                             ])
      gsk_private_gpu_vulkan_shaders += target
    endforeach
  endif
endforeach

