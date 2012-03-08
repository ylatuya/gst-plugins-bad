
plugin_LTLIBRARIES = libgstshout.la

libgstshout_la_SOURCES = gstshout.c 
libgstshout_la_CFLAGS = $(GST_CFLAGS)
libgstshout_la_LIBADD = $(SHOUT_LIBS)
libgstshout_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstshout_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstshout.h
