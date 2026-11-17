xsltproc = find_program('xsltproc', required: false)
assert(xsltproc.found(), 'xsltproc is required to build documentation')

xsltproc_cmd = [
  xsltproc,
  '--output', '@OUTPUT@',
  '--nonet',
  '--stringparam', 'man.output.quietly', '1',
  '--stringparam', 'funcsynopsis.style', 'ansi',
  '--stringparam', 'man.th.extra1.suppress', '1',
  '--stringparam', 'man.authors.section.enabled', '0',
  '--stringparam', 'man.copyright.section.enabled', '0',
  'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
  '@INPUT@'
]

output = meson.project_name() + '.1'

custom_target(
        output,
        input : meson.project_name() + '.xml',
       output : output,
      command : xsltproc_cmd,
      install : true,
  install_dir : join_paths(control_center_mandir, 'man1')
)
