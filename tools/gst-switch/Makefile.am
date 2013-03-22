bin_PROGRAMS := gstswitchsrv gstswitchui

gstswitchsrv_SOURCES := gstswitchsrv.c
gstswitchsrv_CFLAGS := $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
gstswitchsrv_LDFLAGS := $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GSTPB_BASE_LIBS)
gstswitchsrv_LDADD = -lgstvideo-$(GST_API_VERSION) $(GST_LIBS) $(LIBM)

gstswitchui_SOURCES := gstswitchui.c
gstswitchui_CFLAGS := $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(X_CFLAGS) $(GTK_CFLAGS)
gstswitchui_LDFLAGS := $(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GSTPB_BASE_LIBS)
gstswitchui_LDADD = -lgstvideo-$(GST_API_VERSION) $(GST_LIBS) $(X_LIBS) $(LIBM) $(GTK_LIBS)
