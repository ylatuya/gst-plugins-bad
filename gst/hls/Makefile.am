plugin_LTLIBRARIES = libgsthls.la

# sources used to compile this plug-in
libgsthls_la_SOURCES = \
	gstkeyunitsscheduler.c

# headers we need but don't want installed
noinst_HEADERS = \
	gstkeyunitsscheduler.h

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsthls_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
libgsthls_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS)
libgsthls_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthls_la_LIBTOOLFLAGS = --tag=disable-static

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgsthls -:SHARED libgsthls \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgsthls_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgsthls_la_CFLAGS) \
	 -:LDFLAGS $(libgsthls_la_LDFLAGS) \
	           $(libgsthls_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
