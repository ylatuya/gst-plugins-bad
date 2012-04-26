plugin_LTLIBRARIES = libgstvidroidsink.la

libgstvidroidsink_la_SOURCES = gstvidroidsink.c video_platform_wrapper.c

libgstvidroidsink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
                              $(GST_CFLAGS) \
                              $(DROIDSINKS_CFLAGS) \
                              $(X11_CFLAGS)

libgstvidroidsink_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) $(DROIDSINKS_LIBS) $(X11_LIBS) \
	-lgstvideo-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR)

libgstvidroidsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvidroidsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstvidroidsink.h video_platform_wrapper.h
