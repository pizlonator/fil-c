subdir('cursor')

gdk_wayland_sources = files([
  'gdkapplaunchcontext-wayland.c',
  'gdkcairocontext-wayland.c',
  'gdkclipboard-wayland.c',
  'gdkcursor-wayland.c',
  'gdkdevice-wayland.c',
  'gdkdevicepad-wayland.c',
  'gdkdisplay-wayland.c',
  'gdkdrag-wayland.c',
  'gdkdragsurface-wayland.c',
  'gdkdrop-wayland.c',
  'gdkeventsource.c',
  'gdkglcontext-wayland.c',
  'gdkkeymap-wayland.c',
  'gdkmonitor-wayland.c',
  'gdkprimary-wayland.c',
  'gdkseat-wayland.c',
  'gdksurface-wayland.c',
  'gdksubsurface-wayland.c',
  'gdktoplevel-wayland.c',
  'gdkpopup-wayland.c',
  'gdkvulkancontext-wayland.c',
  'gdkwaylandpresentationtime.c',
  'wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'gdkwaylanddevice.h',
  'gdkwaylanddisplay.h',
  'gdkwaylandglcontext.h',
  'gdkwaylandmonitor.h',
  'gdkwaylandpopup.h',
  'gdkwaylandseat.h',
  'gdkwaylandsurface.h',
  'gdkwaylandtoplevel.h',
])

install_headers(gdk_wayland_public_headers, 'gdkwayland.h', subdir: 'gtk-4.0/gdk/wayland/')

gdk_wayland_deps = [
  shmlib,
  xkbdep,
  wlclientdep,
  wlprotocolsdep,
  wlegldep,
]

wayland_scanner = find_program('wayland-scanner')

# Fields:
#  - name: protocol name
#  - stability: protocol stability ('private', 'stable' or 'unstable')
#  - version: protocol version
#  - required: wayland_protocols version check
proto_sources = [
  {
    'name': 'gtk-shell',
    'stability': 'private',
  },
  {
    'name': 'primary-selection',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'pointer-gestures',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'viewporter',
    'stability': 'stable',
  },
  {
    'name': 'xdg-shell',
    'stability': 'unstable',
    'version': 6,
  },
  {
    'name': 'xdg-shell',
    'stability': 'stable',
  },
  {
    'name': 'xdg-foreign',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'xdg-foreign',
    'stability': 'unstable',
    'version': 2,
  },
  {
    'name': 'tablet',
    'stability': 'unstable',
    'version': 2,
  },
  {
    'name': 'keyboard-shortcuts-inhibit',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'server-decoration',
    'stability': 'private',
  },
  {
    'name': 'xdg-output',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'idle-inhibit',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'xdg-activation',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'fractional-scale',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'linux-dmabuf',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'presentation-time',
    'stability': 'stable',
    'version': 1,
  },
  {
    'name': 'xdg-dialog',
    'stability': 'staging',
    'version': 1,
    'required': '>=1.36',
  },
  {
    'name': 'xdg-dialog',
    'stability': 'private',
    'version': 1,
    'required': '<1.36',
  },
]

gdk_wayland_gen_headers = []

foreach p: proto_sources
  proto_name = p.get('name')
  proto_stability = p.get('stability')

  if wlprotocolsdep.version().version_compare(p.get('required', '>=0'))
    if proto_stability == 'stable'
      output_base = proto_name
      input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
    elif proto_stability == 'staging'
      proto_version = p.get('version')
      output_base = '@0@-v@1@'.format(proto_name, proto_version)
      input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
    elif proto_stability == 'private'
      if p.has_key('version')
        output_base = '@0@-v@1@'.format(proto_name, p.get('version'))
      else
        output_base = proto_name
      endif
      input = files('protocol/@0@.xml'.format(output_base))
    else
      proto_version = p.get('version')
      output_base = '@0@-@1@-v@2@'.format(proto_name, proto_stability, proto_version)
      input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
    endif

    gdk_wayland_gen_headers += custom_target('@0@ client header'.format(output_base),
      input: input,
      output: '@0@-client-protocol.h'.format(output_base),
      command: [
        wayland_scanner,
        'client-header',
        '@INPUT@', '@OUTPUT@',
      ],
    )

    gdk_wayland_sources += custom_target('@0@ source'.format(output_base),
      input: input,
      output: '@0@-protocol.c'.format(output_base),
      command: [
        wayland_scanner,
        'private-code',
        '@INPUT@', '@OUTPUT@',
      ],
    )
  endif
endforeach

libgdk_wayland = static_library('gdk-wayland',
  sources: [ gdk_wayland_sources, gdk_wayland_gen_headers, gdk_gen_headers ],
  include_directories: [ confinc, gdkinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  link_with: [ libwayland_cursor, ],
  dependencies: [ gdk_deps, gdk_wayland_deps ],
)

# Used to generate pkg-config Requires
wayland_public_deps = [wlclientdep]
