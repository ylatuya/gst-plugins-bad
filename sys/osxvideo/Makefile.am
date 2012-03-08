
plugin_LTLIBRARIES = libgstosxvideosrc.la

libgstosxvideosrc_la_SOURCES = osxvideoplugin.c osxvideosrc.c
libgstosxvideosrc_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) -Wno-deprecated-declarations
libgstosxvideosrc_la_LIBADD =  \
	$(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR)

if !GST_PLUGIN_BUILD_STATIC
libgstosxvideosrc_la_LIBTOOLFLAGS = --tag=disable-static
endif

libgstosxvideosrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) \
	-Wl,-framework -Wl,Cocoa -Wl,-framework -Wl,QuickTime

noinst_HEADERS = osxvideosrc.h
