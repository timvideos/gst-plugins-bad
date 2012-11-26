#include <gst/gst.h>

static gboolean
bus_observer (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:{
      g_print ("End-Of-Stream\n");
      g_main_loop_quit (loop);
      break;
    }

    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *err;

      gst_message_parse_error (msg, &err, &debug);
      g_printerr ("Debugging info: %s\n", (debug) ? debug : "none");
      g_free (debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      g_main_loop_quit (loop);

      break;
    }

    default:
      //g_print ("message: %d\n", GST_MESSAGE_TYPE (msg));
      break;
  }

  return TRUE;
}

static void
on_pad_added (GstElement * element, GstPad * pad, gpointer data)
{
  GstElement **sinks = (GstElement **) data;
  GstPad *sinkpad = NULL;
  gchar *padname = gst_pad_get_name (pad);

  g_print ("demuxer pad: %s\n", padname);

  if (g_strcmp0 (padname, "audio") == 0) {
    sinkpad = gst_element_get_static_pad (sinks[0], "sink");
  } else if (g_strcmp0 (padname, "video") == 0) {
    sinkpad = gst_element_get_static_pad (sinks[1], "sink");
  }

  if (!sinkpad) {
    g_warning ("no sink pad\n");
  } else {
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);
  }

  g_free (padname);
}

static gboolean
add_file_source (GstElement * pipe, const char *filename, const char *profile)
{
  GstElement *source, *dvdemuxer, *audioconverter, *dvdecoder;
  GstElement *videoconvert1, *videoconvert2, *queue1, *queue2;
  GstElement *speakertracker, *xvimagesink, *audiosink;
  GstElement *demuxSinks[2];

  /* Create elements */

  source = gst_element_factory_make ("filesrc", "source");
  if (!source) {
    g_warning ("'source' plugin missing\n");
    return FALSE;
  }

  dvdemuxer = gst_element_factory_make ("dvdemux", "dv_demuxer");
  if (!dvdemuxer) {
    g_warning ("'dvdemuxer' plugin missing\n");
    return FALSE;
  }

  dvdecoder = gst_element_factory_make ("dvdec", "dv_decoder");
  if (!dvdecoder) {
    g_warning ("'dvdec' plugin missing\n");
    return FALSE;
  }

  videoconvert1 = gst_element_factory_make ("videoconvert", "videoconvert1");
  if (!videoconvert1) {
    g_warning ("'videoconvert' plugin missing\n");
    return FALSE;
  }

  videoconvert2 = gst_element_factory_make ("videoconvert", "videoconvert2");
  if (!videoconvert2) {
    g_warning ("'videoconvert' plugin missing\n");
    return FALSE;
  }

  speakertracker = gst_element_factory_make ("speakertrack", "speaker_tracker");
  if (!speakertracker) {
    g_warning ("'speakertrack' plugin missing\n");
    return FALSE;
  }

  xvimagesink = gst_element_factory_make ("xvimagesink", "video_sink");
  if (!xvimagesink) {
    g_warning ("'xvimagesink' plugin missing\n");
    return FALSE;
  }

  audioconverter = gst_element_factory_make ("audioconvert", "audio_converter");
  if (!audioconverter) {
    g_warning ("'audioconvert' plugin missing\n");
    return FALSE;
  }

  audiosink = gst_element_factory_make ("alsasink", "audio_sink");
  if (!audiosink) {
    g_warning ("'audioconvert' plugin missing\n");
    return FALSE;
  }

  queue1 = gst_element_factory_make ("queue", "queue1");
  queue2 = gst_element_factory_make ("queue", "queue2");
  if (!queue1 || !queue2) {
    g_warning ("'queue' plugin missing\n");
    return FALSE;
  }

  if (profile == NULL)
    profile = "";

  g_object_set (G_OBJECT (source), "location", filename, NULL);
  g_object_set (G_OBJECT (speakertracker), "profile", profile, NULL);
  g_object_set (G_OBJECT (speakertracker), "min-size-width", 60, NULL);
  g_object_set (G_OBJECT (speakertracker), "min-size-height", 60, NULL);

  gst_bin_add_many (GST_BIN (pipe), source, dvdemuxer, dvdecoder,
      videoconvert1, speakertracker, videoconvert2, xvimagesink,
      audioconverter, audiosink, queue1, queue2, NULL);

  /* link elements */
  if (!gst_element_link (source, dvdemuxer)) {
    g_warning ("failed to link element (%d)\n", __LINE__);
  }

  if (!gst_element_link_many (queue1, audioconverter, audiosink, NULL)) {
    g_warning ("failed to link element (%d)\n", __LINE__);
  }

  if (!gst_element_link_many (queue2, dvdecoder, videoconvert1, speakertracker,
          videoconvert2, xvimagesink, NULL)) {
    g_warning ("failed to link element (%d)\n", __LINE__);
  }

  demuxSinks[0] = queue1;
  demuxSinks[1] = queue2;
  g_signal_connect (dvdemuxer, "pad-added", G_CALLBACK (on_pad_added),
      &demuxSinks[0]);

  return TRUE;
}

int
main (int argc, char **argv)
{
  GstElement *bin;
  GstBus *bus;
  int bus_watch_id;
  GMainLoop *loop;
  const char *profile;

  if (argc == 2) {
    profile = NULL;
  } else if (argc == 3) {
    profile = argv[2];
  } else {
    g_print ("Usage: %s <DV file> [<profile>]\n", argv[0]);
    return __LINE__;
  }

  gst_init (&argc, &argv);

  bin = gst_pipeline_new ("speaker-tracking-bin");
  if (!bin) {
    g_warning ("no bin\n");
    return __LINE__;
  }

  if (!add_file_source (bin, argv[1], profile)) {
    g_warning ("failed to init file: %s\n", argv[1]);
    return __LINE__;
  }

  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (bin);
  if (!bus) {
    g_warning ("no bus\n");
    return __LINE__;
  }

  bus_watch_id = gst_bus_add_watch (bus, bus_observer, loop);
  g_object_unref (bus);

  gst_element_set_state (bin, GST_STATE_PLAYING);

  g_main_loop_run (loop);

  gst_element_set_state (bin, GST_STATE_NULL);

  g_object_unref (bin);
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}
