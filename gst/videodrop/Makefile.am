
plugin_LTLIBRARIES = libgstvideodrop.la

libgstvideodrop_la_SOURCES = \
			gstvideodrop.c
libgstvideodrop_la_CFLAGS = $(GST_CFLAGS)
libgstvideodrop_la_LIBADD =
libgstvideodrop_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstvideodrop_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstvideodrop.h
