# App Library

This library should be linked to by getting cflags and libs from
`gstreamer-plugins-base-{{ gst_api_version.md }}pc` and adding
-lgstapp-{{ gst_api_version.md }} to the library flags.

To use it the functionality, insert an `appsrc` or `appsink` element
into a pipeline and call the appropriate functions on the element.
