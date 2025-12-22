hlsl_ps_source = files('PSMain_converter.hlsl')

hlsl_ps_input_formats = [
  ['NV12', false],
  ['NV21', false],
  ['I420', false],
  ['YV12', false],
  ['I420_10', false],
  ['I420_12', false],
  ['VUYA', false],
  ['VUYAPremul', false],
  ['Y410', false],
  ['AYUV', false],
  ['AYUVPremul', false],
  ['Y412', false],
  ['Y412Premul', false],
  ['RGBA', true],
  ['RGBAPremul', true],
  ['RGBx', true],
  ['GBR', true],
  ['GBR_10', true],
  ['GBR_12', true],
  ['GBRA', true],
  ['GBRAPremul', true],
  ['GBRA_10', true],
  ['GBRAPremul_10', true],
  ['GBRA_12', true],
  ['GBRAPremul_12', true],
  ['RGBP', true],
  ['BGRP', true],
  ['xRGB', true],
  ['ARGB', true],
  ['ARGBPremul', true],
  ['xBGR', true],
  ['ABGR', true],
  ['ABGRPremul', true],
  ['BGR10A2', true],
  ['BGRA64', true],
  ['BGRA64Premul', true],
  ['RBGA', true],
  ['RBGAPremul', true],
]

hlsl_ps_output_formats = [
  ['PS_OUTPUT_LUMA', 'Luma', false],
  ['PS_OUTPUT_LUMA', 'Luma_10', false],
  ['PS_OUTPUT_LUMA', 'Luma_12', false],
  ['PS_OUTPUT_CHROMA', 'ChromaNV12', false],
  ['PS_OUTPUT_CHROMA', 'ChromaNV21', false],
  ['PS_OUTPUT_CHROMA_PLANAR', 'ChromaI420', false],
  ['PS_OUTPUT_CHROMA_PLANAR', 'ChromaYV12', false],
  ['PS_OUTPUT_CHROMA_PLANAR', 'ChromaI420_10', false],
  ['PS_OUTPUT_CHROMA_PLANAR', 'ChromaI420_12', false],
  ['PS_OUTPUT_PLANAR', 'Y444', false],
  ['PS_OUTPUT_PLANAR', 'Y444_10', false],
  ['PS_OUTPUT_PLANAR', 'Y444_12', false],
  ['PS_OUTPUT_PLANAR', 'GBR', true],
  ['PS_OUTPUT_PLANAR', 'GBR_10', true],
  ['PS_OUTPUT_PLANAR', 'GBR_12', true],
  ['PS_OUTPUT_PLANAR', 'RGBP', true],
  ['PS_OUTPUT_PLANAR', 'BGRP', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRA', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRAPremul', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRA_10', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRAPremul_10', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRA_12', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRAPremul_12', true],
  ['PS_OUTPUT_PACKED', 'RGBA', true],
  ['PS_OUTPUT_PACKED', 'RGBAPremul', true],
  ['PS_OUTPUT_PACKED', 'RBGA', true],
  ['PS_OUTPUT_PACKED', 'RBGAPremul', true],
  ['PS_OUTPUT_PACKED', 'RGBx', true],
  ['PS_OUTPUT_PACKED', 'VUYA', false],
  ['PS_OUTPUT_PACKED', 'VUYAPremul', false],
  ['PS_OUTPUT_PACKED', 'AYUV', false],
  ['PS_OUTPUT_PACKED', 'AYUVPremul', false],
  ['PS_OUTPUT_PACKED', 'xRGB', true],
  ['PS_OUTPUT_PACKED', 'ARGB', true],
  ['PS_OUTPUT_PACKED', 'ARGBPremul', true],
  ['PS_OUTPUT_PACKED', 'xBGR', true],
  ['PS_OUTPUT_PACKED', 'ABGR', true],
  ['PS_OUTPUT_PACKED', 'ABGRPremul', true],
]

header_collector = find_program('collect_hlsl_header.py')

