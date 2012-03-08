
plugin_LTLIBRARIES = libgstvcdsrc.la

libgstvcdsrc_la_SOURCES = vcdsrc.c
libgstvcdsrc_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) 
libgstvcdsrc_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS)
libgstvcdsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstvcdsrc_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = vcdsrc.h
