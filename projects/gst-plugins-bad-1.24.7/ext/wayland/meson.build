wl_sources = [
    'gstwaylandsink.c'
]

if use_wayland

    gstwaylandsink = library('gstwaylandsink',
        wl_sources,
        c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        include_directories : [configinc],
        dependencies : [gst_dep, gstvideo_dep, gstwayland_dep],
        install : true,
        install_dir : plugins_install_dir,
    )
    plugins += [gstwaylandsink]
endif
