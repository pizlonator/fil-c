subdir('a11y')
subdir('deprecated')
subdir('inspector')
subdir('print')

gtk_cargs = [
  '-DGTK_COMPILATION',
  '-DG_LOG_DOMAIN="Gtk"',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
  '-DGTK_BINARY_VERSION="@0@"'.format(gtk_binary_version),
  '-DGTK_HOST="@0@"'.format(host_machine.system()),
  '-DGTK_DATA_PREFIX="@0@"'.format(gtk_prefix),
]

# List of sources that do not contain public API, and should not be
# introspected
gtk_private_sources = files([
  'fnmatch.c',
  'gdktextureutils.c',
  'gsettings-mapping.c',
  'gtkaccessibleattributeset.c',
  'gtkaccessiblevalue.c',
  'gtkaccessiblevaluestatic.c',
  'gtkactionhelper.c',
  'gtkactionmuxer.c',
  'gtkactionobservable.c',
  'gtkactionobserver.c',
  'gtkallocatedbitmask.c',
  'gtkapplicationaccels.c',
  'gtkapplicationimpl.c',
  'gtkbookmarksmanager.c',
  'gtkbuilder-menus.c',
  'gtkbuilderprecompile.c',
  'gtkbuiltinicon.c',
  'gtkcolorplane.c',
  'gtkcolorpicker.c',
  'gtkcolorpickerkwin.c',
  'gtkcolorpickerportal.c',
  'gtkcolorpickershell.c',
  'gtkcolorscale.c',
  'gtkcolorswatch.c',
  'gtkcolumnviewcellwidget.c',
  'gtkcolumnviewrowwidget.c',
  'gtkcolumnviewtitle.c',
  'gtkconstraintexpression.c',
  'gtkconstraintsolver.c',
  'gtkconstraintvflparser.c',
  'gtkcssanimatedstyle.c',
  'gtkcssanimation.c',
  'gtkcssarrayvalue.c',
  'gtkcssbgsizevalue.c',
  'gtkcssbordervalue.c',
  'gtkcsscalcvalue.c',
  'gtkcsscolorvalue.c',
  'gtkcsscornervalue.c',
  'gtkcssdimensionvalue.c',
  'gtkcssdynamic.c',
  'gtkcsseasevalue.c',
  'gtkcssenumvalue.c',
  'gtkcssfiltervalue.c',
  'gtkcssfontfeaturesvalue.c',
  'gtkcssfontvariationsvalue.c',
  'gtkcssimage.c',
  'gtkcssimageconic.c',
  'gtkcssimagecrossfade.c',
  'gtkcssimagefallback.c',
  'gtkcssimageicontheme.c',
  'gtkcssimageinvalid.c',
  'gtkcssimagelinear.c',
  'gtkcssimagepaintable.c',
  'gtkcssimageradial.c',
  'gtkcssimagerecolor.c',
  'gtkcssimagescaled.c',
  'gtkcssimageurl.c',
  'gtkcssimagevalue.c',
  'gtkcssinheritvalue.c',
  'gtkcssinitialvalue.c',
  'gtkcsskeyframes.c',
  'gtkcsslineheightvalue.c',
  'gtkcsslookup.c',
  'gtkcssnode.c',
  'gtkcssnodedeclaration.c',
  'gtkcssnodestylecache.c',
  'gtkcssnumbervalue.c',
  'gtkcsspalettevalue.c',
  'gtkcsspositionvalue.c',
  'gtkcssrepeatvalue.c',
  'gtkcssselector.c',
  'gtkcssshadowvalue.c',
  'gtkcssshorthandproperty.c',
  'gtkcssshorthandpropertyimpl.c',
  'gtkcssstaticstyle.c',
  'gtkcssstringvalue.c',
  'gtkcssstyle.c',
  'gtkcssstylechange.c',
  'gtkcssstyleproperty.c',
  'gtkcssstylepropertyimpl.c',
  'gtkcsstransformvalue.c',
  'gtkcsstransientnode.c',
  'gtkcsstransition.c',
  'gtkcsstypes.c',
  'gtkcssunsetvalue.c',
  'gtkcssvalue.c',
  'gtkcsswidgetnode.c',
  'gtkdrop.c',
  'gtkfilechooserentry.c',
  'gtkfilechoosererrorstack.c',
  'gtkfilechoosernativeportal.c',
  'gtkfilechooserutils.c',
  'gtkfilechoosercell.c',
  'gtkfilesystemmodel.c',
  'gtkfilethumbnail.c',
  'gtkgizmo.c',
  'gtkiconcache.c',
  'gtkiconcachevalidator.c',
  'gtkiconhelper.c',
  'gtkjoinedmenu.c',
  'gtkkineticscrolling.c',
  'gtkmagnifier.c',
  'gtkmenusectionbox.c',
  'gtkmenutracker.c',
  'gtkmenutrackeritem.c',
  'gtkpanedhandle.c',
  'gtkpango.c',
  'gskpango.c',
  'gtkpathbar.c',
  'gtkplacessidebar.c',
  'gtkplacesview.c',
  'gtkplacesviewrow.c',
  'gtkpointerfocus.c',
  'gtkpopovercontent.c',
  'gtkprivate.c',
  'gtkprogresstracker.c',
  'gtkrbtree.c',
  'gtkquery.c',
  'gtkscaler.c',
  'gtksearchengine.c',
  'gtksearchenginemodel.c',
  'gtksecurememory.c',
  'gtksizerequestcache.c',
  'gtksortkeys.c',
  'gtkstyleanimation.c',
  'gtkstylecascade.c',
  'gtkstyleproperty.c',
  'gtktextbtree.c',
  'gtktexthistory.c',
  'gtktextviewchild.c',
  'timsort/gtktimsort.c',
  'gtktrashmonitor.c',
])

