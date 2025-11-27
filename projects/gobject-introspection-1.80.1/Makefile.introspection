# -*- Mode: make -*-
# Copyright 2009-2010 Johan Dahlin
#
# This file is free software; the author(s) gives unlimited
# permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# * Input variables:
#
#   INTROSPECTION_GIRS - List of GIRS that should be generated
#   INTROSPECTION_SCANNER - Command to invoke scanner, normally set by
#      GOBJECT_INTROSPECTION_REQUIRE/CHECK() in introspection.m4
#   INTROSPECTION_SCANNER_ARGS - Additional args to pass in to the scanner
#   INTROSPECTION_SCANNER_ENV - Environment variables to set before running
#      the scanner
#   INTROSPECTION_COMPILER - Command to invoke compiler, normally set by
#      GOBJECT_INTROSPECTION_REQUIRE/CHECK() in introspection.m4
#   INTROSPECTION_COMPILER_ARGS - Additional args to pass in to the compiler
#
# * Simple tutorial
#
# Add this to configure.ac:
#   -Wno-portability to AM_INIT_AUTOMAKE
#   GOBJECT_INTROSPECTION_CHECK([0.6.7])
#
# Add this to Makefile.am where your library/program is built:
#   include $(INTROSPECTION_MAKEFILE)
#   INTROSPECTION_GIRS = YourLib-1.0.gir
#   YourLib-1.0.gir: libyourlib.la
#   YourLib_1_0_gir_NAMESPACE = YourLib
#   YourLib_1_0_gir_VERSION = 1.0
#   YourLib_1_0_gir_LIBS = libyourlib.la
#   YourLib_1_0_gir_FILES = $(libyourlib_1_0_SOURCES)
#   girdir = $(datadir)/gir-1.0
#   dist_gir_DATA = YourLib-1.0.gir
#   typelibdir = $(libdir)/girepository-1.0
#   typelib_DATA = YourLib-1.0.typelib
#   CLEANFILES = $(dist_gir_DATA) $(typelib_DATA)
#

# Make sure the required variables are set, these should under normal
# circumstances come from introspection.m4
$(if $(INTROSPECTION_SCANNER),,$(error Need to define INTROSPECTION_SCANNER))
$(if $(INTROSPECTION_COMPILER),,$(error Need to define INTROSPECTION_COMPILER))

# Private functions

## Transform the gir filename to something which can reference through a variable
## without automake/make complaining, eg Gtk-2.0.gir -> Gtk_2_0_gir
_gir_name = $(subst /,_,$(subst -,_,$(subst .,_,$(1))))

# Namespace and Version is either fetched from the gir filename
# or the _NAMESPACE/_VERSION variable combo
_gir_namespace = $(or $($(_gir_name)_NAMESPACE),$(firstword $(subst -, ,$(notdir $(1)))))
_gir_version = $(or $($(_gir_name)_VERSION),$(lastword $(subst -, ,$(1:.gir=))))

# _PROGRAM is an optional variable which needs it's own --program argument
_gir_program = $(if $($(_gir_name)_PROGRAM),--program=$($(_gir_name)_PROGRAM))

# Variables which provides a list of things
_gir_libraries = $(foreach lib,$($(_gir_name)_LIBS),--library=$(lib))
_gir_packages = $(foreach pkg,$($(_gir_name)_PACKAGES),--pkg=$(pkg))
_gir_includes = $(foreach include,$($(_gir_name)_INCLUDES),--include=$(include))
_gir_export_packages = $(foreach pkg,$($(_gir_name)_EXPORT_PACKAGES),--pkg-export=$(pkg))
_gir_c_includes = $(foreach include,$($(_gir_name)_C_INCLUDES),--c-include=$(include))

# Reuse the LIBTOOL variable from automake if it's set
_gir_libtool = $(if $(LIBTOOL),--libtool="$(LIBTOOL)")

# Macros for AM_SILENT_RULES prettiness
_gir_verbosity = $(if $(AM_DEFAULT_VERBOSITY),$(AM_DEFAULT_VERBOSITY),1)

_gir_silent_scanner_prefix = $(_gir_silent_scanner_prefix_$(V))
_gir_silent_scanner_prefix_ = $(_gir_silent_scanner_prefix_$(_gir_verbosity))
_gir_silent_scanner_prefix_0 = @echo "  GISCAN   $(1)";
_gir_silent_scanner_opts = $(_gir_silent_scanner_opts_$(V))
_gir_silent_scanner_opts_ = $(_gir_silent_scanner_opts_$(_gir_verbosity))
_gir_silent_scanner_opts_0 = --quiet

