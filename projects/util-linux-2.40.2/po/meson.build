if not find_program('gettext', required : false).found()
  subdir_done()
endif

i18n = import('i18n')
i18n.gettext(meson.project_name(),
             preset : 'glib',
             data_dirs : '.')