# List of files that contain public API, and should be introspected
gtk_public_sources = files([
  'gtkaboutdialog.c',
  'gtkaccelgroup.c',
  'gtkaccessible.c',
  'gtkaccessiblerange.c',
  'gtkaccessibletext.c',
  'gtkactionable.c',
  'gtkactionbar.c',
  'gtkadjustment.c',
  'gtkalertdialog.c',
  'gtkapplication.c',
  'gtkapplicationwindow.c',
  'gtkaspectframe.c',
  'gtkatcontext.c',
  'gtkbinlayout.c',
  'gtkbitset.c',
  'gtkboolfilter.c',
  'gtkbookmarklist.c',
  'gtkborder.c',
  'gtkboxlayout.c',
  'gtkbox.c',
  'gtkbuildable.c',
  'gtkbuilder.c',
  'gtkbuilderlistitemfactory.c',
  'gtkbuilderparser.c',
  'gtkbuilderscope.c',
  'gtkbutton.c',
  'gtkcalendar.c',
  'gtkcenterbox.c',
  'gtkcenterlayout.c',
  'gtkcheckbutton.c',
  'gtkcoloreditor.c',
  'gtkcolorchooserdialog.c',
  'gtkcolorchooserwidget.c',
  'gtkcolordialog.c',
  'gtkcolordialogbutton.c',
  'gtkcolorutils.c',
  'gtkcolumnview.c',
  'gtkcolumnviewcell.c',
  'gtkcolumnviewcolumn.c',
  'gtkcolumnviewrow.c',
  'gtkcolumnviewsorter.c',
  'gtkcomposetable.c',
  'gtkconstraintguide.c',
  'gtkconstraintlayout.c',
  'gtkconstraint.c',
  'gtkcssprovider.c',
  'gtkcustomfilter.c',
  'gtkcustomlayout.c',
  'gtkcustomsorter.c',
  'gtkdialogerror.c',
  'gtkdirectorylist.c',
  'gtkdragicon.c',
  'gtkdragsource.c',
  'gtkdrawingarea.c',
  'gtkdropcontrollermotion.c',
  'gtkdroptarget.c',
  'gtkdroptargetasync.c',
  'gtkdropdown.c',
  'gtkeditable.c',
  'gtkeditablelabel.c',
  'gtkemojichooser.c',
  'gtkemojicompletion.c',
  'gtkentry.c',
  'gtkentrybuffer.c',
  'gtkeventcontroller.c',
  'gtkeventcontrollerfocus.c',
  'gtkeventcontrollerkey.c',
  'gtkeventcontrollerlegacy.c',
  'gtkeventcontrollermotion.c',
  'gtkeventcontrollerscroll.c',
  'gtkexpander.c',
  'gtkexpression.c',
  'gtkfilechooserdialog.c',
  'gtkfilechoosernative.c',
  'gtkfilechooserwidget.c',
  'gtkfiledialog.c',
  'gtkfilefilter.c',
  'gtkfilelauncher.c',
  'gtkfilter.c',
  'gtkfilterlistmodel.c',
  'gtkfixed.c',
  'gtkfixedlayout.c',
  'gtkflattenlistmodel.c',
  'gtkflowbox.c',
  'gtkfontchooserdialog.c',
  'gtkfontchooserutils.c',
  'gtkfontchooserwidget.c',
  'gtkfontdialog.c',
  'gtkfontdialogbutton.c',
  'gtkframe.c',
  'gtkgesture.c',
  'gtkgesturedrag.c',
  'gtkgesturelongpress.c',
  'gtkgestureclick.c',
  'gtkgesturepan.c',
  'gtkgesturerotate.c',
  'gtkgesturesingle.c',
  'gtkgesturestylus.c',
  'gtkgestureswipe.c',
  'gtkgesturezoom.c',
  'gtkglarea.c',
  'gtkgraphicsoffload.c',
  'gtkgrid.c',
  'gtkgridlayout.c',
  'gtkgridview.c',
  'gtkheaderbar.c',
  'gtkicontheme.c',
  'gtkimage.c',
  'gtkimagedefinition.c',
  'gtkimcontext.c',
  'gtkimcontextsimple.c',
  'gtkimmodule.c',
  'gtkimmulticontext.c',
  'gtkinscription.c',
  'gtklabel.c',
  'gtklayoutchild.c',
  'gtklayoutmanager.c',
  'gtklevelbar.c',
  'gtklistbase.c',
  'gtklinkbutton.c',
  'gtklistbox.c',
  'gtklistfactorywidget.c',
  'gtklistheader.c',
  'gtklistheaderbase.c',
  'gtklistheaderwidget.c',
  'gtklistitem.c',
  'gtklistitembase.c',
  'gtklistitemfactory.c',
  'gtklistitemmanager.c',
  'gtklistitemwidget.c',
  'gtklistlistmodel.c',
  'gtklistview.c',
  'gtkmain.c',
  'gtkmaplistmodel.c',
  'gtkmediacontrols.c',
  'gtkmediafile.c',
  'gtkmediastream.c',
  'gtkmenubutton.c',
  'gtkmessagedialog.c',
  'gtkmodelbutton.c',
  'gtkmodules.c',
  'gtkmountoperation.c',
  'gtkmultifilter.c',
  'gtkmultiselection.c',
  'gtkmultisorter.c',
  'gtknativedialog.c',
  'gtknomediafile.c',
  'gtknoselection.c',
  'gtknotebook.c',
  'gtknumericsorter.c',
  'gtkorientable.c',
  'gtkoverlay.c',
  'gtkoverlaylayout.c',
  'gtkpadcontroller.c',
  'gtkpaned.c',
  'gtkpasswordentry.c',
  'gtkpasswordentrybuffer.c',
  'gtkpicture.c',
  'gtkpopover.c',
  'gtkpopovermenu.c',
  'gtkpopovermenubar.c',
  'gtkprintdialog.c',
  'gtkprogressbar.c',
  'gtkpropertylookuplistmodel.c',
  'gtkrange.c',
  'gtkrecentmanager.c',
  'gtkrenderbackground.c',
  'gtkrenderborder.c',
  'gtkrendericon.c',
  'gtkrenderlayout.c',
  'gtkrendernodepaintable.c',
  'gtkrevealer.c',
  'gtkroot.c',
  'gtknative.c',
  'gtkroundedbox.c',
  'gtkscale.c',
  'gtkscalebutton.c',
  'gtkscrollable.c',
  'gtkscrollbar.c',
  'gtkscrollinfo.c',
  'gtkscrolledwindow.c',
  'gtksearchbar.c',
  'gtksearchentry.c',
  'gtksectionmodel.c',
  'gtkselectionfiltermodel.c',
  'gtkselectionmodel.c',
  'gtkseparator.c',
  'gtksettings.c',
  'gtkshortcut.c',
  'gtkshortcutaction.c',
  'gtkshortcutcontroller.c',
  'gtkshortcutlabel.c',
  'gtkshortcutmanager.c',
  'gtkshortcutsgroup.c',
  'gtkshortcutssection.c',
  'gtkshortcutsshortcut.c',
  'gtkshortcutswindow.c',
  'gtkshortcuttrigger.c',
  'gtksidebarrow.c',
  'gtksignallistitemfactory.c',
  'gtksingleselection.c',
  'gtksizegroup.c',
  'gtksizerequest.c',
  'gtkslicelistmodel.c',
  'gtksnapshot.c',
  'gtksorter.c',
  'gtksortlistmodel.c',
  'gtkspinbutton.c',
  'gtkspinner.c',
  'gtkstack.c',
  'gtkstacksidebar.c',
  'gtkstackswitcher.c',
  'gtkstringfilter.c',
  'gtkstringlist.c',
  'gtkstringsorter.c',
  'gtkstyleprovider.c',
  'gtkswitch.c',
  'gtksymbolicpaintable.c',
  'gtktestatcontext.c',
  'gtktestutils.c',
  'gtktext.c',
  'gtktextattributes.c',
  'gtktextbuffer.c',
  'gtktextchild.c',
  'gtktexthandle.c',
  'gtktextiter.c',
  'gtktextlayout.c',
  'gtktextlinedisplaycache.c',
  'gtktextmark.c',
  'gtktextsegment.c',
  'gtktexttag.c',
  'gtktexttagtable.c',
  'gtktexttypes.c',
  'gtktextutil.c',
  'gtktextview.c',
  'gtktogglebutton.c',
  'gtktooltip.c',
  'gtktooltipwindow.c',
  'gtktreeexpander.c',
  'gtktreelistmodel.c',
  'gtktreelistrowsorter.c',
  'gtkurilauncher.c',
  'gtkversion.c',
  'gtkvideo.c',
  'gtkviewport.c',
  'gtkwidget.c',
  'gtkwidgetfocus.c',
  'gtkwidgetpaintable.c',
  'gtkwindow.c',
  'gtkwindowcontrols.c',
  'gtkwindowgroup.c',
  'gtkwindowhandle.c',
])

