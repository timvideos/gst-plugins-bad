lib_LTLIBRARIES = libgstcamcontrol-@GST_API_VERSION@.la

camcontrol_sources = \
	gstcamcontrol.c \
	gstcamcontrol_pana.c \
	gstcamcontrol_canon.c \
	gstcamcontrol_sony.c

sony_visca_sources = \
	visca_sony/commands.cpp \
	visca_sony/comm.cpp \
	visca_sony/init.cpp \
	visca_sony/viscacamera.cpp \
	visca_sony/viscaconnection.cpp

libgstcamcontrol_@GST_API_VERSION@includedir = \
  $(includedir)/gstreamer-@GST_API_VERSION@/gst/speakertrack
libgstcamcontrol_@GST_API_VERSION@include_HEADERS = gstcamcontrol.h

libgstcamcontrol_@GST_API_VERSION@_la_SOURCES = \
	$(camcontrol_sources) $(sony_visca_sources)


libgstcamcontrol_@GST_API_VERSION@_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) \
    $(GST_PLUGINS_BAD_CFLAGS) \
    $(GST_PLUGINS_BASE_CFLAGS) \
    $(GST_CFLAGS)
libgstcamcontrol_@GST_API_VERSION@_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	$(GSTPB_BASE_LIBS)
libgstcamcontrol_@GST_API_VERSION@_la_LDFLAGS = \
	$(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)

plugin_LTLIBRARIES = libgstspeakertrack.la

noinst_PROGRAMS = ptz ptzcontrol

libgstspeakertrack_la_SOURCES = \
	gstfacedetect2.c \
	gstspeakertrack.c

#libgstspeakertrack_la_CXXFLAGS = $(GST_CXXFLAGS) $(OPENCV_CFLAGS)
libgstspeakertrack_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(OPENCV_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) -DCV_INLINE="static inline" \
	-DCV_NO_BACKWARD_COMPATIBILITY \
	-I$(top_srcdir)/ext/opencv

libgstspeakertrack_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(OPENCV_LIBS) \
	$(GSTPB_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) -lgstopencv \
	-L$(top_srcdir)/ext/speakertrack -lgstcamcontrol-@GST_API_VERSION@

libgstspeakertrack_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -L$(top_srcdir)/ext/opencv
libgstspeakertrack_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstfacedetect2.h \
	gstspeakertrack.h \
	gstcamcontrol.h

ptz_SOURCES = test_ptz.c
ptz_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
#ptz_CXXFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
ptz_LDFLAGS = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) -lm
ptz_LDADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	-L$(top_srcdir)/ext/speakertrack -lgstcamcontrol-@GST_API_VERSION@

ptzcontrol_SOURCES = \
	gstptzcontrol_gtk.c

ptzcontrol_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
#ptzcontrol_CXXFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
ptzcontrol_LDFLAGS = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS)
ptzcontrol_LDADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	-L$(top_srcdir)/ext/speakertrack -lgstcamcontrol-@GST_API_VERSION@

