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

sources = files(
  'cc-applications-panel.c',
  'cc-applications-row.c',
  'cc-info-row.c',
  'cc-default-apps-page.c',
  'cc-default-apps-row.c',
  'cc-removable-media-settings.c',
  'globs.c',
  'search.c',
  'utils.c',
)

resource_data = files(
  'cc-applications-panel.ui',
  'cc-default-apps-page.ui',
  'cc-removable-media-settings.ui',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  dependencies: resource_data,
  export : true
)

deps = common_deps

# Fil-C: mm-glib is optional in the pizlonated system (NetworkManager is
# not built).
if host_is_linux and mm_dep.found()
  deps += mm_dep
endif

if enable_snap
  deps += [ json_glib_dep, libsoup_dep ]
  sources += [ 'cc-snap-row.c', 'cc-snapd-client.c' ]
endif

if enable_malcontent
  deps += malcontent_dep
endif

panels_libs += static_library(
           cappletname,
              sources : sources,
  include_directories : [ top_inc, common_inc ],
         dependencies : deps,
               c_args : cflags
)

subdir('icons')