gtk_public_sources += gtk_print_sources

gtk_private_type_headers = files([
  'gtkcsstypesprivate.h',
  'gtktexthandleprivate.h',
  'gtkplacessidebarprivate.h',
])

gtk_public_headers = files([
  'gtkaboutdialog.h',
  'gtkaccelgroup.h',
  'gtkaccessible.h',
  'gtkaccessiblerange.h',
  'gtkaccessibletext.h',
  'gtkactionable.h',
  'gtkactionbar.h',
  'gtkadjustment.h',
  'gtkalertdialog.h',
  'gtkapplication.h',
  'gtkapplicationwindow.h',
  'gtkaspectframe.h',
  'gtkatcontext.h',
  'gtkbinlayout.h',
  'gtkbitset.h',
  'gtkbookmarklist.h',
  'gtkboolfilter.h',
  'gtkborder.h',
  'gtkbox.h',
  'gtkboxlayout.h',
  'gtkbuildable.h',
  'gtkbuilder.h',
  'gtkbuilderlistitemfactory.h',
  'gtkbuilderscope.h',
  'gtkbutton.h',
  'gtkcalendar.h',
  'gtkcenterbox.h',
  'gtkcenterlayout.h',
  'gtkcheckbutton.h',
  'gtkcolordialog.h',
  'gtkcolordialogbutton.h',
  'gtkcolorutils.h',
  'gtkcolumnview.h',
  'gtkcolumnviewcell.h',
  'gtkcolumnviewcolumn.h',
  'gtkcolumnviewrow.h',
  'gtkcolumnviewsorter.h',
  'gtkconstraintguide.h',
  'gtkconstraintlayout.h',
  'gtkconstraint.h',
  'gtkcssprovider.h',
  'gtkcustomfilter.h',
  'gtkcustomlayout.h',
  'gtkcustomsorter.h',
  'gtkdebug.h',
  'gtkdialogerror.h',
  'gtkdirectorylist.h',
  'gtkdragicon.h',
  'gtkdragsource.h',
  'gtkdrawingarea.h',
  'gtkdropcontrollermotion.h',
  'gtkdroptarget.h',
  'gtkdroptargetasync.h',
  'gtkdropdown.h',
  'gtkeditable.h',
  'gtkeditablelabel.h',
  'gtkemojichooser.h',
  'gtkentry.h',
  'gtkentrybuffer.h',
  'gtkenums.h',
  'gtkeventcontroller.h',
  'gtkeventcontrollerfocus.h',
  'gtkeventcontrollerkey.h',
  'gtkeventcontrollerscroll.h',
  'gtkeventcontrollermotion.h',
  'gtkeventcontrollerlegacy.h',
  'gtkexpander.h',
  'gtkexpression.h',
  'gtkfiledialog.h',
  'gtkfilefilter.h',
  'gtkfilelauncher.h',
  'gtkfilter.h',
  'gtkfilterlistmodel.h',
  'gtkfixed.h',
  'gtkfixedlayout.h',
  'gtkflattenlistmodel.h',
  'gtkflowbox.h',
  'gtkfontdialog.h',
  'gtkfontdialogbutton.h',
  'gtkframe.h',
  'gtkgesture.h',
  'gtkgesturedrag.h',
  'gtkgesturelongpress.h',
  'gtkgestureclick.h',
  'gtkgesturepan.h',
  'gtkgesturerotate.h',
  'gtkgesturesingle.h',
  'gtkgesturestylus.h',
  'gtkgestureswipe.h',
  'gtkgesturezoom.h',
  'gtkglarea.h',
  'gtkgraphicsoffload.h',
  'gtkgrid.h',
  'gtkgridlayout.h',
  'gtkgridview.h',
  'gtkheaderbar.h',
  'gtkicontheme.h',
  'gtkimage.h',
  'gtkimcontext.h',
  'gtkimcontextsimple.h',
  'gtkimmodule.h',
  'gtkimmulticontext.h',
  'gtkinscription.h',
  'gtklabel.h',
  'gtklayoutchild.h',
  'gtklayoutmanager.h',
  'gtklevelbar.h',
  'gtklinkbutton.h',
  'gtklistbase.h',
  'gtklistbox.h',
  'gtklistheader.h',
  'gtklistitem.h',
  'gtklistitemfactory.h',
  'gtklistview.h',
  'gtkmain.h',
  'gtkmaplistmodel.h',
  'gtkmediacontrols.h',
  'gtkmediafile.h',
  'gtkmediastream.h',
  'gtkmenubutton.h',
  'gtkmountoperation.h',
  'gtkmultifilter.h',
  'gtkmultiselection.h',
  'gtkmultisorter.h',
  'gtknative.h',
  'gtknativedialog.h',
  'gtknoselection.h',
  'gtknotebook.h',
  'gtknumericsorter.h',
  'gtkorientable.h',
  'gtkoverlay.h',
  'gtkoverlaylayout.h',
  'gtkpadcontroller.h',
  'gtkpaned.h',
  'gtkpasswordentry.h',
  'gtkpasswordentrybuffer.h',
  'gtkpicture.h',
  'gtkpopover.h',
  'gtkpopovermenu.h',
  'gtkpopovermenubar.h',
  'gtkprintdialog.h',
  'gtkprogressbar.h',
  'gtkrange.h',
  'gtkrecentmanager.h',
  'gtkrevealer.h',
  'gtkroot.h',
  'gtkscale.h',
  'gtkscalebutton.h',
  'gtkscrollable.h',
  'gtkscrollbar.h',
  'gtkscrollinfo.h',
  'gtkscrolledwindow.h',
  'gtksearchbar.h',
  'gtksearchentry.h',
  'gtksectionmodel.h',
  'gtkselectionfiltermodel.h',
  'gtkselectionmodel.h',
  'gtkseparator.h',
  'gtksettings.h',
  'gtkshortcut.h',
  'gtkshortcutaction.h',
  'gtkshortcutcontroller.h',
  'gtkshortcutlabel.h',
  'gtkshortcutmanager.h',
  'gtkshortcutsgroup.h',
  'gtkshortcutssection.h',
  'gtkshortcutsshortcut.h',
  'gtkshortcutswindow.h',
  'gtkshortcuttrigger.h',
  'gtksignallistitemfactory.h',
  'gtksingleselection.h',
  'gtksizegroup.h',
  'gtksizerequest.h',
  'gtkslicelistmodel.h',
  'gtksnapshot.h',
  'gtksorter.h',
  'gtksortlistmodel.h',
  'gtkspinbutton.h',
  'gtkspinner.h',
  'gtkstack.h',
  'gtkstacksidebar.h',
  'gtkstackswitcher.h',
  'gtkstringfilter.h',
  'gtkstringlist.h',
  'gtkstringsorter.h',
  'gtkstyleprovider.h',
  'gtkswitch.h',
  'gtksymbolicpaintable.h',
  'gtktestatcontext.h',
  'gtktestutils.h',
  'gtktext.h',
  'gtktextbuffer.h',
  'gtktextchild.h',
  'gtktextiter.h',
  'gtktextmark.h',
  'gtktexttag.h',
  'gtktexttagtable.h',
  'gtktextview.h',
  'gtktogglebutton.h',
  'gtktooltip.h',
  'gtktreeexpander.h',
  'gtktreelistmodel.h',
  'gtktreelistrowsorter.h',
  'gtktypes.h',
  'gtkurilauncher.h',
  'gtkvideo.h',
  'gtkviewport.h',
  'gtkwidget.h',
  'gtkwidgetpaintable.h',
  'gtkwindow.h',
  'gtkwindowcontrols.h',
  'gtkwindowgroup.h',
  'gtkwindowhandle.h',
  'gtk.h',
])

