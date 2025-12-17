We currently have three proxy backends:

 * environment, used if one of several proxy environment variables are set
 * gnome, used if "GNOME" is present in `$XDG_CURRENT_DESKTOP`
 * libproxy, used otherwise

The environment backend is based on how libproxy handles proxy environment
variables, which has some differences from how other programs (e.g. libcurl)
might interpret these variables. In particular, the `http_proxy` or `HTTP_PROXY`
variables are used for *all* protocols, not just HTTP. This backend allows for
basic proxy support even if running outside GNOME and built without libproxy.

The GNOME backend uses the GNOME proxy settings, currently located under
/system/proxy in dconf. Because all /system settings are deprecated, these
settings will probably move somewhere else in the future. See
https://gitlab.gnome.org/GNOME/gsettings-desktop-schemas/-/issues/27.

Finally, libproxy provides the best level of proxy configuration support,
including support for environments like Windows or KDE, which have their own
system proxy settings. But libproxy has some serious problems that are difficult
to fix, e.g. https://github.com/libproxy/libproxy/issues/81. Accordingly, this
backend is used only if the other two are not suitable.

The GNOME and libproxy backends can both be disabled at build time, if desired.
