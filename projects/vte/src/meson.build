# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

subdir('vte')

src_inc = include_directories('.')

a11y_gtk3_sources = files(
  'vteaccess.cc',
  'vteaccess.h',
)

a11y_gtk4_sources = files(
  'vteaccess-gtk4.cc',
  'vteaccess-gtk4.h',
)

color_sources = files(
  'color.hh',
  'color-names.hh',
  'color-parser.cc',
  'color-parser.hh',
)

debug_sources = files(
  'debug.cc',
  'debug.h',
)

glib_glue_sources = files(
  'glib-glue.cc',
  'glib-glue.hh',
)

graphene_glue_sources = files(
  'graphene-glue.hh',
)

gtk_glue_sources = files(
  'gtk-glue.hh',
)

icu_sources = files(
  'icu-converter.cc',
  'icu-converter.hh',
  'icu-decoder.cc',
  'icu-decoder.hh',
  'icu-glue.cc',
  'icu-glue.hh',
)

libc_glue_sources = files(
  'libc-glue.hh',
)

minifont_coverage_sources = custom_target(
  'minifont-coverage.inc',
  input: files(
    'minifont.cc',
  ),
  capture: false,
  command: [
    python,
    files('minifont-coverage.py'),
    '--output', '@OUTPUT@',
    '@INPUT@',
  ],
  install: false,
  output: 'minifont-coverage.inc',
)

modes_sources = files(
  'modes.hh',
)

modes_sources += custom_target(
  'modes',
  command: [
    python,
    files('modes.py'),
    '--destdir', '@OUTDIR@',
  ],
  input: [],
  install: false,
  output: [
    'modes-dec.hh',
    'modes-ecma.hh',
  ],
)

pango_glue_sources = files(
  'pango-glue.hh',
)

parser_sources = files(
  'parser-arg.hh',
  'parser-charset-tables.hh',
  'parser-charset.hh',
  'parser-decsgr.hh',
  'parser-glue.hh',
  'parser-osc.hh',
  'parser-reply.hh',
  'parser-sgr.hh',
  'parser-string.hh',
  'parser.cc',
  'parser.hh',
)

parser_sources += custom_target(
  'parser-seq',
  command: [
    python,
    files('parser-seq.py'),
    '--destdir', '@OUTDIR@',
  ],
  input: [],
  install: false,
  output: [
    'parser-c01.hh',
    'parser-cmd.hh',
    'parser-csi.hh',
    'parser-dcs.hh',
    'parser-esc.hh',
    'parser-sci.hh',
  ],
)

pastify_sources = files(
  'pastify.cc',
  'pastify.hh',
)

pcre2_glue_sources = files(
  'pcre2-glue.hh',
)

pty_sources = files(
  'pty.cc',
  'pty.hh',
  'vtepty.cc',
  'vteptyinternal.hh',
)

refptr_sources = files(
  'refptr.hh',
)

regex_sources = files(
  'regex.cc',
  'regex.hh'
)

std_glue_sources = files(
  'std-glue.hh',
)

systemd_sources = files(
  'systemd.cc',
  'systemd.hh',
)

termprop_sources = files(
  'termprops.hh',
)

unicode_width_sources = files(
  'unicode-width.hh',
)

utf8_sources = files(
  'utf8.cc',
  'utf8.hh',
)

uuid_sources = files(
  'uuid.cc',
  'uuid.hh',
)

vte_uuid_sources = files(
  'vteuuid.cc',
  'vteuuidinternal.hh',
)

vte_glue_sources = files(
  'vte-glue.hh',
)

