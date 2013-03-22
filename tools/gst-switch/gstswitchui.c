/* GstSwitchUI
 * Copyright (C) 2012 Duzy Chan <code@duzy.info>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <stdlib.h>

//#define GETTEXT_PACKAGE "switchui"

typedef struct _GstSwitchUI GstSwitchUI;
struct _GstSwitchUI
{
  GstElement *pipeline;
  GstBus *bus;
  GMainLoop *main_loop;

  GstElement *source_element;
  GstElement *sink_element;

  gboolean paused_for_buffering;
  guint timer_id;
};

GstSwitchUI *gst_switchui_new (void);
void gst_switchui_free (GstSwitchUI * switchui);
void gst_switchui_create_pipeline (GstSwitchUI * switchui);
void gst_switchui_create_pipeline_playbin (GstSwitchUI * switchui,
    const char *uri);
void gst_switchui_start (GstSwitchUI * switchui);
void gst_switchui_stop (GstSwitchUI * switchui);

static gboolean gst_switchui_handle_message (GstBus * bus, GstMessage * message,
    gpointer data);
static gboolean onesecond_timer (gpointer priv);


gboolean verbose;

static GOptionEntry entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},

  {NULL}

};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GstSwitchUI *switchui;
  GMainLoop *main_loop;

  //if (!g_thread_supported ()) g_thread_init(NULL);

  context = g_option_context_new ("- FIXME");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }
  g_option_context_free (context);

  switchui = gst_switchui_new ();

  if (argc > 1) {
    gchar *uri;
    if (gst_uri_is_valid (argv[1])) {
      uri = g_strdup (argv[1]);
    } else {
      uri = g_filename_to_uri (argv[1], NULL, NULL);
    }
    gst_switchui_create_pipeline_playbin (switchui, uri);
    g_free (uri);
  } else {
    gst_switchui_create_pipeline (switchui);
  }

  gst_switchui_start (switchui);

  main_loop = g_main_loop_new (NULL, TRUE);
  switchui->main_loop = main_loop;

  g_main_loop_run (main_loop);

  exit (0);
}


GstSwitchUI *
gst_switchui_new (void)
{
  GstSwitchUI *switchui;

  switchui = g_new0 (GstSwitchUI, 1);

  return switchui;
}

void
gst_switchui_free (GstSwitchUI * switchui)
{
  if (switchui->source_element) {
    gst_object_unref (switchui->source_element);
    switchui->source_element = NULL;
  }
  if (switchui->sink_element) {
    gst_object_unref (switchui->sink_element);
    switchui->sink_element = NULL;
  }

  if (switchui->pipeline) {
    gst_element_set_state (switchui->pipeline, GST_STATE_NULL);
    gst_object_unref (switchui->pipeline);
    switchui->pipeline = NULL;
  }
  g_free (switchui);
}

void
gst_switchui_create_pipeline_playbin (GstSwitchUI * switchui, const char *uri)
{
  GstElement *pipeline;
  GError *error = NULL;

  pipeline = gst_pipeline_new (NULL);
  gst_bin_add (GST_BIN (pipeline),
      gst_element_factory_make ("playbin", "source"));

  if (error) {
    g_print ("pipeline parsing error: %s\n", error->message);
    gst_object_unref (pipeline);
    return;
  }

  switchui->pipeline = pipeline;

  gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
  switchui->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (switchui->bus, gst_switchui_handle_message, switchui);

  switchui->source_element = gst_bin_get_by_name (GST_BIN (pipeline), "source");
  g_print ("source_element is %p\n", switchui->source_element);

  g_print ("setting uri to %s\n", uri);
  g_object_set (switchui->source_element, "uri", uri, NULL);
}

void
gst_switchui_create_pipeline (GstSwitchUI * switchui)
{
  GString *pipe_desc;
  GstElement *pipeline;
  GError *error = NULL;

  pipe_desc = g_string_new ("");

  g_string_append (pipe_desc, "videotestsrc name=source num-buffers=100 ! ");
  g_string_append (pipe_desc, "timeoverlay ! ");
  g_string_append (pipe_desc, "xvimagesink name=sink ");
  g_string_append (pipe_desc,
      "audiotestsrc samplesperbuffer=1600 num-buffers=100 ! ");
  g_string_append (pipe_desc, "alsasink ");

  if (verbose)
    g_print ("pipeline: %s\n", pipe_desc->str);

  pipeline = (GstElement *) gst_parse_launch (pipe_desc->str, &error);
  g_string_free (pipe_desc, FALSE);

  if (error) {
    g_print ("pipeline parsing error: %s\n", error->message);
    gst_object_unref (pipeline);
    return;
  }

  switchui->pipeline = pipeline;

  gst_pipeline_set_auto_flush_bus (GST_PIPELINE (pipeline), FALSE);
  switchui->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (switchui->bus, gst_switchui_handle_message, switchui);

  switchui->source_element = gst_bin_get_by_name (GST_BIN (pipeline), "source");
  switchui->sink_element = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
}

void
gst_switchui_start (GstSwitchUI * switchui)
{
  gst_element_set_state (switchui->pipeline, GST_STATE_READY);

  switchui->timer_id = g_timeout_add (1000, onesecond_timer, switchui);
}

void
gst_switchui_stop (GstSwitchUI * switchui)
{
  gst_element_set_state (switchui->pipeline, GST_STATE_NULL);

  g_source_remove (switchui->timer_id);
}

static void
gst_switchui_handle_eos (GstSwitchUI * switchui)
{
  gst_switchui_stop (switchui);
}

static void
gst_switchui_handle_error (GstSwitchUI * switchui, GError * error,
    const char *debug)
{
  g_print ("error: %s\n", error->message);
  gst_switchui_stop (switchui);
}

static void
gst_switchui_handle_warning (GstSwitchUI * switchui, GError * error,
    const char *debug)
{
  g_print ("warning: %s\n", error->message);
}

static void
gst_switchui_handle_info (GstSwitchUI * switchui, GError * error,
    const char *debug)
{
  g_print ("info: %s\n", error->message);
}

static void
gst_switchui_handle_null_to_ready (GstSwitchUI * switchui)
{
  gst_element_set_state (switchui->pipeline, GST_STATE_PAUSED);

}

static void
gst_switchui_handle_ready_to_paused (GstSwitchUI * switchui)
{
  if (!switchui->paused_for_buffering) {
    gst_element_set_state (switchui->pipeline, GST_STATE_PLAYING);
  }
}

static void
gst_switchui_handle_paused_to_playing (GstSwitchUI * switchui)
{

}

static void
gst_switchui_handle_playing_to_paused (GstSwitchUI * switchui)
{

}

static void
gst_switchui_handle_paused_to_ready (GstSwitchUI * switchui)
{

}

static void
gst_switchui_handle_ready_to_null (GstSwitchUI * switchui)
{
  g_main_loop_quit (switchui->main_loop);

}


static gboolean
gst_switchui_handle_message (GstBus * bus, GstMessage * message, gpointer data)
{
  GstSwitchUI *switchui = (GstSwitchUI *) data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      gst_switchui_handle_eos (switchui);
      break;
    case GST_MESSAGE_ERROR:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_error (message, &error, &debug);
      gst_switchui_handle_error (switchui, error, debug);
    }
      break;
    case GST_MESSAGE_WARNING:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_warning (message, &error, &debug);
      gst_switchui_handle_warning (switchui, error, debug);
    }
      break;
    case GST_MESSAGE_INFO:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_info (message, &error, &debug);
      gst_switchui_handle_info (switchui, error, debug);
    }
      break;
    case GST_MESSAGE_TAG:
    {
      GstTagList *tag_list;

      gst_message_parse_tag (message, &tag_list);
      if (verbose)
        g_print ("tag\n");
    }
      break;
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState oldstate, newstate, pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);
      if (GST_ELEMENT (message->src) == switchui->pipeline) {
        if (verbose)
          g_print ("state change from %s to %s\n",
              gst_element_state_get_name (oldstate),
              gst_element_state_get_name (newstate));
        switch (GST_STATE_TRANSITION (oldstate, newstate)) {
          case GST_STATE_CHANGE_NULL_TO_READY:
            gst_switchui_handle_null_to_ready (switchui);
            break;
          case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_switchui_handle_ready_to_paused (switchui);
            break;
          case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            gst_switchui_handle_paused_to_playing (switchui);
            break;
          case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            gst_switchui_handle_playing_to_paused (switchui);
            break;
          case GST_STATE_CHANGE_PAUSED_TO_READY:
            gst_switchui_handle_paused_to_ready (switchui);
            break;
          case GST_STATE_CHANGE_READY_TO_NULL:
            gst_switchui_handle_ready_to_null (switchui);
            break;
          default:
            if (verbose)
              g_print ("unknown state change from %s to %s\n",
                  gst_element_state_get_name (oldstate),
                  gst_element_state_get_name (newstate));
        }
      }
    }
      break;
    case GST_MESSAGE_BUFFERING:
    {
      int percent;
      gst_message_parse_buffering (message, &percent);
      //g_print("buffering %d\n", percent);
      if (!switchui->paused_for_buffering && percent < 100) {
        g_print ("pausing for buffing\n");
        switchui->paused_for_buffering = TRUE;
        gst_element_set_state (switchui->pipeline, GST_STATE_PAUSED);
      } else if (switchui->paused_for_buffering && percent == 100) {
        g_print ("unpausing for buffing\n");
        switchui->paused_for_buffering = FALSE;
        gst_element_set_state (switchui->pipeline, GST_STATE_PLAYING);
      }
    }
      break;
    case GST_MESSAGE_STATE_DIRTY:
    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STRUCTURE_CHANGE:
    case GST_MESSAGE_STREAM_STATUS:
      break;
    case GST_MESSAGE_STEP_DONE:
    case GST_MESSAGE_APPLICATION:
    case GST_MESSAGE_ELEMENT:
    case GST_MESSAGE_SEGMENT_START:
    case GST_MESSAGE_SEGMENT_DONE:
      //case GST_MESSAGE_DURATION:
    case GST_MESSAGE_LATENCY:
    case GST_MESSAGE_ASYNC_START:
    case GST_MESSAGE_ASYNC_DONE:
    case GST_MESSAGE_REQUEST_STATE:
    case GST_MESSAGE_STEP_START:
    case GST_MESSAGE_QOS:
    default:
      if (verbose) {
        g_print ("message: %s\n", GST_MESSAGE_TYPE_NAME (message));
      }
      break;
  }

  return TRUE;
}



static gboolean
onesecond_timer (gpointer priv)
{
  //GstSwitchUI *switchui = (GstSwitchUI *)priv;

  g_print (".\n");

  return TRUE;
}



/* helper functions */

#if 0
gboolean
have_element (const gchar * element_name)
{
  GstPluginFeature *feature;

  feature = gst_default_registry_find_feature (element_name,
      GST_TYPE_ELEMENT_FACTORY);
  if (feature) {
    g_object_unref (feature);
    return TRUE;
  }
  return FALSE;
}
#endif
