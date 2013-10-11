
lib_LTLIBRARIES = libgstandroidjni-@GST_MAJORMINOR@.la

CLEANFILES = $(BUILT_SOURCES)

libgstandroidjni_@GST_MAJORMINOR@_la_SOURCES = \
	gstjniutils.c \
	gstjniamcdirectbuffer.c \
	gstjnisurface.c \
	gstjnisurfacetexture.c

libgstandroidjni_@GST_MAJORMINOR@includedir = \
	$(includedir)/gstreamer-@GST_MAJORMINOR@/gst/androidjni

libgstandroidjni_@GST_MAJORMINOR@include_HEADERS = \
	gstjniutils.h \
	gstjniamcdirectbuffer.h \
	gstjnisurface.h \
	gstjnisurfacetexture.h

libgstandroidjni_@GST_MAJORMINOR@_la_CFLAGS = \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS)

libgstandroidjni_@GST_MAJORMINOR@_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)

libgstandroidjni_@GST_MAJORMINOR@_la_LDFLAGS = \
	$(GST_LIB_LDFLAGS) \
	$(GST_ALL_LDFLAGS) \
	$(GST_LT_LDFLAGS)
