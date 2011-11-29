plugin_LTLIBRARIES = libgstkuscheduler.la

# sources used to compile this plug-in
libgstkuscheduler_la_SOURCES = \
	gstkeyunitsscheduler.c

# headers we need but don't want installed
noinst_HEADERS = \
	gstkeyunitsscheduler.h

libgstkuscheduler_la_CFLAGS = \
	$(GST_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS)

libgstkuscheduler_la_LIBADD = \
	$(GST_LIBS) \
	$(GST_BASE_LIBS) \
	-lgstvideo-@GST_MAJORMINOR@ \
	$(GST_PLUGINS_BASE_LIBS)

libgstkuscheduler_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstkuscheduler_la_LIBTOOLFLAGS = --tag=disable-static

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstkuscheduler -:SHARED libgstkuscheduler \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstkuscheduler_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstkuscheduler_la_CFLAGS) \
	 -:LDFLAGS $(libgstkuscheduler_la_LDFLAGS) \
	           $(libgstkuscheduler_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
