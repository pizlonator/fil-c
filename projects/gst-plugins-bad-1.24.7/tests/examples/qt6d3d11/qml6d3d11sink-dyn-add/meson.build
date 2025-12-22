qt_preprocessed = qt6_mod.preprocess(qresources : 'qml6d3d11sink-dyn-add.qrc')
executable('qml6d3d11sink-dyn-add', 'qml6d3d11sink-dyn-add.cpp', qt_preprocessed,
    dependencies : [gst_dep, qt6_example_deps],
    override_options : ['cpp_std=c++17'],
    c_args : gst_plugins_bad_args,
    cpp_args : gst_plugins_bad_args,
    include_directories : [configinc],
    install: false)
