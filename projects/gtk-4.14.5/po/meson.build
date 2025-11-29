i18n = import('i18n')

xgettext_args = [
  '--msgid-bugs-address="https://gitlab.gnome.org/GNOME/gtk/-/issues/"',
  '--add-comments',
  '--from-code=utf-8',
  '--flag=g_dngettext:2:pass-c-format',
  '--flag=g_strdup_printf:1:c-format',
  '--flag=g_string_printf:2:c-format',
  '--flag=g_string_append_printf:2:c-format',
  '--flag=g_error_new:3:c-format',
  '--flag=g_set_error:4:c-format',
  '--flag=g_markup_printf_escaped:1:c-format',
  '--flag=g_log:3:c-format',
  '--flag=g_print:1:c-format',
  '--flag=g_printerr:1:c-format',
  '--flag=g_printf:1:c-format',
  '--flag=g_fprintf:2:c-format',
  '--flag=g_sprintf:2:c-format',
  '--flag=g_snprintf:3:c-format',
  '--flag=g_scanner_error:2:c-format',
  '--flag=g_scanner_warn:2:c-format',
  '--flag=gtk_message_dialog_format_secondary_markup:2:c-format',
  '--flag=gtk_message_dialog_format_secondary_text:2:c-format',
  '--flag=gtk_message_dialog_new:5:c-format',
  '--flag=gtk_message_dialog_new_with_markup:5:c-format',
  '--flag=gtk_alert_dialog_new:1:c-format',
]

i18n.gettext('gtk40', args: xgettext_args)