_gir_silent_compiler = $(_gir_silent_compiler_$(V))
_gir_silent_compiler_ = $(_gir_silent_compiler_$(_gir_verbosity))
_gir_silent_compiler_0 = @echo "  GICOMP   $(1)";

_gir_default_scanner_env = CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" CC="$(CC)" PKG_CONFIG="$(PKG_CONFIG)" GI_HOST_OS="$(GI_HOST_OS)" DLLTOOL="$(DLLTOOL)"

#
# Creates a GIR by scanning C headers/sources
# $(1) - Name of the gir file (output)
#
# If output is Gtk-2.0.gir then you should name the variables like
# Gtk_2_0_gir_NAMESPACE, Gtk_2_0_gir_VERSION etc.
# Required variables:
# FILES - C sources and headers which should be scanned
#
# One of these variables are required:
# LIBS - Library where the symbol represented in the gir can be found
# PROGRAM - Program where the symbol represented in the gir can be found
#
# Optional variables
# NAMESPACE - Namespace of the gir, first letter capital,
#   rest should be lower case, for instance: 'Gtk', 'Clutter', 'ClutterGtk'.
#   If not present the namespace will be fetched from the gir filename,
#   the part before the first dash. For 'Gtk-2.0', namespace will be 'Gtk'.
# VERSION - Version of the gir, if not present, will be fetched from gir
# filename, the part after the first dash. For 'Gtk-2.0', version will be '2.0'.
# LIBTOOL - Command to invoke libtool, usually set by automake
# SCANNERFLAGS - Flags to pass in to the scanner, see g-ir-scanner(1) for a list
# CFLAGS - Flags to pass in to the parser when scanning headers
# LDFLAGS - Linker flags used by the scanner
# PACKAGES - list of pkg-config names which cflags are required to parse
#   the headers of this gir
# INCLUDES - Gir files to include without the .gir suffix, for instance
#   GLib-2.0, Gtk-2.0. This is needed for all libraries which you depend on that
#   provides introspection information.
# EXPORT_PACKAGES - list of pkg-config names that are provided by this gir.
#   By default the names in the PACKAGES variable will be used.
# C_INCLUDES - List of public C headers which need to be included by
#   consumers at compile time to make use of the API
#

define introspection-scanner

# Basic sanity check, to make sure required variables are set
$(if $($(_gir_name)_FILES),,$(error Need to define $(_gir_name)_FILES))
$(if $(or $(findstring --header-only,$($(_gir_name)_SCANNERFLAGS)),
          $($(_gir_name)_LIBS),
          $($(_gir_name)_PROGRAM)),,
    $(error Need to define $(_gir_name)_LIBS or $(_gir_name)_PROGRAM))

# Only dependencies we know are actually filenames goes into _FILES, make
# sure these are built before running the scanner. Libraries and programs
# needs to be added manually.
$(1): $$($(_gir_name)_FILES)
	@ $(MKDIR_P) $(dir $(1))
	$(_gir_silent_scanner_prefix) $(_gir_default_scanner_env) $(INTROSPECTION_SCANNER_ENV) $(INTROSPECTION_SCANNER) $(_gir_silent_scanner_opts) \
	$(INTROSPECTION_SCANNER_ARGS) \
	  --namespace=$(_gir_namespace) \
	  --nsversion=$(_gir_version) \
	  $(_gir_libtool) \
	  $(_gir_packages) \
	  $(_gir_includes) \
	  $(_gir_export_packages) \
	  $(_gir_c_includes) \
	  $(_gir_program) \
	  $(_gir_libraries) \
	  $($(_gir_name)_SCANNERFLAGS) \
	  --cflags-begin \
	  $($(_gir_name)_CFLAGS) \
	  --cflags-end \
	  $($(_gir_name)_LDFLAGS) \
	  $$^ \
	  --output $(1)
endef

$(foreach gir,$(INTROSPECTION_GIRS),$(eval $(call introspection-scanner,$(gir))))

#
# Compiles a gir into a typelib
# $(1): gir filename (input)
# $(2): typelib filename (output)
#
define introspection-compiler
$(_gir_silent_compiler) $(INTROSPECTION_COMPILER) $(INTROSPECTION_COMPILER_ARGS) --includedir=. $(1) -o $(2)
endef

# Simple rule to compile a typelib.
%.typelib: %.gir
	$(call introspection-compiler,$<,$@)