install_headers(gtk_public_headers, subdir: 'gtk-4.0/gtk/')
install_headers(gtk_deprecated_headers, subdir: 'gtk-4.0/gtk/deprecated')
install_headers(gtk_print_headers, subdir: 'gtk-4.0/gtk/print')

gtk_sources = gtk_public_sources + gtk_private_sources

proto_sources = [
  ['text-input', 'unstable', 'v3', ],
]

im_wayland_sources = files('gtkimcontextwayland.c')

foreach p: proto_sources
  proto_name = p.get(0)
  proto_stability = p.get(1)

  if wayland_enabled
    if proto_stability == 'stable'
      output_base = proto_name
      input = files('@0@.xml'.format(proto_name))
    else
      proto_version = p.get(2)
      output_base = '@0@-@1@-@2@'.format(proto_name, proto_stability, proto_version)
      input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
    endif

    # wayland_scanner is defined in gdk/wayland/meson.build
    im_wayland_sources += custom_target('@0@ client header'.format(output_base),
      input: input,
      output: '@0@-client-protocol.h'.format(output_base),
      command: [
        wayland_scanner,
        'client-header',
        '@INPUT@', '@OUTPUT@',
      ],
    )

    im_wayland_sources += custom_target('@0@ source'.format(output_base),
      input: input,
      output: '@0@-protocol.c'.format(output_base),
      command: [
        wayland_scanner,
        'private-code',
        '@INPUT@', '@OUTPUT@',
      ],
    )
  endif
