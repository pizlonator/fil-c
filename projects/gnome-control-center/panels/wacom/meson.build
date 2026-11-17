deps = common_deps + wacom_deps + [
  gnome_rr_dep,
  gnome_settings_dep,
  liblanguage_dep,
  x11_dep,
  xi_dep
]

cflags += ['-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)]

test_cflags = cflags + ['-DFAKE_AREA']

wacom_gresource = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
    source_dir : '.',
        c_name : 'cc_' + cappletname,
        export : true
)

subdir('calibrator')

panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
         type : 'desktop',
        input : desktop + '.in',
       output : desktop,
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
)

common_sources = files(
  'cc-tablet-tool-map.c',
  'cc-wacom-button-row.c',
  'cc-wacom-device.c',
  'cc-wacom-page.c',
  'cc-wacom-stylus-action-dialog.c',
  'cc-wacom-stylus-page.c',
  'cc-wacom-tool.c',
  'gsd-wacom-key-shortcut-button.c'
)

common_sources += wacom_gresource

sources = common_sources + files(
  'cc-' + cappletname + '-panel.c',
  'cc-drawing-area.c',
  'cc-wacom-ekr-page.c',
)

deps += libdevice_dep

incs = [
  top_inc,
  calibrator_inc
]

panels_libs += static_library(
  cappletname + '-properties',
              sources : sources,
  include_directories : incs,
         dependencies : deps,
               c_args : cflags,
            link_with : [ libwacom_calibrator ]
)

name = 'test-wacom'

sources = common_sources + files(name + '.c')

executable(
                  name,
               sources,
  include_directories : incs,
         dependencies : deps,
               c_args : test_cflags,
            link_with : [ libwacom_calibrator_test ]
)

subdir('icons')
