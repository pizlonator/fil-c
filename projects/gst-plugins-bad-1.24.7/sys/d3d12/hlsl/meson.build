hlsl_conv_ps_source = files('PSMain_converter.hlsl')

hlsl_conv_ps_input_formats = [
  ['NV12', false, 2],
  ['NV21', false, 2],
  ['I420', false, 3],
  ['YV12', false, 3],
  ['I420_10', false, 3],
  ['I420_12', false, 3],
  ['VUYA', false, 1],
  ['Y410', false, 1],
  ['AYUV', false, 1],
  ['Y412', false, 1],
  ['RGBA', true, 1],
  ['RGBAPremul', true, 1],
  ['RGBx', true, 1],
  ['GBR', true, 3],
  ['GBR_10', true, 3],
  ['GBR_12', true, 3],
  ['GBRA', true, 4],
  ['GBRA_10', true, 4],
  ['GBRA_12', true, 4],
  ['RGBP', true, 3],
  ['BGRP', true, 3],
  ['xRGB', true, 1],
  ['ARGB', true, 1],
  ['xBGR', true, 1],
  ['ABGR', true, 1],
  ['BGR10A2', true, 1],
  ['BGRA64', true, 1],
  ['RBGA', true, 1],
  ['GRAY', false, 1],
]

hlsl_conv_ps_output_formats = [
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
  ['PS_OUTPUT_PLANAR_FULL', 'GBRA_10', true],
  ['PS_OUTPUT_PLANAR_FULL', 'GBRA_12', true],
  ['PS_OUTPUT_PACKED', 'RGBA', true],
  ['PS_OUTPUT_PACKED', 'RGBAPremul', true],
  ['PS_OUTPUT_PACKED', 'RBGA', true],
  ['PS_OUTPUT_PACKED', 'RGBx', true],
  ['PS_OUTPUT_PACKED', 'VUYA', false],
  ['PS_OUTPUT_PACKED', 'AYUV', false],
  ['PS_OUTPUT_PACKED', 'xRGB', true],
  ['PS_OUTPUT_PACKED', 'ARGB', true],
  ['PS_OUTPUT_PACKED', 'xBGR', true],
  ['PS_OUTPUT_PACKED', 'ABGR', true],
]

extra_converters = [
  'Gamma',
  'Primary',
]

header_collector = find_program('collect_hlsl_header.py')

foreach input_format : hlsl_conv_ps_input_formats
  in_format = input_format.get(0)
  num_srv = input_format.get(2)
  foreach output_format : hlsl_conv_ps_output_formats
    converters = []
    if input_format.get(1) != output_format.get(2)
      converters += ['Simple']
    else
      converters += ['Identity', 'Range']
    endif
    output_type = output_format.get(0)
    output_builder = output_format.get(1)
    foreach conv : converters
      entry_point = 'PSMain_@0@_@1@_@2@'.format(in_format, conv, output_builder)
      header = '@0@.h'.format(entry_point)
      compiled_shader = custom_target(header,
          input : hlsl_conv_ps_source,
          output : header,
          command : [dxc, '/Fh', '@OUTPUT@',
                    '/E', entry_point,
                    '/T', 'ps_6_0',
                    '/D', 'OUTPUT_TYPE=@0@'.format(output_type),
                    '/D', 'ENTRY_POINT=@0@'.format(entry_point),
                    '/D', 'SAMPLER=Sampler@0@'.format(in_format),
                    '/D', 'CONVERTER=Converter@0@'.format(conv),
                    '/D', 'OUTPUT_BUILDER=Output@0@'.format(output_builder),
                    '/D', 'NUM_SRV_@0@=1'.format(num_srv),
                    '/all-resources-bound',
                    '@INPUT@'])
      hlsl_precompiled += [compiled_shader]
    endforeach

    foreach conv : extra_converters
      entry_point = 'PSMain_@0@_@1@_@2@'.format(in_format, conv, output_builder)
      header = '@0@.h'.format(entry_point)
      compiled_shader = custom_target(header,
          input : hlsl_conv_ps_source,
          output : header,
          command : [dxc, '/Fh', '@OUTPUT@',
                    '/E', entry_point,
                    '/T', 'ps_6_0',
                    '/D', 'OUTPUT_TYPE=@0@'.format(output_type),
                    '/D', 'ENTRY_POINT=@0@'.format(entry_point),
                    '/D', 'SAMPLER=Sampler@0@'.format(in_format),
                    '/D', 'CONVERTER=Converter@0@'.format(conv),
                    '/D', 'OUTPUT_BUILDER=Output@0@'.format(output_builder),
                    '/D', 'NUM_SRV_@0@=1'.format(num_srv),
                    '/D', 'BUILD_LUT=1',
                    '/all-resources-bound',
                    '@INPUT@'])
      hlsl_precompiled += [compiled_shader]
    endforeach
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

hlsl_sources = [
  ['VSMain_converter', 'vs_6_0'],
  ['PSMain_sample', 'ps_6_0'],
  ['PSMain_sample_premul', 'ps_6_0'],
  ['VSMain_coord', 'vs_6_0'],
  ['PSMain_color', 'ps_6_0'],
  ['VSMain_color', 'vs_6_0'],
  ['PSMain_snow', 'ps_6_0'],
  ['PSMain_checker', 'ps_6_0'],
  ['PSMain_checker_luma', 'ps_6_0'],
  ['PSMain_checker_rgb', 'ps_6_0'],
  ['PSMain_checker_vuya', 'ps_6_0'],
  ['VSMain_pos', 'vs_6_0'],
]

foreach shader : hlsl_sources
  entry_point = shader.get(0)
  source = files('@0@.hlsl'.format(entry_point))
  header = '@0@.h'.format(entry_point)
  compiled_shader = custom_target(header,
      input : source,
      output : header,
      command : [dxc, '/Fh', '@OUTPUT@',
                '/E', entry_point,
                '/T', shader.get(1),
                '/all-resources-bound',
                '@INPUT@'])
  hlsl_precompiled += [compiled_shader]
endforeach