endforeach

if os_unix and tracker3_dep.found()
  gtk_sources += 'gtksearchenginetracker3.c'
endif

gtk_unix_print_main_header = [
  'gtkunixprint.h'
]

if os_unix
  install_headers(gtk_unix_print_main_header, subdir: 'gtk-4.0/unix-print/gtk')
  install_headers(gtk_unix_print_headers, subdir: 'gtk-4.0/unix-print/gtk/print')
  gtk_sources += gtk_unix_print_sources
endif

if os_win32
  gtk_sources += gtk_win32_print_sources
  gtk_sources += [
    'gtkcolorpickerwin32.c',
    'gtkimcontextime.c',
    'gtkfilechoosernativewin32.c',
    'gtkwin32.c',
  ]
endif

if x11_enabled
  gtk_sources += ['gtkapplication-x11.c', 'gtkmountoperation-x11.c']
endif

if wayland_enabled
  gtk_sources += ['gtkapplication-wayland.c']
  gtk_sources += im_wayland_sources
endif

if x11_enabled or wayland_enabled
  gtk_sources += ['gtkapplication-dbus.c']
endif

if win32_enabled
  gtk_sources += ['gtkmountoperation-stub.c']
endif

if macos_enabled
  gtk_sources += files([
    'gtksearchenginequartz.c',
    'gtkapplication-quartz.c',
    'gtkapplication-quartz-menu.c',
    'gtkfilechoosernativequartz.c',
    'gtkimcontextquartz.c',
    'gtkquartz.c',
  ])

  gtk_cargs += ['-xobjective-c']  # FIXME? maybe add_languages() instead?
endif

if not (x11_enabled or win32_enabled)
  gtk_sources += ['gtkmountoperation-stub.c', ]
endif

if not os_win32
  gtk_sources += ['gtkopenuriportal.c', ]
endif

gen_gtk_gresources_xml = find_program('gen-gtk-gresources-xml.py')
gtk_gresources_xml = configure_file(output: 'gtk.gresources.xml',
  command: [
    gen_gtk_gresources_xml,
    meson.current_source_dir(),
    host_machine.endian(),
    '@OUTPUT@'
  ],
)

theme_deps = []
# For git checkouts, but not for tarballs...
if not fs.exists('theme/Default/Default-light.css')
  # ... build the theme files
  sassc = find_program('sassc', native: true)
  sassc_opts = [ '-a', '-M', '-t', 'compact' ]

  subdir('theme/Default')

  theme_deps += [
    default_theme_deps,
  ]
endif


if can_use_objcopy_for_resources
  # Create the resource blob
  gtk_gresource = custom_target('gtk.gresource',
      input : gtk_gresources_xml,
      depends : theme_deps,
      output : 'gtk.gresource',
      depfile : 'gtk.gresource.d',
      command : [glib_compile_resources,
                 '--generate',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '@INPUT@'])

  # Create resource data file
  gtk_resources_c = custom_target('gtkresources.c',
      input : gtk_gresources_xml,
      depends : theme_deps,
      output : 'gtkresources.c',
      depfile : 'gtkresources.c.d',
      command : [glib_compile_resources,
                 '--generate-source',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--external-data',
                 '--c-name', '_gtk',
                 '--manual-register',
                 '@INPUT@'])

  gtk_resources_h = custom_target('gtkresources.h',
      input : gtk_gresources_xml,
      depends : theme_deps,
      output : 'gtkresources.h',
      depfile : 'gtkresources.h.d',
      command : [glib_compile_resources,
                 '--generate-header',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--external-data',
                 '--c-name', '_gtk',
                 '--manual-register',
                 '@INPUT@'])

  # Create object file containing resource data
  gtk_resources_binary = custom_target('gtkresources_blob.o',
      input : gtk_gresource,
      output : 'gtkresources_blob.o',
      command : [ld,
                 '-z', 'noexecstack',
                 '-r',
                 '-b','binary',
                 '@INPUT@',
                 '-o','@OUTPUT@'])

  # Rename symbol to match the one in the C file
  gtk_resources_o = custom_target('gtkresources_blob2.o',
    input : gtk_resources_binary,
    output : 'gtkresources_blob2.o',
    command : [objcopy,
                 '--strip-all',
                 '--rename-section', '.data=.gresource.gtk',
                 '--add-symbol','_gtk_resource_data=.gresource.gtk:0',
                 '@INPUT@',
                 '@OUTPUT@'])

  gtkresources = [
      gtk_resources_c,
      gtk_resources_h,
      gtk_resources_o,
    ]
