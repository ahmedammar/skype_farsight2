
noinst_PROGRAMS = test-level

if HAVE_GTK
if HAVE_GNOME_EFFECTS
 noinst_PROGRAMS += test-video-filters grid-effects grid-effects2
endif
endif

AM_CFLAGS = \
	$(FS2_INTERNAL_CFLAGS) \
	$(FS2_CFLAGS) \
	$(GST_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CFLAGS)

LDADD = \
	$(top_builddir)/gst-libs/gst/farsight/libgstfarsight-0.10.la \
	$(top_builddir)/gst-libs/gst/filters/libfsufilters-0.10.la \
	$(GTK_LIBS) \
	$(GST_LIBS) 
