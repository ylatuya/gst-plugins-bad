plugin_LTLIBRARIES = libgstapplemedia.la

libgstapplemedia_la_SOURCES =			\
	plugin.m				\
	bufferfactory.m				\
	corevideobuffer.c			\
	coremediabuffer.c

libgstapplemedia_la_CFLAGS =			\
	$(GST_CFLAGS)			\
	$(GST_BASE_CFLAGS)			\
	$(GST_PLUGINS_BASE_CFLAGS)

libgstapplemedia_la_OBJCFLAGS =			\
	$(GST_OBJCFLAGS)			\
	$(GST_BASE_CFLAGS)			\
	$(GST_PLUGINS_BASE_CFLAGS)

libgstapplemedia_la_LIBADD =			\
	$(GST_BASE_LIBS)			\
	$(GST_PLUGINS_BASE_LIBS)		\
	-lgstinterfaces-$(GST_MAJORMINOR)	\
	-lgstvideo-$(GST_MAJORMINOR)

if !GST_PLUGIN_BUILD_STATIC
libgstapplemedia_la_LIBTOOLFLAGS = --tag=disable-static --tag=CC
endif

libgstapplemedia_la_LDFLAGS =			\
	$(GST_PLUGIN_LDFLAGS)			\
	$(GST_BASE_LDFLAGS)			\
	$(GST_PLUGINS_BASE_LDFLAGS)		\
	-Wl,-framework -Wl,CoreFoundation	\
	-Wl,-framework -Wl,CoreVideo

noinst_HEADERS =				\
	qtkitvideosrc.h				\
	avfvideosrc.h				\
	bufferfactory.h				\
	corevideobuffer.h			\
	coremediabuffer.h

if HAVE_IOS

libgstapplemedia_la_SOURCES +=			\
	avfvideosrc.m

libgstapplemedia_la_LDFLAGS +=			\
	-Wl,-framework -Wl,CoreMedia		\
	-Wl,-framework -Wl,Foundation		\
	-Wl,-framework -Wl,AVFoundation

else

libgstapplemedia_la_SOURCES +=			\
	qtkitvideosrc.m

libgstapplemedia_la_LDFLAGS +=			\
	-Wl,-framework -Wl,Cocoa		\
	-Wl,-framework -Wl,QTKit

endif

