test_media = [
  'audio.ogg',
  'audio-video.ogg',
  'audio-short.ogg',
  'audio-video-short.ogg',
  'sintel.mkv',
  'test_sub.srt',
]

download_media = find_program('download-media')

foreach m: test_media
  downloaded_media = custom_target(m,
    input : [],
    output : m,
    command: [download_media, 'http://gstreamer.freedesktop.org/data/media/small/' + m, '@OUTPUT@'],
    build_by_default: true,
    install: false)
endforeach
