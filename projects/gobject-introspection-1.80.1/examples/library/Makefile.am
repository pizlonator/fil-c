source_h = \
	gi-sample.h	\
	$(NULL)

source_c = \
	gi-sample.c		\
	$(NULL)

lib_LTLIBRARIES =
lib_LTLIBRARIES += libgi-sample.la

libgi_sample_la_CFLAGS = $(GISAMPLE_CFLAGS)
libgi_sample_la_LIBADD = $(GISAMPLE_LIBS)
libgi_sample_la_SOURCES = $(source_c) $(source_h)
libgi_sample_la_LDFLAGS = -export-dynamic

-include $(INTROSPECTION_MAKEFILE)

DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

INTROSPECTION_GIRS = GISample-1.0.gir

GISample-1.0.gir: libgi-sample.la

GISample_1_0_gir_NAMESPACE = GISample
GISample_1_0_gir_VERSION = 1.0
GISample_1_0_gir_LIBS = libgi-sample.la
GISample_1_0_gir_FILES = $(source_c) $(source_h)
GISample_1_0_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) $(GISAMPLE_CFLAGS)
GISample_1_0_gir_INCLUDES = GObject-2.0
GISample_1_0_gir_SCANNERFLAGS = --warn-all --symbol-prefix=gi_sample

girdir = $(datadir)/gir-1.0
dist_gir_DATA = GISample-1.0.gir

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = GISample-1.0.typelib

CLEANFILES =
CLEANFILES += $(dist_gir_DATA) $(typelibs_DATA)
