
noinst_PROGRAMS = test-video-filters

if HAVE_GTK
 noinst_PROGRAMS += grid-effects
endif

AM_CFLAGS = \
	$(FS2_INTERNAL_CFLAGS) \
	$(FS2_CFLAGS) \
	$(GST_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CFLAGS)

LDADD = \
	$(top_builddir)/gst-libs/gst/farsight/libgstfarsight-0.10.la \
	$(GTK_LIBS) \
	$(GST_LIBS) 
