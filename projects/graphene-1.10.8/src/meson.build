sources = [
  'graphene-alloc.c',
  'graphene-box.c',
  'graphene-euler.c',
  'graphene-frustum.c',
  'graphene-matrix.c',
  'graphene-plane.c',
  'graphene-point.c',
  'graphene-point3d.c',
  'graphene-quad.c',
  'graphene-quaternion.c',
  'graphene-ray.c',
  'graphene-rect.c',
  'graphene-size.c',
  'graphene-sphere.c',
  'graphene-triangle.c',
  'graphene-vectors.c'
]

simd_sources = [
  'graphene-simd4f.c',
  'graphene-simd4x4f.c',
]

if build_gobject
  sources += [ 'graphene-gobject.c' ]
endif

# Internal configuration header
configure_file(
  output: 'config.h',
  configuration: conf,
)

platform_deps = []

if build_gobject
  platform_deps += gobject
endif

libgraphene = library(
  graphene_api_path,
  include_directories: graphene_inc,
  sources: sources + simd_sources,
  version: libversion,
  soversion: soversion,
  darwin_versions: darwin_versions,
  install: true,
  dependencies: [ mathlib, threadlib ] + platform_deps,
  c_args: extra_args + common_cflags + debug_flags + [
    '-DGRAPHENE_COMPILATION',
  ],
  link_args: common_ldflags,
)

foreach simd: [ 'sse2', 'gcc', 'neon', 'scalar', ]
  set_variable('has_' + simd, graphene_simd.contains(simd) ? '1' : '0')
endforeach

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  libgraphene,
  name: 'Graphene',
  description: 'Math classes for graphic libraries',
  url: 'https://ebassi.github.io/graphene',
  filebase: graphene_api_path,
  variables: [
    'graphene_has_sse2=@0@'.format(has_sse2),
    'graphene_has_gcc=@0@'.format(has_gcc),
    'graphene_has_neon=@0@'.format(has_neon),
    'graphene_has_scalar=@0@'.format(has_scalar),
  ],
  subdirs: graphene_api_path,
  extra_cflags: [
    '-I${libdir}/@0@/include'.format(graphene_api_path),
  ] + sse2_cflags + neon_cflags,
)

if build_gobject
  pkgconfig.generate(
    name: 'Graphene-GObject',
    description: 'GObject types for Graphene',
    filebase: graphene_gobject_api_path,
    variables: [
      'graphene_has_sse2=@0@'.format(has_sse2),
      'graphene_has_gcc=@0@'.format(has_gcc),
      'graphene_has_neon=@0@'.format(has_neon),
      'graphene_has_scalar=@0@'.format(has_scalar),
    ],
    subdirs: graphene_api_path,
    requires: [graphene_api_path, 'gobject-2.0'],
  )
endif


graphene_dep_sources = []

# Introspection
if build_gir
  python = python.find_installation('python3')
  identfilter_py = join_paths(meson.current_source_dir(), 'identfilter.py')

  gir_extra_args = [
    '--identifier-filter-cmd=@0@ @1@'.format(python.full_path(), identfilter_py),
    '--accept-unprefixed',
    '--quiet',
    '--warn-all',
    '-DGRAPHENE_COMPILATION',
  ]

  graphene_gir = gnome.generate_gir(libgraphene,
    include_directories: graphene_inc,
    sources: [graphene_config_h] + sources + graphene_public_headers,
    namespace: 'Graphene',
    nsversion: graphene_api_version,
    identifier_prefix: 'Graphene',
    symbol_prefix: 'graphene',
    export_packages: graphene_gobject_api_path,
    includes: [ 'GObject-2.0' ],
    header: 'graphene-gobject.h',
    install: true,
    extra_args: gir_extra_args,
  )
  graphene_dep_sources += graphene_gir
endif

# Dependency for tests and subproject usage
graphene_dep = declare_dependency(
  sources: graphene_dep_sources,
  link_with: libgraphene,
  include_directories: [ graphene_inc ],
  dependencies: [ mathlib, threadlib ] + platform_deps,
)

# In case of subproject usage expose a separate variable for graphene-gobject
# support, so the superproject and sibling subprojects can check for this
# directly as part of the dependency() fallback logic instead of having to
# poke at the build_gobject subproject variable. This mirrors the pkg-config
# lookup logic where we have a dedicated .pc file for graphene-gobject as well.
if build_gobject
  graphene_gobject_dep = graphene_dep
else
  graphene_gobject_dep = dependency('', required: false)
endif
