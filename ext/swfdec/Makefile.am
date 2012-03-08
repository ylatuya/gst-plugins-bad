plugin_LTLIBRARIES = libgstswfdec.la

libgstswfdec_la_SOURCES = gstswfdec.c
libgstswfdec_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(SWFDEC_CFLAGS)
libgstswfdec_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BLIBS) $(SWFDEC_LIBS)
libgstswfdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstswfdec_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstswfdec.h
