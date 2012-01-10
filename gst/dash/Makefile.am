
plugin_LTLIBRARIES = libgstdash.la

libgstdash_la_SOURCES = \
        gstdashsink.c \
        gstmediamanager.c \
        mdp/gstperiod.c \
        mdp/gstgroup.c \
        mdp/gstmediacommon.c \
        mdp/gstmediapresentation.c \
        mdp/gstrepresentation.c \
        mdp/gstsegmentinfo.c \
        mdp/gstxmlhelper.c

noinst_HEADERS = \
        gstdashsink.h \
        gstmediamanager.h \
        mdp/gstperiod.h \
        mdp/gstgroup.h \
        mdp/gstmediacommon.h \
        mdp/gstmediapresentation.h \
        mdp/gstrepresentation.h \
        mdp/gstsegmentinfo.h \
        mdp/gstxmlhelper.h

libgstdash_la_CFLAGS = \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS)
libgstdash_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS)\
	-lgstbaseadaptive-@GST_MAJORMINOR@\
	$(GST_BASE_LIBS)\
	$(GST_LIBS)

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