else
  gtkresources = gnome.compile_resources('gtkresources',
    gtk_gresources_xml,
    dependencies: theme_deps,
    source_dir: [
      # List in order of preference
      meson.current_build_dir(),
      meson.current_source_dir(),
    ],
    c_name: '_gtk',
    extra_args: '--manual-register',
  )
endif

foreach lang : [
    'bn',
    'de',
    'da',
    'fr',
    'es',
    'et',
    'fi',
    'hi',
    'hu',
    'it',
    'ja',
    'ko',
    'lt',
    'ms',
    'nb',
    'nl',
    'pl',
    'pt',
    'ru',
    'sv',
    'th',
    'uk',
    'zh'
  ]
  conf = configuration_data()
  conf.set('lang', lang)
  resxml = configure_file(input: 'emoji/gresource.xml.in',
    output: lang + '.gresource.xml',
    configuration: conf
  )

  gnome.compile_resources(lang,
    resxml,
    source_dir: meson.current_source_dir() / 'emoji',
    gresource_bundle: true,
    install: true,
    install_dir: gtk_datadir / 'gtk-4.0/emoji',
  )
endforeach

gtk_x11_sources = files([
  'gtkapplication-x11.c',
  'gtkmountoperation-x11.c',
])

gtk_wayland_sources = files([
  'gtkapplication-wayland.c'
])

gtk_use_wayland_or_x11_c_sources = files([
  'gtkapplication-dbus.c'
])

gtk_dbus_src = gnome.gdbus_codegen('gtkdbusgenerated',
  'gtkdbusinterfaces.xml',
  interface_prefix: 'org.Gtk.',
  namespace: '_Gtk',
)

gtkmarshalers = gnome.genmarshal('gtkmarshalers',
  sources: 'gtkmarshalers.list',
  prefix: '_gtk_marshal',
  valist_marshallers: true,
)

gtkmarshal_h = gtkmarshalers[1]

gtktypebuiltins = gnome.mkenums('gtktypebuiltins',
  sources: gtk_public_headers + gtk_deprecated_headers + gtk_print_headers,
  c_template: 'gtktypebuiltins.c.template',
  h_template: 'gtktypebuiltins.h.template',
  install_dir: gtk_includedir / 'gtk-4.0/gtk',
  install_header: true,
)

gtktypebuiltins_h = gtktypebuiltins[1]

gtkprivatetypebuiltins = gnome.mkenums('gtkprivatetypebuiltins',
  sources: gtk_private_type_headers,
  c_template: 'gtkprivatetypebuiltins.c.template',
  h_template: 'gtkprivatetypebuiltins.h.template',
)

gtkprivatetypebuiltins_h = gtkprivatetypebuiltins[1]

# Generate gtktypefuncs.inc
typefuncs = custom_target('gtktypefuncs.inc',
  depends: [ gdkenum_h, gskenum_h, ],
  output: 'gtktypefuncs.inc',
  input: gdk_headers +
    gsk_public_headers +
    gtk_public_headers +
    gtk_deprecated_headers +
    [ gtktypebuiltins_h, ] +
    [ gtk_css_enum_h, gdkenum_h, gskenum_h, ],
  command: [
    find_program('gentypefuncs.py'),
    '@OUTPUT@',
    '@INPUT@',
  ],
  install: false,
)

if os_win32
  xdp_dbus_generated = []
else
  xdp_dbus_generated = gnome.gdbus_codegen('xdp-dbus',
    sources : 'org.freedesktop.portal.OpenURI.xml',
    interface_prefix : 'org.freedesktop.portal.',
    namespace : 'GtkXdp',
  )
endif

gtkconfig_cdata = configuration_data()
gtkconfig_cdata.set('GTK_ACCESSIBILITY_ATSPI', gtk_a11y_backends.contains('atspi'))

gtkconfig = configure_file(
  input: 'gtkconfig.h.meson',
  output: 'gtkconfig.h',
  configuration: gtkconfig_cdata,
  install_dir: gtk_includedir / 'gtk-4.0/gtk',
)

gtkversion_cdata = configuration_data()
gtkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gtkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gtkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)
gtkversion_cdata.set('GTK_BINARY_AGE', gtk_binary_age)
gtkversion_cdata.set('GTK_INTERFACE_AGE', gtk_interface_age)
gtkversion_cdata.set('GTK_VERSION', gtk_version)
gtkversion_cdata.set('GTK_API_VERSION', gtk_api_version)

if host_machine.system() == 'windows'
  v = gtk_binary_version.split('.')
  gtk_manifest_version = '.'.join(v.get(0, '0'), v.get(1, '0'), v.get(2, '0'), v.get(3, '0'))
  gtkversion_cdata.set('GTK_MANIFEST_VERSION', gtk_manifest_version)
