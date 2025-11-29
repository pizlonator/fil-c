compare_render = executable('compare-render',
  ['compare-render.c', '../reftests/reftest-compare.c', 'replay-node.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
)

node_parser = executable('node-parser', 'node-parser.c',
  dependencies: libgtk_dep,
  c_args: common_cflags,
)

compare_render_tests = [
  'big-box-glyph-nocairo',
  'big-checkerboard',
  'big-checkerboard-scaled-down',
  'big-checkerboard-scaled-down-nearest',
  'big-checkerboard-scaled-down2',
  'blend-difference',
  'blend-modes',
  'blend-invisible-child',
  'blend-in-rotate',
  'blend-normal',
  'blurred-lines',
  'blur-child-bounds-oversize-nogl',
  'blur-contents-outside-of-clip',
  'blur-huge-contents-outside-of-clip-nogl',
  'border-bottom-right',
  'border-one-rounded',
  'border-opacity',
  'border-zero-width-color',
  'borders-rotated',
  'borders-scaled',
  'clip-contained',
  'clip-coordinates-2d',
  'clip-coordinates-nocairo',
  'clip-in-rounded-clip1',
  'clip-in-rounded-clip2',
  'clip-in-rounded-clip3',
  'clip-in-smaller-rounded-clip',
  'clip-intersection-fail-opacity-nogl',
  'clipped-repeat',
  'clipped_rounded_clip',
  'clip-nested1',
  'clip-translate-offscreen',
  'color-blur0',
  'color-matrix-identity',
  'color-matrix-merge',
  'color-matrix-parsing',
  'conic-gradient-with-64-colorstops',
  'container-single-child-offscreen-for-opacity',
  'crossfade-clip-both-children',
  'cross-fade-clipped-with-huge-children-nogl',
  'cross-fade-in-opacity',
  'cross-fade-in-rotate',
  'css-background',
  'empty-blend',
  'empty-blur',
  'empty-border',
  'empty-cairo',
  'empty-clip',
  'empty-color',
  'empty-color-matrix',
  'empty-container',
  'empty-cross-fade',
  'empty-debug',
  'empty-inset-shadow',
  'empty-linear-gradient',
  'empty-mask',
  'empty-opacity',
  'empty-outset-shadow',
  'empty-repeat',
  'empty-rounded-clip',
  'empty-shadow',
  'empty-texture',
  'empty-transform',
  'fill',
  'fill2',
  'fill-clipped-nogl',
  'fill-fractional-translate-gradient-nogl',
  'fill-fractional-translate-nogl',
  'fill-opacity',
  'fill-scaled-up',
  'fill-with-3d-contents-nogl-nocairo',
  'glyph-cache-overflow',
  'glyph-cache-overflow-slices',
  'glyph-hinting-none',
  'glyph-hinting-slight',
  'glyph-hinting-position-nogl-nocairo',
  'glyph-subpixel-position',
  'glyph-scaling',
  'glyph-antialias',
  'huge-height',
  'huge-width',
  'inset-shadow-multiple',
  'invalid-transform',
  'issue-3615',
  'linear-gradient-3d-nocairo',
  'linear-gradient-nonorthogonal-scale-nogl',
  'linear-gradient-with-64-colorstops',
  'mask',
  'mask-clipped-inverted-alpha',
  'mask-modes',
  'mask-modes-in-opacity',
  'mask-modes-with-alpha',
  'mask-texture-color-alpha',
  'mipmap-generation-later',
  'mipmap-with-1x1',
  'nested-rounded-clips',
  'offscreen-forced-downscale',
  'offscreen-fractional-translate-nogl',
  'offscreen-pixel-alignment-nogl-nocairo',
  'offscreen-pixel-alignment2',
  'opacity_clip',
  'opacity-colormatrix-combinations',
  'opacity-overdraw',
  'opacity-overlapping-children',
  'outset_shadow_offset_both',
  'outset_shadow_offset_x',
  'outset_shadow_offset_y',
  'outset_shadow_rounded_top',
  'outset-shadow-scale-offset',
  'outset_shadow_simple',
  'radial-gradient-with-64-colorstops',
  'repeat',
  'repeating-linear-gradient-edge-colors',
  'repeating-radial-gradient-edge-colors',
  'repeat-child-bounds',
  'repeat-empty-child-bounds',
  'repeat-huge-overdraw',
  'repeat-negative-coords',
  'repeat-no-repeat',
  'repeat-node-tiling-horizontal',
  'repeat-node-tiling-vertical',
  'repeat-noninteger-nogl-nocairo',
  'repeat-repeats',
  'repeat-repeats-nested-nogl',
  'repeat-scaling',
  'repeat-texture',
  'repeat-tiny-nogl-nocairo',
  'repeating-gradient-scaled',
  'reuse-of-texture-nested-in-offscreens',
  'rounded-clip-with-huge-bounds-nogl',
  'scale-textures-negative-ngl',
  'scale-up-down',
  'scaled-cairo',
  'scaled-texture',
  'scaled-texture-scale',
  'scaled-texture-scale-clip-nocairo',
  'scale0-crash',
  'shadow-behind',
  'shadow-clip-contents',
  'shadow-huge-offset',
  'shadow-in-opacity',
  'shadow-offset-clip',
  'shadow-offset-to-outside-clip',
  'shadow-opacity',
  'shadow-replay-nocairo',
  'shrink-rounded-border',
  'stroke',
  'stroke-clipped-nogl',
  'stroke-fractional-translate-gradient-nogl',
  'stroke-fractional-translate-nogl',
  'stroke-opacity',
  'stroke-with-3d-contents-nogl-nocairo',
  'subpixel-positioning',
  'subpixel-positioning-hidpi-nogl-nocairo',
  'text-color-mix',
  'text-glyph-lsb',
  'text-mixed-color-nocairo',
  'text-mixed-color-colrv1',
  'texture-coords',
  'texture-scale-filters-nocairo',
  'texture-scale-magnify-10000x',
  'texture-scale-magnify-rotate',
  'texture-scale-nearest-noninteger-scaled-nogl',
  'texture-scale-nearest-vs-linear',
  'texture-scale-offset',
  'texture-scale-stripes',
  'texture-url',
  'transform-huge-child-nogl',
  'transform-huge-child-3d-nocairo-nogl',
  'transform-in-transform',
  'transform-in-transform-in-transform',
  'z-transform-clipping-bounds-nocairo',
]

# these are too sensitive to differences in the renderers
# to run in ci, but still useful to keep around
informative_render_tests = [
  'big-glyph',
  'empty-text',
  'huge-glyph',
  'rounded-clip-in-clip-nocairo', # both cairo and gl fail this one
  'text-missing-glyph', # https://gitlab.gnome.org/GNOME/pango/-/merge_requests/718
]

renderers = [
  { 'name': 'gl' },
  { 'name': 'broadway' },
  { 'name': 'cairo' },
  { 'name': 'ngl' },
  { 'name': 'vulkan' },
]

compare_xfails_small_texture = [
  'big-checkerboard',
  'big-checkerboard-scaled-down',
  'big-checkerboard-scaled-down2',
  'repeat-repeats', # only fails when rotated
  'glyph-scaling',
]

variants = {
  'plain': '--plain',
  'flipped': '--flip',
  'rotated': '--rotate',
  'repeated': '--repeat',
  'masked': '--mask',
  'replayed': '--replay',
  'clipped': '--clip',
  'colorflipped': '--colorflip',
}

compare_xfails = {
  'ngl': {
    # the gradients are prone to rounding errors which become
    # more pronounced in small clips, it seems
    'radial-gradient-with-64-colorstops': ['clipped', 'colorflipped'],
    'conic-gradient-with-64-colorstops': ['clipped', 'colorflipped'],
    'blur-child-bounds-oversize-nogl': ['clipped'], # 6450
    'rounded-clip-with-huge-bounds-nogl': ['plain','flipped','rotated','repeated','masked','replayed','clipped','colorflipped'], # 6532
    'big-checkerboard': ['clipped'], # 6444
  },
  'vulkan': {
    'radial-gradient-with-64-colorstops': ['clipped', 'colorflipped'],
    'conic-gradient-with-64-colorstops': ['clipped', 'colorflipped'],
    'blur-child-bounds-oversize-nogl': ['clipped'], # 6450
    'repeat-repeats-nested-nogl': ['clipped'], # 6522
    'repeating-radial-gradient-edge-colors': ['colorflipped'],
    'rounded-clip-with-huge-bounds-nogl': ['plain','flipped','rotated','repeated','masked','replayed','clipped','colorflipped'], # 6532
    'big-checkerboard': ['clipped'], # 6444
  },
  'gl': {
    # 6525
    'container-single-child-offscreen-for-opacity': ['rotated'],
    'cross-fade-in-opacity': ['rotated'],
    'mask-modes-in-opacity': ['rotated'],
    'opacity-overlapping-children': ['rotated'],
    'scaled-texture-scale': ['rotated'],
    'shadow-opacity': ['rotated'],
    'texture-scale-nearest-vs-linear': ['rotated'],
  },
}

foreach renderer : renderers
  renderer_name = renderer.get('name')
  renderer_xfails = compare_xfails.get(renderer_name, { })

  foreach testname : compare_render_tests
    test_xfails = renderer_xfails.get(testname, [])
    exclude_term = '-no' + renderer_name

    suites = [
      'gsk',
      'gsk-compare',
      'gsk-' + renderer_name,
      'gsk-compare-' + renderer_name,
    ]

    if compare_xfails_small_texture.contains(testname)
      suites += 'wayland_gles2_failing'
    endif

    test_env = [
      'GSK_RENDERER=' + renderer_name,
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'TEST_FONT_DIR=@0@/fonts'.format(meson.current_source_dir())
    ]

    if ((not testname.contains(exclude_term)) and
        (renderer_name != 'broadway' or broadway_enabled) and
        (renderer_name != 'vulkan' or have_vulkan))

      foreach variant : variants.keys()
        extra_suites = [ 'gsk-compare-' + variant + '-' + renderer_name ]
        if test_xfails.contains(variant) or (renderer_name == 'cairo' and variant == 'clipped')
          extra_suites += ['failing']
        endif
        test('compare ' + renderer_name + ' ' + testname + ' ' + variant, compare_render,
          args: [
            variants.get(variant),
            '--output', join_paths(meson.current_build_dir(), 'compare', renderer_name),
            join_paths(meson.current_source_dir(), 'compare', testname + '.node'),
            join_paths(meson.current_source_dir(), 'compare', testname + '.png'),
          ],
          env: test_env,
          suite: suites + extra_suites
        )
      endforeach
    endif
  endforeach
endforeach

node_parser_tests = [
  'blend.node',
  'blend-unknown-mode.errors',
  'blend-unknown-mode.node',
  'blend-unknown-mode.ref.node',
  'border.node',
  'color.node',
  'conic-gradient.node',
  'conic-gradient.ref.node',
  'crash1.errors',
  'crash1.node',
  'crash1.ref.node',
  'crash2.errors',
  'crash2.node',
  'crash2.ref.node',
  'crash3.node',
  'crash3.ref.node',
  'crash4.errors',
  'crash4.node',
  'crash4.ref.node',
  'debug.node',
  'debug-fail.node',
  'debug-fail.ref.node',
  'debug-fail.errors',
  'empty-blend.node',
  'empty-blend.ref.node',
  'empty-blur.node',
  'empty-blur.ref.node',
  'empty-border.node',
  'empty-border.ref.node',
  'empty-cairo.node',
  'empty-cairo.ref.node',
  'empty-clip.node',
  'empty-clip.ref.node',
  'empty-color.node',
  'empty-color.ref.node',
  'empty-color-matrix.node',
  'empty-color-matrix.ref.node',
  'empty-container.node',
  'empty-container.ref.node',
  'empty-cross-fade.node',
  'empty-cross-fade.ref.node',
  'empty-debug.node',
  'empty-debug.ref.node',
  'empty-fill.node',
  'empty-fill.ref.node',
  'empty-inset-shadow.node',
  'empty-inset-shadow.ref.node',
  'empty-linear-gradient.node',
  'empty-linear-gradient.ref.node',
  'empty-mask.node',
  'empty-mask.ref.node',
  'empty-opacity.node',
  'empty-opacity.ref.node',
  'empty-outset-shadow.node',
  'empty-outset-shadow.ref.node',
  'empty-repeat.node',
  'empty-repeat.ref.node',
  'empty-rounded-clip.node',
  'empty-rounded-clip.ref.node',
  'empty-shadow.node',
  'empty-shadow.ref.node',
  'empty-stroke.node',
  'empty-stroke.ref.node',
  'empty-subsurface.node',
  'empty-subsurface.ref.node',
  'empty-text.node',
  'empty-text.ref.node',
  'empty-texture.node',
  'empty-texture.ref.node',
  'empty-texture-scale.node',
  'empty-texture-scale.ref.node',
  'empty-transform.node',
  'empty-transform.ref.node',
  'fill.node',
  'fill.ref.node',
  'fill2.node',
  'fill2.ref.node',
  'glshader.node',
  'glshader.ref.node',
  'glshader.errors',
  'gradient-fail.node',
  'gradient-fail.ref.node',
  'gradient-fail.errors',
  'mask-modes.node',
  'mask-modes.ref.node',
  'node-names.node',
  'node-names-everywhere.errors',
  'node-names-everywhere.node',
  'node-names-everywhere.ref.node',
  'radial-gradient.node',
  'radial-gradient.ref.node',
  'repeating-linear-gradient.node',
  'repeating-linear-gradient.ref.node',
  'repeating-radial-gradient.node',
  'repeating-radial-gradient.ref.node',
  'rounded-rect.node',
  'shadow.node',
  'shadow-fail.node',
  'shadow-fail.ref.node',
  'shadow-fail.errors',
  'string-error.errors',
  'string-error.node',
  'string-error.ref.node',
  'stroke.node',
  'stroke.ref.node',
  'testswitch.node',
  'text.node',
  'text-color.node',
  'text-fail.node',
  'text-fail.ref.node',
  'text-fail.errors',
  'text-font-blob.node',
  'text-font-blob.ref.node',
  'text-font-errors.node',
  'text-font-errors.ref.node',
  'text-font-errors.errors',
  'text-font-options.node',
  'text-font-options.ref.node',
  'text-font-options-error.node',
  'text-font-options-error.ref.node',
  'text-font-options-error.ref.errors',
  'text-no-color.node',
  'texture-fail.node',
  'texture-fail.ref.node',
  'texture-fail.errors',
  'texture-names.node',
  'texture-scale-filters.node',
  'texture-scale-filters.ref.node',
  'texture-scale-unknown-filter.errors',
  'texture-scale-unknown-filter.node',
  'texture-scale-unknown-filter.ref.node',
  'transform-fail.node',
  'transform-fail.ref.node',
  'transform-fail.errors',
  # this fails in f40 since libpng changes the pngs while reencoding
  #'widgetfactory.node',
]

foreach test : node_parser_tests
  if test.endswith('.node') and not test.endswith('.ref.node')
    test('parser ' + test, node_parser,
      args: [
        join_paths(meson.current_source_dir(), 'nodeparser', test)
      ],
      env: [
        'GSK_RENDERER=opengl',
        'GTK_A11Y=test',
        'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
        'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
      ],
      protocol: 'exitcode',
      suite: [ 'gsk', 'gsk-nodeparser' ]
    )
  endif
endforeach

# offload does not work outside of linux
if os_linux
  offload = executable('offload', 'offload.c', 'gskrendernodeattach.c',
    dependencies : libgtk_static_dep,
    c_args: common_cflags,
  )

  offload_tests = [
    'simple.node',
    'transforms.node',
    'deep.node',
    'move.node',
    'start_offloading.node',
    'stop_offloading.node',
    'source.node',
    'nested.node',
    'clipped.node',
    'not-clipped.node',
    'complex-clip.node',
  ]

  foreach test : offload_tests
    if test.endswith('.node') and not test.endswith('.out')
      test('offload ' + test, offload,
        args: [
          join_paths(meson.current_source_dir(), 'offload', test)
        ],
        env: [
          'GSK_RENDERER=opengl',
          'GTK_A11Y=test',
          'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
          'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
        ],
        protocol: 'exitcode',
        suite: ['gsk', 'offload'],
      )
    endif
  endforeach
endif

tests = [
  [ 'normalize', [ 'normalize.c', '../reftests/reftest-compare.c' ] ],
  [ 'transform' ],
  [ 'shader' ],
  [ 'path', [ 'path-utils.c' ], [ 'flaky'] ],
  [ 'path-special-cases' ],
  [ 'scaling' ],
]

test_cargs = []

foreach t : tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_suites = t.get(2, [])

  test_exe = executable(test_name, test_srcs,
    dependencies : libgtk_dep,
    c_args : test_cargs + common_cflags,
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'GSK_RENDERER=cairo',
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: [ 'gsk' ] + test_extra_suites,
  )
endforeach

internal_tests = [
  [ 'boundingbox'],
  [ 'curve', [ ], [ 'flaky' ]],
  [ 'curve-special-cases' ],
  [ 'diff' ],
  [ 'half-float' ],
  [ 'misc'],
  [ 'path-private' ],
  [ 'rounded-rect'],
]

foreach t : internal_tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_suites = t.get(2, [])

  test_exe = executable(test_name,
    sources: test_srcs,
    dependencies : libgtk_static_dep,
    c_args : test_cargs + common_cflags + ['-DGTK_COMPILATION'],
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'GSK_RENDERER=cairo',
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: [ 'gsk' ] + test_extra_suites,
  )
endforeach


