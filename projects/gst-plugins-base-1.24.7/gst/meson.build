foreach plugin : ['adder', 'app', 'audioconvert', 'audiomixer', 'audiorate', 'audioresample',
                  'audiotestsrc', 'compositor', 'dsd', 'debugutils', 'encoding', 'gio', 'overlaycomposition',
                  'pbtypes', 'playback', 'rawparse', 'subparse', 'tcp', 'typefind',
                  'videoconvertscale', 'videorate', 'videotestsrc', 'volume']
  if not get_option(plugin).disabled()
    subdir(plugin)
  endif
endforeach