endif

gtkversion = configure_file(input: 'gtkversion.h.in',
  output: 'gtkversion.h',
  configuration: gtkversion_cdata,
  install: true,
  install_dir: gtk_includedir / 'gtk-4.0/gtk',
)

gtk_gen_headers = [
  gtkmarshal_h,
  gtktypebuiltins_h,
  gtkprivatetypebuiltins_h,
  gtkconfig,
  gtkversion,
]

font_script_language_sources = files([
  'language-names.c',
])

gtk_sources += [
  gtk_a11y_src,
  gtk_dbus_src,
  gtk_deprecated_sources,
  inspector_sources,
  gtkresources,
  gtkmarshalers,
  gtkprivatetypebuiltins,
  gtktypebuiltins,
  font_script_language_sources,
]

gtk_deps = [
  gmodule_dep,
  gobject_dep,
  platform_gio_dep,
  pangocairo_dep,
  harfbuzz_dep,
  fribidi_dep,
  cairogobj_dep,
  fontconfig_dep,
  pixbuf_dep,
  epoxy_dep,
  libm,
  graphene_dep,
  profile_conf_h,
]

if x11_enabled
  x11_data_prefix = dependency('x11').get_variable(pkgconfig: 'prefix')

  gtk_cargs += [ '-DX11_DATA_PREFIX="@0@"'.format(x11_data_prefix), ]
  gtk_sources += gtk_x11_sources
  gtk_deps += [ xi_dep, x11_dep, ]
endif

if wayland_enabled
  gtk_sources += gtk_wayland_sources
endif

if broadway_enabled
  gtk_sources += ['gtkimcontextbroadway.c']
endif

if win32_enabled
  gtk_cargs += ['-DGTK_PRINT_BACKENDS="file,lpr"']
  gtk_deps += [ giowin32_dep, pangowin32_dep ]

  gtk_deps += [
    cc.find_library('advapi32'),
    cc.find_library('comctl32'),
    cc.find_library('crypt32'), # For CryptProtectMemory()
    cc.find_library('dwmapi'),
    cc.find_library('imm32'),
    cc.find_library('setupapi'),
    cc.find_library('winmm'),
  ]

  gtkwin32rc = configure_file(input: 'gtk-win32.rc.body.in',
    output: 'gtk-win32.rc.body',
    configuration: gtkversion_cdata,
  )

  win32_manifest = configure_file(input: 'libgtk.manifest.in',
    output: 'libgtk.manifest',
    configuration: gtkversion_cdata,
  )

  win32res = import('windows').compile_resources(gtkwin32rc, include_directories: win32rcinc)
  gtk_sources += win32res
endif

# So we don't add these twice
if x11_enabled or wayland_enabled
  gtk_sources += gtk_use_wayland_or_x11_c_sources
endif

if pangoft_dep.found()
  gtk_deps += pangoft_dep
endif

if macos_enabled
  gtk_macos_frameworks = [
    'AppKit',
  ]
  gtk_deps += [dependency('appleframeworks', modules: gtk_macos_frameworks)]
endif

gtk_deps += cloudproviders_dep

if os_unix
  gtk_deps += tracker3_dep
endif

# Unconditional. If libintl isn't found,
# the object just does nothing being in the deplist
gtk_deps += libintl_dep

gtk_settings_schemas = [
  'org.gtk.gtk4.Settings.FileChooser.gschema.xml',
  'org.gtk.gtk4.Settings.ColorChooser.gschema.xml',
  'org.gtk.gtk4.Settings.EmojiChooser.gschema.xml',
  'org.gtk.gtk4.Settings.Debug.gschema.xml',
]
install_data(gtk_settings_schemas, install_dir: gtk_schemasdir)
gnome.compile_schemas(depend_files: files(gtk_settings_schemas), build_by_default: true)
gtk_schema_build_dir = meson.current_build_dir()

# Check for more things
if cc.has_header('langinfo.h')
  foreach nl_enum: [ '_NL_MEASUREMENT_MEASUREMENT',
                     '_NL_PAPER_HEIGHT',
                     '_NL_PAPER_WIDTH',
                     '_NL_TIME_FIRST_WEEKDAY', ]
    cdata.set('HAVE_' + nl_enum, cc.has_header_symbol('langinfo.h', nl_enum))
  endforeach
endif

# Generate versions for macOS dylib
darwin_versions = [
  # compatibility version
  1 + '@0@'.format(gtk_binary_age - gtk_interface_age).to_int(),
  # current version
  '@0@.@1@'.format(1 + '@0@'.format(gtk_binary_age - gtk_interface_age).to_int(), gtk_interface_age),
]

# Library
libgtk_static = static_library('gtk',
  sources: [typefuncs, gtk_sources, gtkmarshal_h, gtkprivatetypebuiltins_h, xdp_dbus_generated],
  c_args: gtk_cargs + common_cflags,
  include_directories: [confinc, gdkinc, gskinc, gtkinc],
  dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
  link_with: [libgtk_css, libgdk, libgsk ],
)

libgtk = shared_library('gtk-4',
  c_args: gtk_cargs + common_cflags,
  include_directories: [confinc, gdkinc, gskinc, gtkinc],
  dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
  link_whole: [libgtk_static, libgtk_css, libgdk, libgsk ],
  link_args: common_ldflags,
  soversion: gtk_soversion,
  version: gtk_library_version,
  darwin_versions: darwin_versions,
  gnu_symbol_visibility: 'hidden',
  install: true,
)

