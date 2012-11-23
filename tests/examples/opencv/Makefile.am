noinst_PROGRAMS = gstmotioncells_dynamic_test \
  gstfacerec_test

gstmotioncells_dynamic_test_SOURCES = 	\
	gstmotioncells_dynamic_test.c	\
	gst_element_print_properties.c
gstmotioncells_dynamic_test_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
gstmotioncells_dynamic_test_LDFLAGS = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GSTPB_BASE_LIBS)

noinst_HEADERS = 			\
	gstmotioncells_dynamic_test.h	\
	gst_element_print_properties.h

gstfacerec_test_SOURCES = \
	gstfacerec_test.c
gstfacerec_test_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(X_CFLAGS) $(GTK_CFLAGS) -DDEFAULT_VIDEOSINK="\"autovideosink\""
gstfacerec_test_LDFLAGS = $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GSTPB_BASE_LIBS)
gstfacerec_test_LDADD = -lgstvideo-1.0 \
	$(GST_LIBS) $(X_LIBS) $(LIBM) $(GTK_LIBS)
