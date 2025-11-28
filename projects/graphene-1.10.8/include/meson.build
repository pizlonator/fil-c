graphene_public_headers = files([
  'graphene-box.h',
  'graphene-euler.h',
  'graphene-frustum.h',
  'graphene-macros.h',
  'graphene-matrix.h',
  'graphene-plane.h',
  'graphene-point.h',
  'graphene-point3d.h',
  'graphene-quad.h',
  'graphene-quaternion.h',
  'graphene-ray.h',
  'graphene-rect.h',
  'graphene-size.h',
  'graphene-sphere.h',
  'graphene-triangle.h',
  'graphene-types.h',
  'graphene-vec2.h',
  'graphene-vec3.h',
  'graphene-vec4.h',
  'graphene-version-macros.h',
])

graphene_simd_headers = files([
  'graphene-simd4f.h',
  'graphene-simd4x4f.h',
])

if build_gobject
  graphene_public_headers += files('graphene-gobject.h')
endif

# External configuration header
graphene_config_h = configure_file(
  input: 'graphene-config.h.meson',
  output: 'graphene-config.h',
  configuration: graphene_conf,
  install_dir: join_paths(graphene_libdir, graphene_api_path, 'include'),
)

# Version header
version_conf = configuration_data()
version_conf.set('GRAPHENE_VERSION', graphene_version)
version_conf.set('GRAPHENE_MAJOR_VERSION', graphene_major_version)
version_conf.set('GRAPHENE_MINOR_VERSION', graphene_minor_version)
version_conf.set('GRAPHENE_MICRO_VERSION', graphene_micro_version)

configure_file(
  input: 'graphene-version.h.meson',
  output: 'graphene-version.h',
  configuration: version_conf,
  install_dir: join_paths(graphene_includedir, graphene_api_path),
)

install_headers(
  graphene_public_headers + graphene_simd_headers + [ 'graphene.h' ],
  subdir: graphene_api_path,
)

graphene_inc = include_directories('.')
