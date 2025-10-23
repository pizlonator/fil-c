foreach name: ['dlopen',
               'pam_start',
               'pam_end',
               'pam_fail_delay',
               'pam_open_session',
               'pam_close_session',
               'pam_acct_mgmt',
               'pam_authenticate',
               'pam_chauthtok',
               'pam_setcred',
               'pam_get_authtok',
               'pam_get_item',
               'pam_set_item',
               'pam_getenvlist',
               'pam_get_user',
               'pam_get_data',
               'pam_set_data',
               'pam_mkargv',
               'pam_start_confdir',
              ]
  tst_deps = [libpam_internal_dep, libpam_dep]
  if name == 'dlopen'
    tst_deps += libdl
  endif
  tst_name = 'tst-' + name

  tst_exe = executable(
    tst_name,
    sources: tst_name + '.c',
    dependencies: tst_deps,
    include_directories: [libpam_private_inc],
    c_args: ['-DLIBPAM_COMPILE'],
    link_args: exe_link_args,
  )

  if name == 'dlopen'
    tst_dlopen = tst_exe
  endif

  test(
    tst_name,
    chdir_meson_build_subdir,
    args: [tst_exe],
    env: ['MESON_BUILD_SUBDIR=' + meson.current_build_dir(),
          'srcdir=' + meson.current_source_dir()
         ],
  )
endforeach
