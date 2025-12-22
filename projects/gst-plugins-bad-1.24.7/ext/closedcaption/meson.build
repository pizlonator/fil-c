closedcaption_dep = dependency('pangocairo', version : '>= 1.32.6',
                            required : get_option('closedcaption'))

zvbi_sources = [
  'bit_slicer.c',
  'decoder.c',
  'raw_decoder.c',
  'sampling_par.c',
  'io-sim.c',
]

if closedcaption_dep.found()
  gstclosedcaption = library('gstclosedcaption',
    'gstcccombiner.c', 'gstccextractor.c', 'gstccconverter.c', 'gstcea608mux.c', 'gstclosedcaption.c',
    'gstline21dec.c', 'gstcea708decoder.c', 'gstceaccoverlay.c', 'gstline21enc.c', 'ccutils.c',
    zvbi_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, gstbase_dep, gst_dep, closedcaption_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstclosedcaption]
endif
