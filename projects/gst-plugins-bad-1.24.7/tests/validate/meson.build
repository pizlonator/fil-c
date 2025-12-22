gst_tester = find_program('gst-tester-@0@'.format(api_version), required: get_option('tests'))
if not gst_tester.found()
    subdir_done()
endif

tests = [
    {'path': 'opencv/cvtracker'},
    {'path': 'testsrcbin/caps_spec'},
    {'path': 'codectimestamper/h264_propagate_caps'},
    {'path': 'wpe/load_bytes_first', 'skip': not building_wpe},
    {'path': 'vtenc/vtenc_h264', 'skip': not applemedia_found_deps},
    {'path': 'vtenc/vtenc_h264_b_frames', 'skip': not applemedia_found_deps},
    {'path': 'vtenc/vtenc_h265', 'skip': not applemedia_found_deps},
    {'path': 'vtenc/vtenc_h265_b_frames', 'skip': not applemedia_found_deps},
    {'path': 'autovideoconvert/renegotiate'},
]

env = environment()
env.set('GST_PLUGIN_PATH_1_0', meson.global_build_root(), pluginsdirs)
env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
env.set('GST_REGISTRY', '@0@/@1@.registry'.format(meson.current_build_dir(), 'validate'))
env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)
env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer', 'gst-validate', 'gst-plugins-base',
    'gst-plugins-bad@' + meson.project_build_root())

foreach t: tests
    if not t.get('skip', false)
        test_dir_name = t.get('path').split('/')
        test_name = 'validate'
        foreach c: test_dir_name
            test_name += '.' + c
        endforeach
        test_env = env
        test_env.set('GST_VALIDATE_LOGSDIR', join_paths(meson.current_build_dir(), test_name))
        test_file = join_paths(meson.current_source_dir(), t.get('path') + '.validatetest')
        test(test_name, gst_tester, args: [test_file, '--use-fakesinks'],
            env: test_env, timeout : 3 * 60, protocol: 'tap')
    endif
endforeach
