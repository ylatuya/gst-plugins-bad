
plugin_LTLIBRARIES = libgstartsdsink.la

libgstartsdsink_la_SOURCES = gstartsdsink.c
libgstartsdsink_la_CFLAGS = $(GST_CFLAGS) $(ARTSC_CFLAGS)
libgstartsdsink_la_LIBADD = $(ARTSC_LIBS)
libgstartsdsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstartsdsink_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstartsdsink.h
