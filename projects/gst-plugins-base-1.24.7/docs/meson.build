build_hotdoc = false

if meson.is_cross_build()
    if get_option('doc').enabled()
        error('Documentation enabled but building the doc while cross building is not supported yet.')
    endif

    message('Documentation not built as building it while cross building is not supported yet.')
    subdir_done()
endif

if static_build
    if get_option('doc').enabled()
        error('Documentation enabled but not supported when building statically.')
    endif

    message('Building statically, can\'t build the documentation')
    subdir_done()
endif

if not build_gir
    if get_option('doc').enabled()
        error('Documentation enabled but introspection not built.')
    endif

    message('Introspection not built, won\'t build documentation requiring it')
    subdir_done()
endif


required_hotdoc_extensions = ['gi-extension', 'gst-extension']
if gst_dep.type_name() == 'internal'
    gst_proj = subproject('gstreamer')
    plugins_cache_generator = gst_proj.get_variable('plugins_cache_generator')
else
    plugins_cache_generator = find_program(join_paths(gst_dep.get_variable('libexecdir'), 'gstreamer-' + api_version, 'gst-plugins-doc-cache-generator'),
        required: false)
endif

cdir = meson.current_source_dir()
plugins_cache = cdir / 'plugins/gst_plugins_cache.json'
if plugins.length() == 0
    message('All base plugins have been disabled')
elif plugins_cache_generator.found()
    gst_plugins_doc_dep = custom_target('base-plugins-doc-cache',
        command: [plugins_cache_generator, plugins_cache, '@OUTPUT@', '@INPUT@'],
        input: plugins,
        output: 'gst_plugins_cache.json',
        build_always_stale: true,
    )
else
    warning('GStreamer plugin inspector for documentation not found, can\'t update the cache')
endif

hotdoc_p = find_program('hotdoc', required: get_option('doc'))
if not hotdoc_p.found()
    message('Hotdoc not found, not building the documentation')
    subdir_done()
endif

hotdoc_req = '>= 0.11.0'
hotdoc_version = run_command(hotdoc_p, '--version', check: false).stdout()
if not hotdoc_version.version_compare(hotdoc_req)
    if get_option('doc').enabled()
        error('Hotdoc version @0@ not found, got @1@'.format(hotdoc_req, hotdoc_version))
    else
        message('Hotdoc version @0@ not found, got @1@'.format(hotdoc_req, hotdoc_version))
        subdir_done()
    endif
endif

hotdoc = import('hotdoc')
foreach extension: required_hotdoc_extensions
    if not hotdoc.has_extensions(extension)
        if get_option('doc').enabled()
            error('Documentation enabled but @0@ missing'.format(extension))
        endif

        message('@0@ extension not found, not building documentation'.format(extension))
        subdir_done()
    endif
endforeach

build_hotdoc = true
docconf = configuration_data()
docconf.set('GST_API_VERSION', api_version)

version_entities = configure_file(input : 'version.in',
    output : 'gst_api_version.md',
    configuration : docconf)

libs_excludes = []
foreach h: ['pbutils-private.h', 'gsttageditingprivate.h', 'id3v2.h',
  'kiss_fft_f32.h', 'kiss_fft_f64.h', 'kiss_fftr_f32.h', 'kiss_fftr_f64.h',
  'kiss_fftr_s16.h', 'kiss_fftr_s32.h', 'kiss_fft_s16.h', 'kiss_fft_s32.h',
  '_kiss_fft_guts_f32.h', '_kiss_fft_guts_f64.h', '_kiss_fft_guts_s16.h',
  '_kiss_fft_guts_s16.h', '_kiss_fft_guts_s32.h', '_kiss_fft_guts_s32.h',
  'pbutils-marshal.h', 'audio-resampler-private.h', '*orc-dist.*',
  '*-neon.h', 'audio-resampler-macros.[ch]', '*-prelude.h', '*_private.h',
  'gstglfuncs.[ch]', 'gstgl_fwd.h'
  ]

  libs_excludes += [join_paths(meson.current_source_dir(), '..', 'gst-libs/gst/*/', h)]
endforeach

