foreach prog: ['xsh',
               'vpass',
               'blank',
               'check_user',
               'tty_conv'
              ]
  executable(
    prog,
    sources: prog + '.c',
    link_args: exe_link_args,
    include_directories: [libpamc_inc],
    dependencies: [libpam_dep, libpam_misc_dep],
  )
endforeach
