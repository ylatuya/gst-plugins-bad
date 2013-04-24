plugin_LTLIBRARIES = libgstapplemedia_nonpublic.la

libgstapplemedia_nonpublic_la_SOURCES =			\
	plugin.m				\
	bufferfactory.m				\
	corevideobuffer.c			\
	coremediabuffer.c			\
	coremediactx.c				\
	cvapi.c					\
	cmapi.c					\
	dynapi.c

libgstapplemedia_nonpublic_la_CFLAGS =			\
	$(GST_CFLAGS)			\
	$(GST_BASE_CFLAGS)			\
	$(GST_PLUGINS_BASE_CFLAGS)		\
	-Dgst_dyn_api_get_type=gst_dyn_api_priv_get_type			\
	-Dgst_dyn_api_debug=gst_dyn_api_priv_debug				\
	-D_gst_dyn_api_new=_gst_dyn_api_priv_new				\
	-Dgst_core_media_buffer_new=gst_core_media_buffer_priv_new		\
	-Dgst_core_media_buffer_get_type=gst_core_media_buffer_priv_get_type	\
	-Dgst_core_media_buffer_get_pixel_buffer=gst_core_media_buffer_priv_get_pixel_buffer\
	-Dgst_core_video_buffer_new=gst_core_video_buffer_priv_new		\
	-Dgst_core_video_buffer_get_type=gst_core_video_buffer_priv_get_type	\
	-Dgst_core_media_ctx_new=gst_core_media_ctx_priv_new			\
	-Dgst_core_media_ctx_get_type=gst_core_media_ctx_priv_get_type

libgstapplemedia_nonpublic_la_OBJCFLAGS =			\
	$(GST_OBJCFLAGS)			\
	$(GST_BASE_CFLAGS)			\
	$(GST_PLUGINS_BASE_CFLAGS)

libgstapplemedia_nonpublic_la_LIBADD =			\
	$(GST_BASE_LIBS)			\
	$(GST_PLUGINS_BASE_LIBS)		\
	-lgstinterfaces-$(GST_MAJORMINOR)	\
	-lgstvideo-$(GST_MAJORMINOR)

if !GST_PLUGIN_BUILD_STATIC
libgstapplemedia_nonpublic_la_LIBTOOLFLAGS = --tag=disable-static --tag=CC
endif

libgstapplemedia_nonpublic_la_LDFLAGS =			\
	$(GST_PLUGIN_LDFLAGS)			\
	$(GST_BASE_LDFLAGS)			\
	$(GST_PLUGINS_BASE_LDFLAGS)		\
	-Wl,-framework -Wl,Foundation		\
	-Wl,-framework -Wl,CoreFoundation	\
	-Wl,-framework -Wl,CoreVideo

noinst_HEADERS =				\
	miovideosrc.h				\
	miovideodevice.h			\
	celvideosrc.h				\
	bufferfactory.h				\
	corevideobuffer.h			\
	coremediabuffer.h			\
	coremediactx.h				\
	cvapi.h					\
	cmapi.h					\
	mioapi.h				\
	mtapi.h					\
	celapi.h				\
	dynapi.h				\
	dynapi-internal.h

if HAVE_IOS

libgstapplemedia_nonpublic_la_SOURCES +=			\
	celvideosrc.c				\
	mtapi.c					\
	celapi.c

libgstapplemedia_nonpublic_la_LDFLAGS +=			\
	-Wl,-framework -Wl,CoreMedia		\
	-Wl,-framework -Wl,Foundation

else

libgstapplemedia_nonpublic_la_SOURCES +=			\
	miovideosrc.c				\
	miovideodevice.c			\
	mioapi.c

endif