libs = [
  ['allocators', allocators_gir, allocators_dep],
  ['app', app_gir, app_dep],
  ['audio', audio_gir, audio_dep],
  # FIXME! ['fft', fft_gir, fft_dep],
  ['pbutils', pbutils_gir, pbutils_dep],
  ['rtp', rtp_gir, rtp_dep],
  ['rtsp', rtsp_gir, rtsp_dep],
  ['sdp', sdp_gir, sdp_dep],
  ['tag', tag_gir, tag_dep],
  ['video', video_gir, video_dep],
]

if build_gstgl
  libs += [['gl', gl_gir, gstgl_dep]]
  if enabled_gl_platforms.contains('egl')
    libs += [['gl-egl', gl_egl_gir, gstgl_dep, [
      join_paths('../gst-libs/gst', 'gl', 'egl', 'gstegl.[ch]'),
      join_paths('../gst-libs/gst', 'gl', 'egl', 'gsteglimage.[ch]'),
      join_paths('../gst-libs/gst', 'gl', 'egl', 'gstgldisplay_egl.[ch]'),
      join_paths('../gst-libs/gst', 'gl', 'egl', 'gstgldisplay_egl_device.[ch]'),
      join_paths('../gst-libs/gst', 'gl', 'egl', 'gstglmemoryegl.[ch]'),
    ]]]
  endif
  if enabled_gl_winsys.contains('x11')
    libs += [['gl-x11', gl_x11_gir, gstgl_dep, [
      join_paths('../gst-libs/gst', 'gl', 'x11', 'gstgldisplay_x11.[ch]'),
    ]]]
  endif
  if enabled_gl_winsys.contains('wayland')
    libs += [['gl-wayland', gl_wayland_gir, gstgl_dep, [
      join_paths('../gst-libs/gst', 'gl', 'wayland', 'gstgldisplay_wayland.[ch]'),
    ]]]
  endif
endif

# Used to avoid conflicts with known plugin names
project_names = {
  'app': 'applib',
  'rtp': 'rtplib',
  'rtsp': 'rtsplib',
}

libs_doc = []
foreach lib: libs
  name = lib[0]
  gir = lib[1]
  deps = [lib[2], gir]
  extra_sources = []
  if lib.length() >= 4
    extra_sources = lib[3]
  endif
  project_name = project_names.get(name, name)
  libs_doc += [hotdoc.generate_doc(project_name,
      project_version: api_version,
      gi_c_sources: [join_paths('../gst-libs/gst', name, '*.[hc]')] + extra_sources,
      gi_sources: gir[0].full_path(),
      gi_c_source_filters: libs_excludes,
      gi_c_source_roots: [join_paths(meson.current_source_dir(), '../gst-libs/gst/' + name), ],
      sitemap: 'libs/' + name + '/sitemap.txt',
      index: 'libs/' + name + '/index.md',
      gi_index: 'libs/' + name + '/index.md',
      gi_smart_index: true,
      gi_order_generated_subpages: true,
      dependencies: deps,
      install: false,
      depends: gir[0],
  )]
endforeach

if not hotdoc.has_extensions('c-extension')
    if get_option('doc').enabled()
        error('Documentation enabled but c-extension missing')
    endif
    message('c-extension not found, not building documentation')
else
    libs_doc += [hotdoc.generate_doc('riff',
        project_version: api_version,
        c_sources: ['../gst-libs/gst/riff/*.[hc]'],
        c_source_filters: libs_excludes,
        sitemap: 'libs/riff/sitemap.txt',
        index: 'libs/riff/index.md',
        c_index: 'libs/riff/index.md',
        c_smart_index: true,
        c_order_generated_subpages: true,
        dependencies: [gst_base_dep, riff_dep],
        install: false,
        disable_incremental_build: true,
    )]
endif

if host_machine.system() == 'windows'
  pathsep = ';'
else
  pathsep = ':'
endif
gst_plugins_doc = run_command(
    plugins_cache_generator,
    'hotdoc-config',
    '--builddir', meson.current_build_dir(),
    '--project_version', api_version,
    '--sitemap', cdir / 'plugins/sitemap.txt',
    '--index', cdir / 'plugins/index.md',
    '--gst_index', cdir / 'plugins/index.md',
    '--gst_c_sources',
        cdir / '../sys/*/*.[ch]',
        cdir / '../ext/*/*.[cmh]',
        cdir / '../gst/*/*.[ch]',
    '--gst_cache_file', plugins_cache,
    check: true,
).stdout().split(pathsep)
