
plugin_LTLIBRARIES = libgstdash.la

libgstdash_la_SOURCES = \
        gstdashsink.c \
        gstdashmanager.c \
        mdp/gstperiod.c \
        mdp/gstadaptationset.c \
        mdp/gstmediacommon.c \
        mdp/gstmediapresentation.c \
        mdp/gstrepresentation.c \
        mdp/gstsegmentlist.c \
        mdp/gstxmlhelper.c

noinst_HEADERS = \
        gstdashsink.h \
        gstdashmanager.h \
        mdp/gstperiod.h \
        mdp/gstadaptationset.h \
        mdp/gstmediacommon.h \
        mdp/gstmediapresentation.h \
        mdp/gstrepresentation.h \
        mdp/gstsegmentlist.h \
        mdp/gstxmlhelper.h

libgstdash_la_CFLAGS = \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS) \
	$(LIBXML2_CFLAGS)

libgstdash_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS)\
	$(GST_BASE_LIBS)\
	$(GST_LIBS)\
	$(LIBXML2_LIBS)\
	$(top_builddir)/gst-libs/gst/baseadaptive/libgstbaseadaptive-$(GST_API_VERSION).la

libgstdash_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)
libgstdash_la_LIBTOOLFLAGS = --tag=disable-static

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstdash -:SHARED libgstdash \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstdash_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstdash_la_CFLAGS) \
	 -:LDFLAGS $(libgstdash_la_LDFLAGS) \
	           $(libgstdash_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
