#################
# Desktop files #
#################

desktop_files = [
  'dynamic-panel',
  'static-init',
]

foreach desktop_file : desktop_files
  i18n.merge_file(
                type : 'desktop',
               input : 'gtp-@0@.desktop.in'.format(desktop_file),
              output : 'gnome-@0@-panel.desktop'.format(desktop_file),
              po_dir : po_dir,
    build_by_default : true,
             install : false,
  )
endforeach
