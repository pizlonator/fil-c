xtest_progs = [
  'tst-pam_access1',
  'tst-pam_access2',
  'tst-pam_access3',
  'tst-pam_access4',
  'tst-pam_authfail',
  'tst-pam_authsucceed',
  'tst-pam_dispatch1',
  'tst-pam_dispatch2',
  'tst-pam_dispatch3',
  'tst-pam_dispatch4',
  'tst-pam_dispatch5',
  'tst-pam_group1',
  'tst-pam_limits1',
  'tst-pam_motd',
  'tst-pam_pwhistory1',
  'tst-pam_shells',
  'tst-pam_succeed_if1',
  'tst-pam_time1',
  'tst-pam_unix1',
  'tst-pam_unix2',
  'tst-pam_unix3',
  'tst-pam_unix4',
]

foreach prog: xtest_progs
  tst_exe = executable(
    prog,
    sources: prog + '.c',
    c_args: ['-DLIBPAM_COMPILE'],
    link_args: exe_link_args,
    include_directories: [libpamc_inc],
    dependencies: [libpam_dep, libpam_misc_dep],
  )

  test(
    'xtests ' + prog,
    chdir_meson_build_subdir,
    args: [
      files('run-xtests.sh'),
      meson.current_source_dir(),
      fs.name(tst_exe),
    ],
    env: ['MESON_BUILD_SUBDIR=' + meson.current_build_dir()],
    is_parallel: false,
  )
endforeach

xtest_scripts = [
  'tst-pam_assemble_line1',
  'tst-pam_substack1',
  'tst-pam_substack2',
  'tst-pam_substack3',
  'tst-pam_substack4',
  'tst-pam_substack5',
]

foreach name: xtest_scripts
  test(
    'xtests ' + name,
    chdir_meson_build_subdir,
    args: [
      files('run-xtests.sh'),
      meson.current_source_dir(),
      name,
    ],
    env: ['MESON_BUILD_SUBDIR=' + meson.current_build_dir()],
    is_parallel: false,
  )
endforeach
