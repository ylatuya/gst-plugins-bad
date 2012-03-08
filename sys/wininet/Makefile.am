plugin_LTLIBRARIES = libgstwininet.la

libgstwininet_la_SOURCES = gstwininetsrc.c

libgstwininet_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstwininet_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) -lwininet
libgstwininet_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstwininet_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstwininetsrc.h

