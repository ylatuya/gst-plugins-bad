
plugin_LTLIBRARIES = libgstoverlay.la

libgstoverlay_la_SOURCES = gstoverlay.c

noinst_HEADERS = gstoverlay.h

libgstoverlay_la_CFLAGS = $(GST_CFLAGS)
libgstoverlay_la_LIBADD =
libgstoverlay_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstoverlay_la_LIBTOOLFLAGS = --tag=disable-static
endif

EXTRA_DIST = 

