plugin_LTLIBRARIES = libgstmms.la 

libgstmms_la_SOURCES = gstmms.c

libgstmms_la_CFLAGS = $(GST_CFLAGS) $(LIBMMS_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
libgstmms_la_LIBADD = $(GST_LIBS) $(LIBMMS_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS)
libgstmms_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstmms_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstmms.h
