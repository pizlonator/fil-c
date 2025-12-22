hlsl_sources = [
  ['PSMain_checker_luma', 'ps_4_0'],
  ['PSMain_checker_rgb', 'ps_4_0'],
  ['PSMain_checker_vuya', 'ps_4_0'],
  ['PSMain_checker', 'ps_4_0'],
  ['PSMain_color', 'ps_4_0'],
  ['PSMain_sample_premul', 'ps_4_0'],
  ['PSMain_sample', 'ps_4_0'],
  ['PSMain_snow', 'ps_4_0'],
  ['VSMain_color', 'vs_4_0'],
  ['VSMain_coord', 'vs_4_0'],
  ['VSMain_pos', 'vs_4_0'],
]

foreach shader : hlsl_sources
  entry_point = shader.get(0)
  source = files('@0@.hlsl'.format(entry_point))
  header = '@0@.h'.format(entry_point)
  compiled_shader = custom_target(header,
      input : source,
      output : header,
      command : [fxc, '/Fh', '@OUTPUT@',
                '/E', entry_point,
                '/T', shader.get(1),
                '/D', 'BUILDING_HLSL=1',
                '/nologo',
                '@INPUT@'])
  hlsl_precompiled += [compiled_shader]
endforeach
