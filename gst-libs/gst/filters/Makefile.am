libgstfiltersincludedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/filters

libgstfiltersinclude_HEADERS = \
		fsu-filter-manager.h \
		fsu-single-filter-manager.h \
		fsu-multi-filter-manager.h \
		fsu-filter.h \
		fsu-filter-helper.h \
		fsu-volume-filter.h \
		fsu-level-filter.h \
		fsu-resolution-filter.h \
		fsu-framerate-filter.h \
		fsu-gnome-effect-filter.h \
		fsu-preview-filter.h \
		fsu-audioconverter-filter.h \
		fsu-videoconverter-filter.h

nodist_libgstfiltersinclude_HEADERS = \
		fs-enum-types.h

# Don't forget to rename on ABI break
fs-enum-types.h: fs-enumtypes.h
	$(AM_V_GEN)test -L fs-enum-types.h || { rm -f fs-enum-types.h;  $(LN_S) fs-enumtypes.h fs-enum-types.h ; }

lib_LTLIBRARIES = libgstfilters-@GST_MAJORMINOR@.la

BUILT_SOURCES = \
	$(nodist_libgstfilters_@GST_MAJORMINOR@_la_SOURCES) \
	$(nodist_libgstfiltersinclude_HEADERS)

CLEANFILES = $(BUILT_SOURCES) fs-enumtypes.h fs-marshal.list

libgstfilters_@GST_MAJORMINOR@_la_SOURCES = \
		fsu-filter-manager.c \
		fsu-single-filter-manager.c \
		fsu-multi-filter-manager.c \
		fsu-filter.c \
		fsu-filter-helper.c \
		fsu-volume-filter.c \
		fsu-level-filter.c \
		fsu-resolution-filter.c \
		fsu-framerate-filter.c \
		fsu-gnome-effect-filter.c \
		fsu-preview-filter.c \
		fsu-audioconverter-filter.c \
		fsu-videoconverter-filter.c

nodist_libgstfilters_@GST_MAJORMINOR@_la_SOURCES = \
		fs-marshal.c \
		fs-marshal.h \
		fs-enumtypes.c


fs-marshal.list: $(libgstfilters_@GST_MAJORMINOR@_la_SOURCES) Makefile.am
	$(AM_V_GEN) ( cd $(srcdir) && \
	sed -n -e 's/.*_fs_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libgstfilters_@GST_MAJORMINOR@_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
		touch $@; \
	else \
		mv $@.tmp $@; \
	fi

libgstfilters_@GST_MAJORMINOR@_la_CFLAGS = \
	$(FS2_INTERNAL_CFLAGS) $(FS2_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS)
libgstfilters_@GST_MAJORMINOR@_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)  \
	$(GST_INTERFACES_LIBS)
libgstfilters_@GST_MAJORMINOR@_la_LDFLAGS = \
	$(FS2_LIB_LDFLAGS) \
	$(FS2_ALL_LDFLAGS) \
	$(FS2_LT_LDFLAGS)


public_headers = \
		fsu-filter-manager.h \
		fsu-single-filter-manager.h \
		fsu-multi-filter-manager.h \
		fsu-filter.h \
		fsu-filter-helper.h \
		fsu-volume-filter.h \
		fsu-level-filter.h \
		fsu-resolution-filter.h \
		fsu-framerate-filter.h \
		fsu-gnome-effect-filter.h \
		fsu-preview-filter.h \
		fsu-audioconverter-filter.h \
		fsu-videoconverter-filter.h

glib_enum_headers=$(public_headers)
glib_enum_define=FS
glib_gen_prefix=_fs
glib_gen_basename=fs

include $(top_srcdir)/common-modified/gst-glib-gen.mak
