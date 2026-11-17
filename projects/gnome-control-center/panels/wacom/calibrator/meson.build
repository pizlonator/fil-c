calibrator_inc = include_directories('.')

common_sources = files(
  'calibrator.c',
  'calibrator-gui.c',
  'cc-clock.c',
)

calibrator_deps = deps + [libdevice_dep, m_dep]

libwacom_calibrator = static_library(
  cappletname + '-calibrator',
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: cflags
)

libwacom_calibrator_test = static_library(
  cappletname + '-calibrator-test',
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: test_cflags
)

sources = common_sources + wacom_gresource + files('main.c')

executable(
  'test-calibrator',
  sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: cflags
)
