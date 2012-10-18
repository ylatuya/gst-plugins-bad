glib_gen_prefix = __gst_adaptive
glib_gen_basename = gstadaptive

include $(top_srcdir)/common/gst-glib-gen.mak

built_sources = gstadaptive-marshal.c
built_headers = gstadaptive-marshal.h

BUILT_SOURCES = $(built_sources) $(built_headers)
CLEANFILES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libgstbaseadaptive-@GST_API_VERSION@.la

libgstbaseadaptive_@GST_API_VERSION@_la_SOURCES = \
        gstfragment.c \
        gststreamsmanager.c \
        gstbaseadaptivesink.c \
	$(built_sources)

libgstbaseadaptive_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_API_VERSION@/gst/baseadaptive
libgstbaseadaptive_@GST_API_VERSION@include_HEADERS = \
        gstadaptive.h \
        gstfragment.h \
        gststreamsmanager.h \
        gstbaseadaptivesink.h \
	$(built_headers)

libgstbaseadaptive_@GST_API_VERSION@_la_CFLAGS = \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS)\
	$(GIO_CFLAGS)

libgstbaseadaptive_@GST_API_VERSION@_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS)\
	$(GST_PLUGINS_BAD_LIBS) \
	-lgstvideo-@GST_API_VERSION@ \
	-lgstpbutils-@GST_API_VERSION@ \
	-lgstapp-@GST_API_VERSION@ \
	$(GST_BASE_LIBS)\
	$(GST_LIBS) \
	$(GIO_LIBS)

libgstbaseadaptive_@GST_API_VERSION@_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS) $(GIO_LDFLAGS)

Android.mk: Makefile.am
	androgenizer -:PROJECT libgstbaseadaptive -:STATIC libgstbaseadaptive-@GST_API_VERSION@ \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstbaseadaptive_@GST_API_VERSION@_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(libgstbaseadaptive_@GST_API_VERSION@_la_CFLAGS) \
	 -:LDFLAGS $(libgstbaseadaptive_@GST_API_VERSION@_la_LDFLAGS) \
	           $(libgstbaseadaptive_@GST_API_VERSION@_la_LIBADD) \
	           -ldl \
	 -:LIBFILTER_STATIC gstphotography-@GST_API_VERSION@ \
	 -:HEADER_TARGET gstreamer-@GST_API_VERSION@/gst/baseadaptive \
	 -:HEADERS $(libgstbaseadaptiveinclude_HEADERS) \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
	> $@
