plugin_LTLIBRARIES = libfsutils.la

libfsutils_la_SOURCES = fsu-common.c \
			fsu-source.c \
			fsu-audio-source.c \
			fsu-video-source.c \
			fsu-sink.c \
			fsu-audio-sink.c \
			fsu-video-sink.c

libfsutils_la_CFLAGS = \
	$(FS2_CFLAGS) \
	$(FS2_INTERNAL_CFLAGS) \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS)
libfsutils_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libfsutils_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/farsight/libgstfarsight-0.10.la \
	$(FS2_LIBS) \
	$(GST_LIBS) \
	$(GST_INTERFACES_LIBS)

noinst_HEADERS = fsu-common.h
