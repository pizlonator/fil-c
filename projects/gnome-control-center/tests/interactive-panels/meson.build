subdir('applications')

###########
# Sources #
###########

sources = files(
  'gtp-dynamic-panel.c',
  'gtp-static-init.c',
  'main.c',
)


##############
# GResources #
##############

sources += gnome.compile_resources(
  'test-panels-resources',
   'panels.gresource.xml',
              source_dir : '.',
                  c_name : 'test_panels',
                  export : true,
)


######################
# interactive-panels #
######################

includes = [top_inc]

exe = executable(
  'test-interactive-panels',
               sources,
  include_directories : includes,
         dependencies : shell_deps + [libtestshell_dep],
               c_args : cflags
)
