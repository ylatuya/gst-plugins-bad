
plugin_LTLIBRARIES = libgstavc.la

libgstavc_la_SOURCES = gstavcplugin.cpp gstavcsrc.cpp
libgstavc_la_CPPFLAGS = \
	$(GST_PLUGINS_BAD_CXXFLAGS) \
	$(GST_PLUGINS_BASE_CXXFLAGS) \
	$(GST_CXXFLAGS) \
	-framework AVCVideoServices \
	-Wno-deprecated-declarations
libgstavc_la_LIBADD =  \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)

if !GST_PLUGIN_BUILD_STATIC
libgstavc_la_LIBTOOLFLAGS = --tag=disable-static
endif

libgstavc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) \
	-Wl,-framework -Wl,AVCVideoServices \
	-Wl,-framework -Wl,Cocoa \
	-Wl,-framework -Wl,QuickTime

noinst_HEADERS = gstavcsrc.h