libvte_common_sources = color_sources + config_sources + debug_sources + glib_glue_sources + gtk_glue_sources + libc_glue_sources + minifont_coverage_sources + modes_sources + pango_glue_sources + parser_sources + pastify_sources + pcre2_glue_sources + pty_sources + refptr_sources + regex_sources + std_glue_sources + termprop_sources + utf8_sources + uuid_sources + vte_uuid_sources + vte_glue_sources + files(
  'attr.hh',
  'bidi.cc',
  'bidi.hh',
  'box-drawing.hh',
  'buffer.h',
  'cairo-glue.hh',
  'caps.hh',
  'cell.hh',
  'chunk.cc',
  'chunk.hh',
  'clipboard-gtk.cc',
  'clipboard-gtk.hh',
  'color-palette.hh',
  'color-triple.hh',
  'cxx-utils.hh',
  'drawing-context.cc',
  'drawing-context.hh',
  'fonts-pangocairo.cc',
  'fonts-pangocairo.hh',
  'gobject-glue.hh',
  'keymap.cc',
  'keymap.h',
  'minifont.cc',
  'minifont.hh',
  'missing.cc',
  'missing.hh',
  'osc-colors.hh',
  'osc-colors.cc',
  'reaper.cc',
  'reaper.hh',
  'rect.hh',
  'ring.cc',
  'ring.hh',
  'ringview.cc',
  'ringview.hh',
  'scheduler.cc',
  'scheduler.h',
  'sgr.hh',
  'spawn.cc',
  'spawn.hh',
  'unicode-width.hh',
  'utf8.cc',
  'utf8.hh',
  'vte.cc',
  'vtedefines.hh',
  'vtegtk.cc',
  'vtegtk.hh',
  'vteinternal.hh',
  'vteregex.cc',
  'vteregexinternal.hh',
  'vterowdata.cc',
  'vterowdata.hh',
  'vteseq.cc',
  'vtespawn.cc',
  'vtespawn.hh',
  'vtestream-base.h',
  'vtestream-file.h',
  'vtestream.cc',
  'vtestream.h',
  'vtetypes.cc',
  'vtetypes.hh',
  'vteunistr.cc',
  'vteunistr.h',
  'vteutils.cc',
  'vteutils.h',
  'widget.cc',
  'widget.hh',
)

libvte_gtk3_drawing_sources = files(
  'drawing-cairo.cc',
  'drawing-cairo.hh',
)

libvte_gtk4_drawing_sources = files(
  'drawing-gsk.cc',
  'drawing-gsk.hh',
)

if get_option('icu')
  libvte_common_sources += icu_sources
endif

if systemd_dep.found()
  libvte_common_sources += systemd_sources
endif

libvte_common_doc_sources = files(
  # These file contain gtk-doc comments to be extracted for docs and gir
  'vtegtk.cc',
  'vtepty.cc',
  'vteregex.cc',
)

libvte_common_sources += gnome.compile_resources(
  'vteresources',
  'vte.gresource.xml',
  c_name: '_vte',
)

libvte_common_sources += gnome.genmarshal(
  'marshal',
  sources: 'marshal.list',
  prefix: '_vte_marshal',
  internal: true,
  valist_marshallers: true,
)

libvte_common_public_deps = [
  cairo_dep,
  gio_dep,
  glib_dep,
  gobject_dep,
  pango_dep,
]

libvte_common_deps = libvte_common_public_deps + [
  cairo_gobject_dep,
  fribidi_dep,
  gnutls_dep,
  icu_dep,
  pcre2_dep,
  libm_dep,
  liblz4_dep,
  pthreads_dep,
  systemd_dep,
]

incs = [
  top_inc,
  vte_inc,
]

libvte_common_cppflags = [
  '-DG_LOG_DOMAIN="VTE"',
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
  '-DVTE_COMPILATION',
  '-UPARSER_INCLUDE_NOP',
]

