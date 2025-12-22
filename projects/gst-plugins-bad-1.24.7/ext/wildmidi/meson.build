if get_option('wildmidi').disabled()
  subdir_done()
endif

# Recent versions ship a pkg-config file, so try that first
wildmidi_dep = dependency('WildMIDI', version: '>= 0.4.2', required: false)

if not wildmidi_dep.found() and cc.has_header ('wildmidi_lib.h')
  wildmidi_ver_maj = cc.get_define('LIBWILDMIDI_VER_MAJOR', prefix: '#include <wildmidi_lib.h>')
  wildmidi_ver_min = cc.get_define('LIBWILDMIDI_VER_MINOR', prefix: '#include <wildmidi_lib.h>')
  if wildmidi_ver_maj != '' and wildmidi_ver_min != ''
    wildmidi_ver_maj = ''.join(wildmidi_ver_maj.split('L')).to_int() # '0L' -> 0
    wildmidi_ver_min = ''.join(wildmidi_ver_min.split('L')).to_int() # '4L' -> 4
    if wildmidi_ver_maj == 0 and wildmidi_ver_min >= 4
      wildmidi_dep = cc.find_library('WildMidi', required: false)
    else
      message('WildMidi version too old, need >= 0.4.x')
    endif
  else
    message('WildMidi version too old, need >= 0.4.x')
  endif
endif

if wildmidi_dep.found()
  gstwildmidi = library('gstwildmidi',
    'gstwildmididec.c',
    c_args: gst_plugins_bad_args,
    include_directories: [configinc],
    dependencies: [gstaudio_dep, gstbadaudio_dep, wildmidi_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstwildmidi]
elif get_option('wildmidi').enabled()
  error('WildMidi plugin explicitly enabled, but required library or headers not found.')
endif
