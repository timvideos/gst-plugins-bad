plugin_LTLIBRARIES = libgstspeakertrack.la

noinst_PROGRAMS = ptz

libgstspeakertrack_la_SOURCES = \
	gstfacedetect2.c \
	gstspeakertrack.c \
	gstcamcontrol.c \
	gstcamcontrol_pana.c \
	gstcamcontrol_visca.c

libgstspeakertrack_la_CXXFLAGS = $(GST_CXXFLAGS) $(OPENCV_CFLAGS)
libgstspeakertrack_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(OPENCV_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) -DCV_INLINE="static inline" \
	-DCV_NO_BACKWARD_COMPATIBILITY \
	-I$(top_srcdir)/ext/opencv

libgstspeakertrack_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(OPENCV_LIBS) \
	$(GSTPB_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) -lgstopencv

libgstspeakertrack_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -L$(top_srcdir)/ext/opencv
libgstspeakertrack_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstfacedetect2.h \
	gstspeakertrack.h \
	gstcamcontrol.h

ptz_SOURCES = ptz.c
ptz_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
ptz_CXXFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
ptz_LDFLAGS = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS)
ptz_LDADD = $(GST_LIBS) $(GST_BASE_LIBS)
