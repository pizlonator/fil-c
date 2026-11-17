setxkbmap = find_program('setxkbmap', required: false)
Xvfb = find_program('Xvfb', required: false)

subdir('common')
#subdir('datetime')
if host_is_linux
  subdir('network')
endif

# FIXME: this is a workaround because interactive-tests don't work with libadwaita as a subproject. See !1754
if not libadwaita_is_subproject
  subdir('interactive-panels')
endif

subdir('printers')
subdir('keyboard')