if get_option('gtk3')
  libvte_gtk3_sources = libvte_common_sources + libvte_gtk3_public_headers + libvte_gtk3_enum_sources + libvte_gtk3_drawing_sources
  libvte_gtk3_cppflags = libvte_common_cppflags + gtk3_version_cppflags + ['-DVTE_GTK=3',] + release_args
  libvte_gtk3_deps = libvte_common_deps + [gtk3_dep,]
  libvte_gtk3_public_deps = libvte_common_public_deps + [gtk3_dep,]

  if get_option('a11y')
    libvte_gtk3_sources += a11y_gtk3_sources
  endif

  libvte_gtk3 = shared_library(
    vte_gtk3_api_name,
    sources: libvte_gtk3_sources,
    version: libvte_gtk3_soversion,
    include_directories: incs,
    dependencies: libvte_gtk3_deps,
    cpp_args: libvte_gtk3_cppflags,
    install: true,
  )

  libvte_gtk3_dep = declare_dependency(
    sources: libvte_gtk3_public_headers,
    include_directories: [src_inc, vte_inc,],
    dependencies: libvte_gtk3_deps,
    link_with: libvte_gtk3
  )

  pkg.generate(
    libvte_gtk3,
    version: vte_version,
    name: 'vte',
    description: 'VTE widget for GTK+ 3.0',
    filebase: vte_gtk3_api_name,
    subdirs: vte_gtk3_api_name,
    requires: libvte_gtk3_public_deps,
    variables: 'exec_prefix=${prefix}',
  )
endif

if get_option('gtk4')

  libvte_gtk4_resources_data = files(
  )

  libvte_gtk4_resources_sources = gnome.compile_resources(
    'vteresources-gtk4.cc',
    'vte-gtk4.gresource.xml',
    c_name: 'vte',
    dependencies: libvte_gtk4_resources_data,
  )

  libvte_gtk4_sources = libvte_common_sources + libvte_gtk4_public_headers + libvte_gtk4_enum_sources + libvte_gtk4_resources_sources + graphene_glue_sources + libvte_gtk4_drawing_sources
  libvte_gtk4_cppflags = libvte_common_cppflags + gtk4_version_cppflags + ['-DVTE_GTK=4',] + release_args
  libvte_gtk4_deps = libvte_common_deps + [gtk4_dep,]
  libvte_gtk4_public_deps = libvte_common_public_deps + [gtk4_dep,]

  if get_option('a11y')
    libvte_gtk4_sources += a11y_gtk4_sources
  endif

  libvte_gtk4 = shared_library(
    vte_gtk4_api_name,
    sources: libvte_gtk4_sources,
    version: libvte_gtk4_soversion,
    include_directories: incs,
    dependencies: libvte_gtk4_deps,
    cpp_args: libvte_gtk4_cppflags,
    install: true,
  )

  libvte_gtk4_dep = declare_dependency(
    sources: libvte_gtk4_public_headers,
    include_directories: [src_inc, vte_inc,],
    dependencies: libvte_gtk4_deps,
    link_with: libvte_gtk4
  )

  pkg.generate(
    libvte_gtk4,
    version: vte_version,
    name: 'vte',
    description: 'VTE widget for GTK+ 4.0',
    filebase: vte_gtk4_api_name,
    subdirs: vte_gtk4_api_name,
    requires: libvte_gtk4_public_deps,
    variables: 'exec_prefix=${prefix}',
  )
endif

## Tests

# decoder cat

decoder_cat_sources = config_sources + glib_glue_sources + libc_glue_sources + utf8_sources + debug_sources + files(
  'decoder-cat.cc',
)

if get_option('icu')
  decoder_cat_sources += icu_sources
endif

decoder_cat = executable(
  'decoder-cat',
  decoder_cat_sources,
  dependencies: [glib_dep, icu_dep,],
  cpp_args: [],
  include_directories: top_inc,
  install: false,
)

# parser cat

parser_cat_sources = config_sources + glib_glue_sources + libc_glue_sources + parser_sources + std_glue_sources + utf8_sources + debug_sources + files(
  'parser-cat.cc',
  'vtedefines.hh',
)

parser_cat = executable(
  'parser-cat',
  parser_cat_sources,
  dependencies: [glib_dep,],
  cpp_args: ['-DPARSER_INCLUDE_NOP'],
  include_directories: top_inc,
  install: false,
)

# dumpkeys

dumpkeys_sources = config_sources + files(
  'dumpkeys.c'
)

