atspi_xml = [
  'Accessible.xml',
  'Action.xml',
  'Application.xml',
  'Cache.xml',
  'Collection.xml',
  'Component.xml',
  'DeviceEventController.xml',
  'DeviceEventListener.xml',
  'Document.xml',
  'EditableText.xml',
  'Event.xml',
  'Hyperlink.xml',
  'Hypertext.xml',
  'Image.xml',
  'Registry.xml',
  'Selection.xml',
  'Socket.xml',
  'Table.xml',
  'TableCell.xml',
  'Text.xml',
  'Value.xml',
]

gdbus_codegen = find_program('gdbus-codegen')
atspi_src = []
foreach xml: atspi_xml
  obj_name = xml.split('.').get(0)

  # We cannot use gnome.gdbus_codegen() directly because we only care about
  # the interface definitions, not the whole GTypeInterface/GObject proxy
  # classes
  gen_hdr = custom_target('atspi-' + obj_name.to_lower() + '-hdr',
    input: xml,
    output: 'atspi-' + obj_name.to_lower() + '.h',
    command: [
      gdbus_codegen,
      '--interface-prefix=org.a11y.atspi',
      '--c-namespace=Atspi',
      '--pragma-once',
      '--interface-info-header',
      '--output=@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: true,
  )

  gen_src = custom_target('atspi-' + obj_name.to_lower() + '-src',
    input: xml,
    output: 'atspi-' + obj_name.to_lower() + '.c',
    command: [
      gdbus_codegen,
      '--interface-prefix=org.a11y.atspi',
      '--c-namespace=Atspi',
      '--interface-info-body',
      '--output=@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: true,
  )

  atspi_src += [gen_src, gen_hdr]
endforeach

gtk_a11y_src += atspi_src
