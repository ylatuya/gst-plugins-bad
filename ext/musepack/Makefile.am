plugin_LTLIBRARIES = libgstmusepack.la

libgstmusepack_la_SOURCES = \
	gstmusepackdec.c \
	gstmusepackreader.c
libgstmusepack_la_CFLAGS = $(MUSEPACK_CFLAGS) $(GST_CFLAGS)
libgstmusepack_la_LIBADD = $(MUSEPACK_LIBS) $(GST_LIBS)
libgstmusepack_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstmusepack_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = \
	gstmusepackdec.h \
	gstmusepackreader.h
