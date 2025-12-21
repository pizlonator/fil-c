# FFT Library

The gstfft library is based on
[kissfft](http://sourceforge.net/projects/kissfft) by Mark Borgerding.

This library should be linked to by getting cflags and libs from
`gstreamer-plugins-base-{{ gst_api_version.md }}.pc` and adding
`-lgstfft-{{ gst_api_version.md }}` to the library flags.
