plugin_LTLIBRARIES = libgstdirectsoundsrc.la

libgstdirectsoundsrc_la_SOURCES =  gstdirectsoundsrc.c gstdirectsoundplugin.c
libgstdirectsoundsrc_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) $(DIRECTX_CFLAGS)
libgstdirectsoundsrc_la_LIBADD = \
	$(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) -lgstaudio-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR) \
	$(DIRECTX_LDFLAGS) -ldsound
libgstdirectsoundsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstdirectsoundsrc_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS= gstdirectsoundsrc.h
