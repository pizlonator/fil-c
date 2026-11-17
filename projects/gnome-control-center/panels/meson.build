subdir('common')

# Fil-C: panels whose external dependencies are not part of the pizlonated
# system are dropped here (the panel-loader entries in
# shell/cc-panel-loader.c are guarded by the same HAVE_* macros):
# - color             (colord-gtk4)
# - online-accounts   (goa-1.0 / goa-backend-1.0)
# - printers          (cups, smbclient)
# - privacy           (libgtop-2.0)
# - sound             (gsound)
# - system            (libgtop-2.0, udisks2, libpwquality, krb5)
# - wwan / network    (libnm, libnma-gtk4, mm-glib)
# - bluetooth / wacom (gnome-bluetooth-ui-3.0, libwacom)
panels = [
  'applications',
  'background',
  'display',
  'keyboard',
  'mouse',
  'multitasking',
  'notifications',
  'power',
  'search',
  'sharing',
  'universal-access',
]

panels_list = []
panels_libs = []
foreach cappletname: panels
  cflags = [
    '-DG_LOG_DOMAIN="cc-@0@-panel"'.format(cappletname),
    '-DPANEL_ID="@0@"'.format(cappletname)
  ]

  subdir(cappletname)
endforeach
