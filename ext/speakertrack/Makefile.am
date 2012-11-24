plugin_LTLIBRARIES = libgstspeakertrack.la

libgstspeakertrack_la_SOURCES = \
	gstspeakertrack.c \
	gstopencvutils.c \
	gstopencvvideofilter.c

libgstspeakertrack_la_CXXFLAGS = $(GST_CXXFLAGS) $(OPENCV_CFLAGS)
libgstspeakertrack_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(OPENCV_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) -DCV_INLINE="static inline" \
	-DCV_NO_BACKWARD_COMPATIBILITY

libgstspeakertrack_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(OPENCV_LIBS) \
			$(GSTPB_BASE_LIBS) -lgstvideo-$(GST_API_VERSION)

libgstspeakertrack_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstspeakertrack_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstspeakertrack.h \
	gstopencvutils.h \
	gstopencvvideofilter.h
