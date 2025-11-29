gtk_print_headers = files([
  'gtkpagesetup.h',
  'gtkpapersize.h',
  'gtkprintcontext.h',
  'gtkprintoperation.h',
  'gtkprintoperationpreview.h',
  'gtkprintsettings.h',
])

gtk_print_sources = files([
  'gtkpagesetup.c',
  'gtkpapersize.c',
  'gtkprintcontext.c',
  'gtkprintoperation.c',
  'gtkprintoperationpreview.c',
  'gtkprintsettings.c',
  'gtkprintutils.c',
])

gtk_unix_print_headers = []
gtk_unix_print_sources = []

if os_unix
  gtk_unix_print_headers += files([
    'gtkpagesetupunixdialog.h',
    'gtkprintunixdialog.h',
    'gtkprinter.h',
    'gtkprintjob.h',
  ])

  gtk_unix_print_sources += files([
    'gtkcustompaperunixdialog.c',
    'gtkpagesetupunixdialog.c',
    'gtkpagethumbnail.c',
    'gtkprintbackend.c',
    'gtkprinter.c',
    'gtkprinteroption.c',
    'gtkprinteroptionset.c',
    'gtkprinteroptionwidget.c',
    'gtkprintjob.c',
    'gtkprintoperation-unix.c',
    'gtkprintoperation-portal.c',
    'gtkprintunixdialog.c',
  ])
endif

gtk_win32_print_headers = []
gtk_win32_print_sources = []

if os_win32
  gtk_win32_print_sources += files([
    'gtkprint-win32.c',
    'gtkprintoperation-win32.c',
  ])

  if cc.has_header_symbol('windows.h', 'IPrintDialogCallback')
    cdata.set('HAVE_IPRINTDIALOGCALLBACK', 1)
  endif
endif
