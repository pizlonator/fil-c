if not find_program('gettext', required: get_option('i18n'), native: true).found()
  subdir_done()
endif

i18n = import('i18n')
i18n.gettext(
  meson.project_name(),
  preset: 'glib',
  data_dirs: '.',
  args: [
    '--copyright-holder=Linux-PAM Project',
    '--msgid-bugs-address=https://github.com/linux-pam/linux-pam/issues',
    '--package-version=1.7.1',
  ],
)
