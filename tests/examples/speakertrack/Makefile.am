noinst_PROGRAMS := speakertrack_test

speakertrack_test_SOURCES := gstspeakertrack_test.c

speakertrack_test_CFLAGS := $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
speakertrack_test_LDFLAGS := $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GSTPB_BASE_LIBS)
speakertrack_test_LDADD = -lgstvideo-$(GST_API_VERSION) \
	$(GST_LIBS) $(X_LIBS) $(LIBM) $(GTK_LIBS)
