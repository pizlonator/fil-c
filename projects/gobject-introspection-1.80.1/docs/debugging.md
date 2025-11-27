Debugging
---------

You can use the `GI_SCANNER_DEBUG` environment variable; see
[`utils.py`](giscanner/utils.py) for a list of debug flags.

It is possible to debug the various `g-ir-*` tools using PyDev's remote
debugger. To do this:

- locate the directory that contains the pydevd.py file matching the PyDev
  version running in your Eclipse instance. This might look like:

    /usr/lib64/eclipse/dropins/pydev-core/eclipse/plugins/org.python.pydev_4.0.0.201505131500/pysrc/)

- open the "Debug" perspective in your Eclipse instance, open the "PyDev"
  menu and start the debug server
- execute the `g-ir-*` tool setting the `GI_SCANNER_DEBUG` and `PYDEVDPATH`
  environment variables. For example:

    GI_SCANNER_DEBUG="pydevd" \
    PYDEVDPATH="/usr/lib64/eclipse/dropins/pydev-core/eclipse/plugins/org.python.pydev_4.0.0.201505131500/pysrc/" \
    g-ir-scanner <parameters>

- In the example above, the PyDev debugger will run `g-ir-scanner` and stop
  execution in the main script. You can then set extra breakpoints, step
  through the code, inspect variables at the current scope, inspect the
  stack and so forth.
