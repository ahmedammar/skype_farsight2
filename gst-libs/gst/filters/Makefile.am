libfsufiltersincludedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/filters

libfsufiltersinclude_HEADERS = \
		fsu-filter-manager.h \
		fsu-single-filter-manager.h \
		fsu-multi-filter-manager.h \
		fsu-filter.h \
		fsu-filter-helper.h \
		fsu-volume-filter.h \
		fsu-level-filter.h \
		fsu-resolution-filter.h \
		fsu-maxframerate-filter.h \
		fsu-framerate-filter.h \
		fsu-gnome-effect-filter.h \
		fsu-preview-filter.h \
		fsu-audioconverter-filter.h \
		fsu-videoconverter-filter.h


lib_LTLIBRARIES = libfsufilters-@GST_MAJORMINOR@.la

BUILT_SOURCES = \
	$(nodist_libfsufilters_@GST_MAJORMINOR@_la_SOURCES) \
	$(nodist_libfsufiltersinclude_HEADERS)

CLEANFILES = $(BUILT_SOURCES) fsu-marshal.list

libfsufilters_@GST_MAJORMINOR@_la_SOURCES = \
		fsu-filter-manager.c \
		fsu-single-filter-manager.c \
		fsu-multi-filter-manager.c \
		fsu-filter.c \
		fsu-filter-helper.c \
		fsu-volume-filter.c \
		fsu-level-filter.c \
		fsu-resolution-filter.c \
		fsu-maxframerate-filter.c \
		fsu-framerate-filter.c \
		fsu-gnome-effect-filter.c \
		fsu-preview-filter.c \
		fsu-audioconverter-filter.c \
		fsu-videoconverter-filter.c

nodist_libfsufilters_@GST_MAJORMINOR@_la_SOURCES = \
		fsu-marshal.c \
		fsu-marshal.h


fsu-marshal.list: $(libfsufilters_@GST_MAJORMINOR@_la_SOURCES) Makefile.am
	$(AM_V_GEN) ( cd $(srcdir) && \
	sed -n -e 's/.*_fsu_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libfsufilters_@GST_MAJORMINOR@_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
		touch $@; \
	else \
		mv $@.tmp $@; \
	fi

libfsufilters_@GST_MAJORMINOR@_la_CFLAGS = \
	$(FS2_INTERNAL_CFLAGS) $(FS2_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS)
libfsufilters_@GST_MAJORMINOR@_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)  \
	$(GST_INTERFACES_LIBS)
libfsufilters_@GST_MAJORMINOR@_la_LDFLAGS = \
	$(FS2_LIB_LDFLAGS) \
	$(FS2_ALL_LDFLAGS) \
	$(FS2_LT_LDFLAGS)


glib_gen_prefix=_fsu
glib_gen_basename=fsu

include $(top_srcdir)/common-modified/gst-glib-gen.mak
