plugin_LTLIBRARIES = libgstcdaudio.la

libgstcdaudio_la_SOURCES = gstcdaudio.c 
libgstcdaudio_la_CFLAGS = $(GST_CFLAGS)
libgstcdaudio_la_LIBADD = $(GST_LIBS)
libgstcdaudio_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lcdaudio
if !GST_PLUGIN_BUILD_STATIC
libgstcdaudio_la_LIBTOOLFLAGS = --tag=disable-static
endif

