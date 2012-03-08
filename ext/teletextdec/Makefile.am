plugin_LTLIBRARIES = libgstteletextdec.la

libgstteletextdec_la_SOURCES = gstteletextdec.c teletext.c

libgstteletextdec_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(TELETEXTDEC_CFLAGS)
libgstteletextdec_la_LIBADD = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_LIBS) $(TELETEXTDEC_LIBS)
libgstteletextdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstteletextdec_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstteletextdec.h
