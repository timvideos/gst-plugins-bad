/*
 * Speakertrack test
 * Copyright (C) 2012 Duzy Chan <code@duzy.info>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

static void
window_closed (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
  GstElement *pipe = user_data;
  gst_element_set_state (pipe, GST_STATE_NULL);
  gtk_main_quit ();
}

static gboolean
bus_observer (GstBus * bus, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:{
      g_print ("End-Of-Stream\n");
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
      break;
    }

    case GST_MESSAGE_ELEMENT:{
      const GstStructure *st =
          (const GstStructure *) gst_message_get_structure (msg);
      const GValue *faces =
          (const GValue *) gst_structure_get_value (st, "faces");
      const GValue *face_value =
          (const GValue *) gst_structure_get_value (st, "face");

      //g_print ("message: %s, %s\n", GST_MESSAGE_SRC_NAME (msg), GST_MESSAGE_TYPE_NAME (msg));
      if (faces) {
        g_print ("%s: faces=%d\n", GST_MESSAGE_SRC_NAME (msg),
            gst_value_list_get_size (faces));
      }

      if (face_value && G_VALUE_HOLDS_BOXED (face_value)) {
        GstStructure *face = (GstStructure *) g_value_get_boxed (face_value);
        if (GST_IS_STRUCTURE (face)) {
          guint x, y, w, h;
          gst_structure_get_uint (face, "x", &x);
          gst_structure_get_uint (face, "y", &y);
          gst_structure_get_uint (face, "width", &w);
          gst_structure_get_uint (face, "height", &h);
          g_print ("%s: face=[(%d,%d), %d,%d]\n", GST_MESSAGE_SRC_NAME (msg),
              x, y, w, h);
        }
      }

      break;
    }

    default:
      //g_print ("message: %d\n", GST_MESSAGE_SRC_NAME (msg), GST_MESSAGE_TYPE_NAME (msg));
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

  dvdemuxer = gst_element_factory_make ("dvdemux", "dv-demuxer");
  if (!dvdemuxer) {
    g_warning ("'dvdemuxer' plugin missing\n");
    return FALSE;
  }

  dvdecoder = gst_element_factory_make ("dvdec", "dv-decoder");
  if (!dvdecoder) {
    g_warning ("'dvdec' plugin missing\n");
    return FALSE;
  }

  videoconvert1 = gst_element_factory_make ("videoconvert", "videoconvert1");
  videoconvert2 = gst_element_factory_make ("videoconvert", "videoconvert2");
  if (!videoconvert1 || !videoconvert2) {
    g_warning ("'videoconvert' plugin missing\n");
    return FALSE;
  }

  speakertracker = gst_element_factory_make ("speakertrack", "speaker-tracker");
  if (!speakertracker) {
    g_warning ("'speakertrack' plugin missing\n");
    return FALSE;
  }

  xvimagesink = gst_element_factory_make ("xvimagesink", "video-sink");
  if (!xvimagesink) {
    g_warning ("'xvimagesink' plugin missing\n");
    return FALSE;
  }

  audioconverter = gst_element_factory_make ("audioconvert", "audio-converter");
  if (!audioconverter) {
    g_warning ("'audioconvert' plugin missing\n");
    return FALSE;
  }

  audiosink = gst_element_factory_make ("alsasink", "audio-sink");
  if (!audiosink) {
    g_warning ("'alsasink' plugin missing\n");
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
  GdkWindow *video_window_xwindow;
  GtkWidget *window, *video_window;
  GstElement *bin, *sink;
  GstBus *bus;
  gulong embed_xid;
  int bus_watch_id;
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
  gtk_init (&argc, &argv);

  bin = gst_pipeline_new ("speaker-tracking-bin");
  if (!bin) {
    g_warning ("no bin\n");
    return __LINE__;
  }

  if (!add_file_source (bin, argv[1], profile)) {
    g_warning ("failed to init file: %s\n", argv[1]);
    return __LINE__;
  }

  sink = gst_bin_get_by_name (GST_BIN (bin), "video-sink");
  if (!sink) {
    g_warning ("video-sink not created\n");
    return __LINE__;
  }

  bus = gst_element_get_bus (bin);
  if (!bus) {
    g_warning ("no bus\n");
    return __LINE__;
  }

  bus_watch_id = gst_bus_add_watch (bus, bus_observer, NULL);
  g_object_unref (bus);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (window_closed), (gpointer) bin);
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), "Speaker Track");

  video_window = gtk_drawing_area_new ();
  gtk_widget_set_double_buffered (video_window, FALSE);
  gtk_container_add (GTK_CONTAINER (window), video_window);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);

  gtk_widget_show_all (window);
  gtk_widget_realize (window);

  video_window_xwindow = gtk_widget_get_window (video_window);
  embed_xid = GDK_WINDOW_XID (video_window_xwindow);
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink), embed_xid);

  gst_element_set_state (bin, GST_STATE_PLAYING);

  gtk_main ();

  g_object_unref (bin);
  g_source_remove (bus_watch_id);
  return 0;
}