foreach input_format : hlsl_ps_input_formats
  in_format = input_format.get(0)
  foreach output_format : hlsl_ps_output_formats
    converter = ''
    if input_format.get(1) != output_format.get(2)
      converter = 'Simple'
    else
      converter = 'Identity'
    endif
    output_type = output_format.get(0)
    output_builder = output_format.get(1)
    entry_point = 'PSMain_@0@_@1@_@2@'.format(in_format, converter, output_builder)
    header = '@0@.h'.format(entry_point)
    compiled_shader = custom_target(header,
        input : hlsl_ps_source,
        output : header,
        command : [fxc, '/Fh', '@OUTPUT@',
                  '/E', entry_point,
                  '/T', 'ps_4_0',
                  '/D', 'BUILDING_HLSL=1',
                  '/D', 'OUTPUT_TYPE=@0@'.format(output_type),
                  '/D', 'ENTRY_POINT=@0@'.format(entry_point),
                  '/D', 'SAMPLER=Sampler@0@'.format(in_format),
                  '/D', 'CONVERTER=Converter@0@'.format(converter),
                  '/D', 'OUTPUT_BUILDER=Output@0@'.format(output_builder),
                  '/nologo',
                  '@INPUT@'])
    hlsl_precompiled += [compiled_shader]
  endforeach
endforeach

header_collection = 'PSMainConverter.h'
generated_collection = custom_target(header_collection,
    input : hlsl_precompiled,
    output : header_collection,
    command : [header_collector,
        '--input', meson.current_build_dir(),
        '--prefix', 'PSMain_',
        '--output', '@OUTPUT@'
    ])

hlsl_precompiled += generated_collection

hlsl_vs_sources = [
  [files('VSMain_converter.hlsl'), 'VSMain_converter', 'vs_4_0'],
]

foreach shader : hlsl_vs_sources
  source = shader.get(0)
  entry_point = shader.get(1)
  header = '@0@.h'.format(entry_point)
  compiled_shader = custom_target(header,
      input : source,
      output : header,
      command : [fxc, '/Fh', '@OUTPUT@',
                '/E', entry_point,
                '/T', shader.get(2),
                '/D', 'BUILDING_HLSL=1',
                '/nologo',
                '@INPUT@'])
  hlsl_precompiled += [compiled_shader]
endforeach

hlsl_cs_source = files('CSMain_converter.hlsl')
hlsl_cs_entry_points = [
  'CSMain_YUY2_to_AYUV',
  'CSMain_UYVY_to_AYUV',
  'CSMain_VYUY_to_AYUV',
  'CSMain_YVYU_to_AYUV',
  'CSMain_v210_to_AYUV',
  'CSMain_v308_to_AYUV',
  'CSMain_IYU2_to_AYUV',
  'CSMain_AYUV_to_YUY2',
  'CSMain_AYUV_to_UYVY',
  'CSMain_AYUV_to_VYUY',
  'CSMain_AYUV_to_YVYU',
  'CSMain_AYUV_to_v210',
  'CSMain_AYUV_to_v308',
  'CSMain_AYUV_to_IYU2',
  'CSMain_AYUV_to_Y410',
  'CSMain_RGB_to_RGBA',
  'CSMain_BGR_to_RGBA',
  'CSMain_RGB16_to_RGBA',
  'CSMain_BGR16_to_RGBA',
  'CSMain_RGB15_to_RGBA',
  'CSMain_BGR15_to_RGBA',
  'CSMain_r210_to_RGBA',
  'CSMain_RGBA_to_RGB',
  'CSMain_RGBA_to_BGR',
  'CSMain_RGBA_to_RGB16',
  'CSMain_RGBA_to_BGR16',
  'CSMain_RGBA_to_RGB15',
  'CSMain_RGBA_to_BGR15',
  'CSMain_RGBA_to_r210',
  'CSMain_RGBA_to_BGRA',
]

foreach shader : hlsl_cs_entry_points
  entry_point = shader
  header = '@0@.h'.format(entry_point)
  compiled_shader = custom_target(header,
      input : hlsl_cs_source,
      output : header,
      command : [fxc, '/Fh', '@OUTPUT@',
                '/E', entry_point,
                '/T', 'cs_5_0',
                '/D', 'BUILDING_HLSL=1',
                '/D', 'ENTRY_POINT=@0@'.format(entry_point),
                '/D', 'BUILDING_@0@=1'.format(entry_point),
                '/nologo',
                '@INPUT@'])
  hlsl_precompiled += [compiled_shader]
endforeach

header_collection = 'CSMainConverter.h'
generated_collection = custom_target(header_collection,
    input : hlsl_precompiled,
    output : header_collection,
    command : [header_collector,
        '--input', meson.current_build_dir(),
        '--prefix', 'CSMain_',
        '--output', '@OUTPUT@'
    ])

hlsl_precompiled += generated_collection