desktop = 'gnome-users-panel.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

image_data = files(
  'data/faces/bicycle.jpg',
  'data/faces/book.jpg',
  'data/faces/calculator.jpg',
  'data/faces/cat.jpg',
  'data/faces/coffee2.jpg',
  'data/faces/flower2.jpg',
  'data/faces/gamepad.jpg',
  'data/faces/guitar2.jpg',
  'data/faces/headphones.jpg',
  'data/faces/hummingbird.jpg',
  'data/faces/mountain.jpg',
  'data/faces/plane.jpg',
  'data/faces/surfer.jpg',
  'data/faces/tomatoes.jpg',
  'data/faces/tree.jpg',
)

legacy_image_data = files(
  'data/faces/legacy/astronaut.jpg',
  'data/faces/legacy/baseball.png',
  'data/faces/legacy/butterfly.png',
  'data/faces/legacy/cat-eye.jpg',
  'data/faces/legacy/chess.jpg',
  'data/faces/legacy/coffee.jpg',
  'data/faces/legacy/dice.jpg',
  'data/faces/legacy/energy-arc.jpg',
  'data/faces/legacy/fish.jpg',
  'data/faces/legacy/flake.jpg',
  'data/faces/legacy/flower.jpg',
  'data/faces/legacy/grapes.jpg',
  'data/faces/legacy/guitar.jpg',
  'data/faces/legacy/launch.jpg',
  'data/faces/legacy/leaf.jpg',
  'data/faces/legacy/lightning.jpg',
  'data/faces/legacy/penguin.jpg',
  'data/faces/legacy/puppy.jpg',
  'data/faces/legacy/sky.jpg',
  'data/faces/legacy/soccerball.png',
  'data/faces/legacy/sunflower.jpg',
  'data/faces/legacy/sunset.jpg',
  'data/faces/legacy/tennis-ball.png',
  'data/faces/legacy/yellow-rose.jpg',
)

image_dir = join_paths(control_center_datadir, 'pixmaps', 'faces')

install_data(
  image_data,
  install_dir: image_dir
)

legacy_image_dir = join_paths(image_dir, 'legacy')

install_data(
  legacy_image_data,
  install_dir: legacy_image_dir
)

# create symlinks for legacy images to not break current images for people
meson.add_install_script('sh', '-c',
                         '''for f in $DESTDIR@0@/*; do
                          ln -sf legacy/$(basename $f) $DESTDIR@1@/$(basename $f);
                          done'''.format(legacy_image_dir, image_dir))

polkit = 'org.gnome.controlcenter.user-accounts.policy'
i18n.merge_file(
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

realmd_namespace = 'org.freedesktop.realmd'
sources += gnome.gdbus_codegen(
  'cc-realm-generated',
  'data/' + realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'CcRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

sources += gnome.mkenums_simple(
  'cc-user-accounts-enum-types',
  sources: ['cc-fingerprint-manager.h']
)

fprintd_namespace = 'net.reactivated.Fprint'
sources += gnome.gdbus_codegen(
  'cc-fprintd-generated',
  sources: [
    'data' / fprintd_namespace + '.Manager.xml',
    'data' / fprintd_namespace + '.Device.xml',
  ],
  interface_prefix: fprintd_namespace + '.',
  namespace: 'CcFprintd',
  autocleanup: 'all',
)

# Kerberos support
krb_dep = dependency('krb5', required: false)
assert(krb_dep.found(), 'kerberos libraries not found in your path')

cflags += [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
  '-DHAVE_LIBPWQUALITY',
  '-DUM_PIXMAP_DIR="@0@"'.format(join_paths(control_center_pkgdatadir, 'pixmaps'))
]

subdir('icons')
