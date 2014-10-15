plugin_LTLIBRARIES = libgstapplemedia.la

libgstapplemedia_la_SOURCES =			\
	plugin.m				\
	corevideobuffer.c			\
	coremediabuffer.c			\
	coremediactx.c				\
	dynapi.c				\
	vtapi.c					\
	vtdec.c					\
	vtenc.c					\
	vth264decbin.c				\
	vth264encbin.c				\
	avfvideosrc.m \
	vtutil.c

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
	-Wl,-framework -Wl,CoreMedia		\
	-Wl,-framework -Wl,AVFoundation		\
	-Wl,-framework -Wl,CoreVideo

noinst_HEADERS =				\
	dynapi-internal.h			\
	dynapi.h				\
	coremediactx.h				\
	qtkitvideosrc.h				\
	avfvideosrc.h				\
	corevideobuffer.h			\
	coremediabuffer.h			\
	iosassetsrc.h				\
	vtapi.h					\
	vtdec.h					\
	vtenc.h					\
	vth264decbin.h				\
	vth264encbin.h				\
	iosavassetsrc.h				\
	vtutil.h

if HAVE_IOS

libgstapplemedia_la_SOURCES +=			\
	iosassetsrc.m \
	iosavassetsrc.m

libgstapplemedia_la_LDFLAGS +=			\
	-Wl,-framework -Wl,Foundation		\
	-Wl,-framework -Wl,AssetsLibrary

else

libgstapplemedia_la_SOURCES +=			\
	qtkitvideosrc.m

libgstapplemedia_la_LDFLAGS +=			\
	-Wl,-framework -Wl,Cocoa		\
	-Wl,-framework -Wl,QTKit

endif

