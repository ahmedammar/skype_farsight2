plugin_LTLIBRARIES = libfsutils.la

libfsutils_la_SOURCES = fsu.c \
			fsu-audio-source.c \
			fsu-video-source.c \
			fsu-audio-sink.c \
			fsu-video-sink.c

libfsutils_la_CFLAGS = \
	$(FS2_CFLAGS) \
	$(FS2_INTERNAL_CFLAGS) \
	$(GST_CFLAGS)
libfsutils_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libfsutils_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/farsight/libgstfarsight-0.10.la \
	$(FS2_LIBS) \
	$(GST_LIBS)

noinst_HEADERS = fsu.h \
			fsu-audio-source.h \
			fsu-video-source.h \
			fsu-audio-sink.h \
			fsu-video-sink.h
