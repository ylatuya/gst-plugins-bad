plugin_LTLIBRARIES = libgstapexsink.la

libgstapexsink_la_SOURCES = gstapexplugin.c gstapexraop.c gstapexsink.c
libgstapexsink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(APEXSINK_CFLAGS)
libgstapexsink_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
	-lgstaudio-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(APEXSINK_LIBS)
libgstapexsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstapexsink_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstapexraop.h gstapexsink.h