gtk_dep_sources = [gtkconfig, gtkversion, gtktypebuiltins_h]

# Introspection
if build_gir
  gir_args = [
    '-DGTK_COMPILATION',
    '--quiet',
  ]

  gdk_gir_inc = [ 'cairo-1.0', 'Gio-2.0', 'GdkPixbuf-2.0', 'Pango-1.0', 'PangoCairo-1.0' ]

  gdk_gir = gnome.generate_gir(libgtk,
    sources: gdk_public_headers + gdk_deprecated_headers + gdk_public_sources + gdk_deprecated_sources + [ gdkenum_h ],
    namespace: 'Gdk',
    nsversion: gtk_api_version,
    identifier_prefix: 'Gdk',
    symbol_prefix: 'gdk',
    export_packages: 'gtk4',
    includes: gdk_gir_inc,
    header: 'gdk/gdk.h',
    install: true,
    dependencies: libgdk_dep,
    extra_args: gir_args,
    fatal_warnings: get_option('werror'),
  )
  gtk_dep_sources += gdk_gir

  if x11_enabled
    gdk_x11_gir = gnome.generate_gir(libgtk,
      sources: gdk_x11_public_headers + gdk_x11_public_sources,
      namespace: 'GdkX11',
      nsversion: gtk_api_version,
      identifier_prefix: 'Gdk',
      symbol_prefix: 'gdk',
      export_packages: 'gtk4-x11',
      includes: [ gdk_gir[0], 'xlib-2.0', ],
      install: true,
      dependencies: libgdk_dep,
      header: 'gdk/x11/gdkx.h',
      extra_args: gir_args,
      fatal_warnings: get_option('werror'),
    )
    gtk_dep_sources += gdk_x11_gir
  endif

  if wayland_enabled
    gdk_wayland_gir = gnome.generate_gir(libgtk,
      sources: gdk_wayland_public_headers + gdk_wayland_sources,
      namespace: 'GdkWayland',
      nsversion: gtk_api_version,
      identifier_prefix: 'Gdk',
      symbol_prefix: 'gdk',
      export_packages: 'gtk4-wayland',
      includes: [ gdk_gir[0], ],
      install: true,
      dependencies: libgdk_dep,
      header: 'gdk/wayland/gdkwayland.h',
      extra_args: gir_args,
      fatal_warnings: get_option('werror'),
    )
    gtk_dep_sources += gdk_wayland_gir
  endif

  if win32_enabled
    gdk_win32_gir = gnome.generate_gir(libgtk,
      sources: gdk_win32_public_headers + gdk_win32_public_sources,
      namespace: 'GdkWin32',
      nsversion: gtk_api_version,
      identifier_prefix: 'Gdk',
      symbol_prefix: 'gdk',
      export_packages: 'gtk4-win32',
      includes: [ gdk_gir[0], 'win32-1.0', ],
      install: true,
      dependencies: libgdk_dep,
      header: 'gdk/win32/gdkwin32.h',
      extra_args: gir_args,
      fatal_warnings: get_option('werror'),
    )
    gtk_dep_sources += gdk_win32_gir
  endif

  gsk_gir_inc = [ gdk_gir[0], 'Graphene-1.0' ]

  gsk_gir = gnome.generate_gir(libgtk,
    sources: gsk_public_headers + gsk_public_sources + [ gskenum_h ],
    namespace: 'Gsk',
    nsversion: gtk_api_version,
    identifier_prefix: 'Gsk',
    symbol_prefix: 'gsk',
    export_packages: 'gtk4',
    includes: gsk_gir_inc,
    header: 'gsk/gsk.h',
    install: true,
    dependencies: libgsk_dep,
    extra_args: gir_args,
    fatal_warnings: get_option('werror'),
  )
  gtk_dep_sources += gsk_gir

  gtk_introspection_sources = [
    gtk_public_headers,
    gtk_public_sources,
    gtk_print_headers,
    gtk_css_public_headers,
    gtk_css_public_sources,
    gtk_deprecated_headers,
    gtk_deprecated_sources,
    gtktypebuiltins_h,
    gtkversion,
    gtkconfig,
  ]

  if os_unix
    gtk_introspection_sources += [ gtk_unix_print_headers + gtk_unix_print_main_header, gtk_unix_print_sources ]
  endif

  if os_win32
    gtk_introspection_sources += [ gtk_win32_print_headers, gtk_win32_print_sources ]
  endif

  gtk_gir = gnome.generate_gir(libgtk,
    sources: gtk_introspection_sources,
    namespace: 'Gtk',
    nsversion: gtk_api_version,
    identifier_prefix: 'Gtk',
    symbol_prefix: 'gtk',
    export_packages: 'gtk4',
    includes: [ gdk_gir[0], gsk_gir[0], ],
    header: 'gtk/gtk.h',
    install: true,
    dependencies: [gtk_deps, libgsk_dep],
    extra_args: gir_args,
    fatal_warnings: get_option('werror'),
  )
  gtk_dep_sources += gtk_gir
endif

libgtk_dep = declare_dependency(sources: gtk_dep_sources,
  include_directories: [confinc, gtkinc],
  dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
  link_with: libgtk,
  link_args: common_ldflags,
)

libgtk_static_dep = declare_dependency(sources: gtk_dep_sources,
  include_directories: [confinc, gtkinc],
  dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
  link_with: [libgtk_static, libgtk_css, libgdk, libgsk ],
  link_args: common_ldflags,
)

subdir('compose')