dumpkeys = executable(
  'dumpkeys',
  sources: dumpkeys_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# mev

mev_sources = config_sources + files(
  'mev.c'
)

mev = executable(
  'mev',
  sources: mev_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# slowcat

slowcat_sources = config_sources + files(
  'slowcat.c'
)

slowcat = executable(
  'slowcat',
  sources: slowcat_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# reflect

if get_option('gtk3')

  reflect_sources = config_sources + files(
    'reflect.c'
  )

  reflect_textview = executable(
    'reflect-textview',
    sources: reflect_sources,
    dependencies: [gtk3_dep],
    c_args: ['-DUSE_TEXT_VIEW'],
    include_directories: top_inc,
    install: false,
  )

  reflect_vte = executable(
    'reflect-vte',
    sources: reflect_sources,
    dependencies: [gtk3_dep, libvte_gtk3_dep,],
    c_args: [
      '-DUSE_VTE',
      '-DVTE_DISABLE_DEPRECATION_WARNINGS',
    ],
    include_directories: [top_inc,],
    install: false,
  )

endif

# vte-urlencode-cwd

vte_urlencode_cwd_sources = config_sources + files(
  'urlencode.cc',
)

vte_urlencode_cwd = executable(
  'vte-urlencode-cwd',
  cpp_args: [
    '-nostdlib',
  ],
  include_directories: [top_inc,],
  install: true,
  install_dir: vte_libexecdir,
  sources: vte_urlencode_cwd_sources,
)

# xticker

xticker_sources = config_sources + files(
  'xticker.c'
)

xticker = executable(
  'xticker',
  sources: xticker_sources,
  dependencies: [glib_dep],
  c_args: ['-DVTE_DISABLE_DEPRECATION_WARNINGS',],
  include_directories: top_inc,
  install: false,
)

# Unit tests

test_colors_sources = config_sources + debug_sources + glib_glue_sources + color_sources + files(
  'color-test.cc',
)

test_colors = executable(
  'test-colors',
  cpp_args: ['-DVTE_GTK=0'],
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
  sources: test_colors_sources,
)

test_minifont_sources = config_sources + minifont_coverage_sources + files(
  'minifont-test.cc'
)

test_minifont_sources += custom_target(
  'minifont-coverage-tests.inc',
  input: files(
    'minifont.cc',
  ),
  capture: false,
  command: [
    python,
    files('minifont-coverage.py'),
    '--output', '@OUTPUT@',
    '--tests',
    '@INPUT@',
  ],
  install: false,
  output: 'minifont-coverage-tests.inc',
)

test_minifont = executable(
  'test-minifont',
  sources: test_minifont_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_modes_sources = config_sources + modes_sources + files(
  'modes-test.cc',
)

test_modes = executable(
  'test-modes',
  sources: test_modes_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_parser_sources = config_sources + parser_sources + files(
  'parser-test.cc',
)

test_parser = executable(
  'test-parser',
  sources: test_parser_sources,
  dependencies: [glib_dep],
  cpp_args: ['-DPARSER_INCLUDE_NOP'],
  include_directories: top_inc,
  install: false,
)

test_pastify_sources = config_sources + pastify_sources + files(
  'pastify-test.cc',
)

test_pastify = executable(
  'test-pastify',
  sources: test_pastify_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_reaper_sources = config_sources + debug_sources + files(
  'reaper.cc',
  'reaper.hh'
)

test_reaper = executable(
  'test-reaper',
  sources: test_reaper_sources,
  dependencies: [gobject_dep],
  cpp_args: ['-DMAIN'],
  include_directories: top_inc,
  install: false,
)

test_refptr_sources = config_sources + files(
  'refptr-test.cc',
  'refptr.hh'
)

test_refptr = executable(
  'test-refptr',
  sources: test_refptr_sources,
  dependencies: [glib_dep, gobject_dep],
  include_directories: top_inc,
  install: false,
)

test_stream_sources = config_sources + files(
  'vtestream-base.h',
  'vtestream-file.h',
  'vtestream.cc',
  'vtestream.h',
  'vteutils.cc',
  'vteutils.h',
)

test_stream = executable(
  'test-stream',
  sources: test_stream_sources,
  dependencies: [gio_dep, gnutls_dep, liblz4_dep],
  cpp_args: ['-DVTESTREAM_MAIN'],
  include_directories: top_inc,
  install: false,
)

test_tabstops_sources = config_sources + files(
  'tabstops-test.cc',
  'tabstops.hh'
)

test_tabstops = executable(
  'test-tabstops',
  sources: test_tabstops_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_termprops_sources = color_sources + config_sources + debug_sources + glib_glue_sources + termprop_sources + uuid_sources + files(
  'termprops-test.cc',
)

test_termprops = executable(
  'test-termprops',
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
  sources: test_termprops_sources,
)

test_unicode_width_sources = config_sources + unicode_width_sources + files(
  'unicode-width-test.cc',
)

test_unicode_width = executable(
  'test-unicode-width',
  sources: test_unicode_width_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_utf8_sources = config_sources + utf8_sources + files(
  'utf8-test.cc',
)

test_utf8 = executable(
  'test-utf8',
  sources: test_utf8_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_uuid_sources = config_sources + debug_sources + glib_glue_sources + uuid_sources + files(
  'uuid-test.cc',
)

test_uuid = executable(
  'test-uuid',
  sources: test_uuid_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_vtetypes_sources = config_sources + libc_glue_sources + files(
   'vtetypes.cc',
   'vtetypes.hh',
)

if get_option('gtk3')
  test_vtetypes = executable(
    'test-vtetypes',
    sources: test_vtetypes_sources,
    dependencies: [glib_dep, pango_dep, gtk3_dep,],
    cpp_args: ['-DMAIN'],
    include_directories: top_inc,
    install: false,
  )
endif

test_env = [
  'VTE_DEBUG=0'
]

# apparently there is no way to get a name back from an executable(), so it this ugly way
test_units = [
  ['colors', test_colors],
  ['minifont', test_minifont],
  ['modes', test_modes],
  ['parser', test_parser],
  ['pastify', test_pastify],
  ['reaper', test_reaper],
  ['refptr', test_refptr],
  ['stream', test_stream],
  ['tabstops', test_tabstops],
  ['termprops', test_termprops],
  ['unicode-width', test_unicode_width],
  ['utf8', test_utf8],
  ['uuid', test_uuid],
]

if get_option('gtk3')
  test_units += [
    ['vtetypes', test_vtetypes],
  ]
endif

foreach test: test_units
  test(
    test[0],
    test[1],
    env: test_env,
    timeout: 600, # See issue vte#120
  )
endforeach

test_scripts = [
  ['doc syntax', 'check-doc-syntax.sh'],
]

foreach test: test_scripts
  test(
    test[0],
    files(test[1]),
  )
endforeach

test_scripts_with_arg = [
  ['vte-urlencode-cwd', 'test-vte-urlencode-cwd.sh', vte_urlencode_cwd],
]

foreach test: test_scripts_with_arg
  test(
    test[0],
    files(test[1]),
    args: [test[2]],
  )
endforeach

# Shell integration

vte_csh = configure_file(
  input: 'vte.csh.in',
  output: '@BASENAME@',
  configuration: {
    'libexecdir': vte_prefix / vte_libexecdir,
  },
  install: true,
  install_dir: vte_sysconfdir / 'profile.d',
)

vte_sh = configure_file(
  input: 'vte.sh.in',
  output: '@BASENAME@',
  configuration: {
    'libexecdir': vte_prefix / vte_libexecdir,
  },
  install: true,
  install_dir: vte_sysconfdir / 'profile.d',
)

# Systemd integration

if systemd_dep.found()
  install_data(
    sources: 'vte-spawn-.scope.conf',
    install_dir: vte_systemduserunitdir / 'vte-spawn-.scope.d',
    rename: 'defaults.conf'
  )
endif

# Demo application

subdir('app')
