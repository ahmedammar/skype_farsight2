plugin_LTLIBRARIES = libfsutils.la

libfsutils_la_SOURCES = fsu-common.c \
			fsu-source.c \
			fsu-audio-source.c \
			fsu-video-source.c \
			fsu-sink.c \
			fsu-audio-sink.c

libfsutils_la_CFLAGS = \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS)
libfsutils_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libfsutils_la_LIBADD = \
	$(GST_LIBS) \
	$(GST_INTERFACES_LIBS)

noinst_HEADERS = fsu-common.h
