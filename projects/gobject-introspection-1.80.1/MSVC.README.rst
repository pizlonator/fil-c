Building GObject-Introspection (G-I) on Windows using Visual Studio
===================================================================

Building G-I on Windows with Visual Studio is now supported via the use
of the Meson build system, the Visual Studio 2008~2017 projects have been
dropped from the distribution.  This README.msvc file will outline the
steps of performing such a build on Visual Studio 2008~2017.

You will need the following, in addition to your Visual Studio installation:

- A Python installation that matches the build configuration that you are
  planning to build.  That is, you need the 32-bit Python installation for
  building 32-bit builds of G-I and the 64-bit (amd64) Python installation
  for building x64/x86_64/amd64 builds of G-I.  You need at least the version
  that is required for Meson, which is 3.4.x at the time of writing.  You will
  then need to install Meson using the pip tool.

- The Ninja build tool, which is optional on Visual Studio 2010, 2015 and 2017
  builds (if one chooses to generate Visual Studio projects with Meson), but is
  required for 2008, 2012 and 2013 builds.

- GIT for Windows is recommended, as Meson will download using GIT the sources
  of libraries that it depends on, if they cannot be found (such as GLib, please
  see below)

- A recent enough version of GLib, preferably built with the same compiler that
  is now being used to build G-I.  If none is found, the Meson build will fetch
  GLib from the tip of the main branch and build it first before continuing to
  build G-I.  Note that it will require a libintl implementation (must be installed
  beforehand, the headers and lib and DLL must be found in the paths specified by
  %INCLUDE%, %LIB% and %PATH% respectively), along with ZLib and libffi (GLib's
  own Meson build will build  them if they are not found).  If a pre-existing copy
  of GLib is available, be sure to set PKG_CONFIG_PATH to where its .pc files can
  be found, and ensure that its DLLs/ executables can be first found in the paths
  specified in %PATH%. Note that libffi is needed both by GLib and G-I.

- A DLL build of Cairo with Cairo-GObject built, also preferably with the same
  compiler that is now used to build G-I.

- A recent version of winflex/flex and Bison. They can be obtained via MSYS/MSYS64
  or Cygwin installations.  Either the 32-bit or 64-bit version will work, as long
  as they run on your system.

Set PATH to contain your Python-3.4.x+ interpreter, Ninja build tool (if needed) and
winflex/flex and Bison executables towards its end.  Please note that if using Python
2.7.x (such as with the case of building with Visual Studio 2008), the PATH variable
needs to contain the native Windows Python 2.7.x installation path as well, before
the path where the Flex and Bison executables are, if using the Flex and Bison
executables from MSYS2 or Cygwin, as the copy of the Python interpreter from MSYS2
and Cygwin will likely conflict with the installation of the native Windows Python
2.7.x.  Please note that building against MSYS2 or Cygwin Python with Visual Studio
builds is not (and will likely never be) supported.

Open a Visual Studio command prompt and create an empty build directory (which needs
to be on the same drive as the G-I sources).  In that directory, run the following::

  python $(PythonInstallationPath)\scripts\meson.py $(G-I_srcdir) --buildtype=<build_configuration> --prefix=$(PREFIX) -Dcairo_libname=<DLL filename of cairo-gobject> -Dpython=<full path to Python interpreter to build _giscanner.pyd>

The -Dcairo-libname is likely necessary as the default DLL file name for Cairo-GObject
may likely not match the default "libcairo-gobject-2.dll", which is the default
DLL filename for Cairo-GObject that is built with MinGW/mingw-w64.

The -Dpython is likely necessary when using multiple Python installations on the
system and is necessary when building with Visual Studio 2008~2013 when building
with later versions of Meson (which requires Python 3.5+), due to CRT differences.
Note that for this setting, Python-2.7.x or Python-3.4.x or later is supported.

When Meson completes configuring and generating the build files, proceed building
using Ninja or the generated Visual Studio projects.

Additional notes for building and running against Python 3.8.x and later
------------------------------------------------------------------------
Python 3.8.x and later made restrictions on where DLLs are searched for third-party
modules, which will therefore affect how the Python tools in ``tools/`` look for dependent
DLLs, as they rely on a C Python module, _giscanner.pyd, as the paths in %PATH% are
no longer referred to, except for system-supplied DLLs in their designated locations
on the system.  In order to cope with this, DLLs are being searched for in the
locations indicated by the 'bindir' entry in the pkg-config files that are being
required for the individual packages, followed by locations (note the plural form-multiple
paths are supported by GI_EXTRA_BASE_DLL_DIRS, separated by Python's os.pathsep, which is
';' on Windows cmd.exe) that are indicated through the envvar GI_EXTRA_BASE_DLL_DIRS.
This means, if there are any DLLs required (including their dependent non-system DLLs) for
the .gir files being generated or queried, they must be in the locations indicated by the
'bindir' entries in the dependent packages' .pc files of the current package, and/or in
the locations indicated by GI_EXTRA_BASE_DLL_DIRS.

Additional notes for building with Visual Studio 2008 only
----------------------------------------------------------
Due to its use of security manifests, after Meson completes configuring and
generating the build files, first build the giscanner\_giscanner.pyd target
using 'ninja giscanner\_giscanner.pyd'.  After doing so, embed the manifest
using the following command::

  mt /manifest giscanner\_giscanner.pyd.manifest /outputresource:giscanner\_giscanner.pyd;2

The .gir files cannot be successfully generated without this step.

On x64 builds, building girepository\girepository-1.0-1.dll may hang during compilation.
If this happens, terminate all 'cl.exe' processes, which will terminate the build process.
Open build.ninja and change the compiler flags by changing /O2 to /O1 for all the sources
that hang during compilation.  At this time of writing, girepository\girwriter.c,
girepository\girparser.c, girepository\girnode.c and tests\repository\gitypelibtest.c are
affected--this is due to an issue in Visual Studio 2008's x64 compiler in regards to
optimization.  Re-attempt the build, and the build should complete normally.  This does not
affect Win32/x86 builds.

On all Visual Studio 2008 builds, after successfully completing/installing the build, run
the following so that we ensure the manifests are embedded to the built DLLs and EXEs::

  for /r %f in (*.dll.manifest) do if exist $(PREFIX)\bin\%~nf mt /manifest %f /outputresource:$(PREFIX)\bin\%~nf;2
  for /r %f in (*.exe.manifest) do if exist $(PREFIX)\bin\%~nf mt /manifest %f /outputresource:$(PREFIX)\bin\%~nf;1

Additional notes on using clang-cl (LLVM/CLang's Visual Studio compiler emulation)
----------------------------------------------------------------------------------
Support has been added to build GObject-Introspection with clang-cl, specifically for
running g-ir-scanner with clang-cl and lld-link as the compiler and linker.  To enable
such support, you need to set *both* the environment variables CC and CXX to clang-cl
prior to building GObject-Introspection or running g-ir-scanner.  This is in line with
building with clang-cl in place of using the stock Visual Studio compiler to perform
builds with the Meson build system.
