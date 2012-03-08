plugin_LTLIBRARIES = libgstdivxdec.la libgstdivxenc.la

AM_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(DIVX_CFLAGS)

libgstdivxenc_la_SOURCES = gstdivxenc.c
libgstdivxenc_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(DIVXENC_LIBS)
libgstdivxenc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstdivxenc_la_LIBTOOLFLAGS = --tag=disable-static
endif

libgstdivxdec_la_SOURCES = gstdivxdec.c
libgstdivxdec_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(DIVXDEC_LIBS)
libgstdivxdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstdivxdec_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstdivxenc.h gstdivxdec.h
