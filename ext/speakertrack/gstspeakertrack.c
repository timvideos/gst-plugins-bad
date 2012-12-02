/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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

/**
 * SECTION:element-speakertrack
 *
 * Performs face detection on videos and images.
 *
 * The image is scaled down multiple times using the GstSpeakerTrack::scale-factor
 * until the size is &lt;= GstSpeakerTrack::min-size-width or 
 * GstSpeakerTrack::min-size-height. 
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 autovideosrc ! decodebin2 ! colorspace ! speakertrack ! videoconvert ! xvimagesink
 * ]| Detect and show faces
 * |[
 * gst-launch-0.10 autovideosrc ! video/x-raw,width=320,height=240 ! videoconvert ! speakertrack min-size-width=60 min-size-height=60 ! colorspace ! xvimagesink
 * ]| Detect large faces on a smaller image 
 *
 * </refsect2>
 */

/* FIXME: development version of OpenCV has CV_HAAR_FIND_BIGGEST_OBJECT which
 * we might want to use if available
 * see https://code.ros.org/svn/opencv/trunk/opencv/modules/objdetect/src/haar.cpp
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <glib/gprintf.h>

#include "gstopencvutils.h"
#include "gstspeakertrack.h"

GST_DEBUG_CATEGORY_STATIC (gst_speaker_track_debug);
#define GST_CAT_DEFAULT gst_speaker_track_debug

#define HAAR_CASCADES_DIR OPENCV_PREFIX "/share/opencv/haarcascades/"
#define DEFAULT_FACE_PROFILE HAAR_CASCADES_DIR "haarcascade_frontalface_default.xml"
#define DEFAULT_NOSE_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_nose.xml"
#define DEFAULT_MOUTH_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_mouth.xml"
#define DEFAULT_EYES_PROFILE HAAR_CASCADES_DIR "haarcascade_mcs_eyepair_small.xml"
#define DEFAULT_SCALE_FACTOR 1.1
#define DEFAULT_FLAGS 0
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 0
#define DEFAULT_MIN_SIZE_HEIGHT 0
#define FACE_DETECT_TIME_GAP 6000

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_DISPLAY_NOSE,
  PROP_DISPLAY_MOUTH,
  PROP_DISPLAY_EYES,
  PROP_DETECT_NOSE,
  PROP_DETECT_MOUTH,
  PROP_DETECT_EYES,
  PROP_DETECT_PER_FRAME,
  PROP_FACE_PROFILE,
  PROP_NOSE_PROFILE,
  PROP_MOUTH_PROFILE,
  PROP_EYES_PROFILE,
  PROP_SCALE_FACTOR,
  PROP_MIN_NEIGHBORS,
  PROP_FLAGS,
  PROP_MIN_SIZE_WIDTH,
  PROP_MIN_SIZE_HEIGHT
};


/*
 * GstSpeakerTrackFlags:
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_SPEAKER_TRACK_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstSpeakerTrackFlags;

#define GST_TYPE_SPEAKER_TRACK_FLAGS (gst_speaker_track_flags_get_type())

static void
register_gst_speaker_track_flags (GType * id)
{
  static const GFlagsValue values[] = {
    {(guint) GST_SPEAKER_TRACK_HAAR_DO_CANNY_PRUNING,
        "Do Canny edge detection to discard some regions", "do-canny-pruning"},
    {0, NULL, NULL}
  };
  *id = g_flags_register_static ("GstSpeakerTrackFlags", values);
}

static GType
gst_speaker_track_flags_get_type (void)
{
  static GType id;
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) register_gst_speaker_track_flags, &id);
  return id;
}

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"))
    );

G_DEFINE_TYPE (GstSpeakerTrack, gst_speaker_track,
    GST_TYPE_OPENCV_VIDEO_FILTER);

static void gst_speaker_track_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_speaker_track_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_speaker_track_set_caps (GstOpencvVideoFilter * transform,
    gint in_width, gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels);
static GstFlowReturn gst_speaker_track_transform_ip (GstOpencvVideoFilter *
    base, GstBuffer * buf, IplImage * img);

static CvHaarClassifierCascade *gst_speaker_track_load_profile (GstSpeakerTrack
    * filter, gchar * profile);

static void gst_speaker_track_select_face (GstSpeakerTrack * filter,
    gint x, gint y);

/* Clean up */
static void
gst_speaker_track_finalize (GObject * obj)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (obj);

  if (filter->cvGray)
    cvReleaseImage (&filter->cvGray);
  if (filter->cvStorage)
    cvReleaseMemStorage (&filter->cvStorage);

  g_free (filter->face_profile);
  g_free (filter->nose_profile);
  g_free (filter->mouth_profile);
  g_free (filter->eyes_profile);

  if (filter->cvSpeakerTrack)
    cvReleaseHaarClassifierCascade (&filter->cvSpeakerTrack);
  if (filter->cvNoseDetect)
    cvReleaseHaarClassifierCascade (&filter->cvNoseDetect);
  if (filter->cvMouthDetect)
    cvReleaseHaarClassifierCascade (&filter->cvMouthDetect);
  if (filter->cvEyesDetect)
    cvReleaseHaarClassifierCascade (&filter->cvEyesDetect);

  G_OBJECT_CLASS (gst_speaker_track_parent_class)->finalize (obj);
}

static gboolean
gst_speaker_track_send_event (GstElement * element, GstEvent * event)
{
  GstElementClass *parent_class =
      GST_ELEMENT_CLASS (gst_speaker_track_parent_class);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      const GstStructure *s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "select")) {
        gint x, y;
        if (gst_structure_get_int (s, "x", &x)
            && gst_structure_get_int (s, "y", &y)) {
          gst_speaker_track_select_face (GST_SPEAKER_TRACK (element), x, y);
        }
        return TRUE;
      }
    }
      break;

    default:
      break;
  }
  return parent_class->send_event (element, event);
}

/* initialize the speakertrack's class */
static void
gst_speaker_track_class_init (GstSpeakerTrackClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class =
      (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_speaker_track_finalize);
  gobject_class->set_property = gst_speaker_track_set_property;
  gobject_class->get_property = gst_speaker_track_get_property;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_speaker_track_transform_ip;
  gstopencvbasefilter_class->cv_set_caps = gst_speaker_track_set_caps;

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_boolean ("display", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_NOSE,
      g_param_spec_boolean ("display-nose", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_MOUTH,
      g_param_spec_boolean ("display-mouth", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_EYES,
      g_param_spec_boolean ("display-eyes", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DETECT_NOSE,
      g_param_spec_boolean ("detect-nose", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DETECT_MOUTH,
      g_param_spec_boolean ("detect-mouth", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DETECT_EYES,
      g_param_spec_boolean ("detect-eyes", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DETECT_PER_FRAME,
      g_param_spec_boolean ("detect-per-frame", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FACE_PROFILE,
      g_param_spec_string ("profile", "Face profile",
          "Location of Haar cascade file to use for face detection",
          DEFAULT_FACE_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_NOSE_PROFILE,
      g_param_spec_string ("nose-profile", "Nose profile",
          "Location of Haar cascade file to use for nose detection",
          DEFAULT_NOSE_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MOUTH_PROFILE,
      g_param_spec_string ("mouth-profile", "Mouth profile",
          "Location of Haar cascade file to use for mouth detection",
          DEFAULT_MOUTH_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_EYES_PROFILE,
      g_param_spec_string ("eyes-profile", "Eyes profile",
          "Location of Haar cascade file to use for eye-pair detection",
          DEFAULT_EYES_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to cvHaarDetectObjects",
          GST_TYPE_SPEAKER_TRACK_FLAGS, DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SCALE_FACTOR,
      g_param_spec_double ("scale-factor", "Scale factor",
          "Factor by which the frame is scaled after each object scan",
          1.1, 10.0, DEFAULT_SCALE_FACTOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_NEIGHBORS,
      g_param_spec_int ("min-neighbors", "Mininum neighbors",
          "Minimum number (minus 1) of neighbor rectangles that makes up "
          "an object", 0, G_MAXINT, DEFAULT_MIN_NEIGHBORS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_WIDTH,
      g_param_spec_int ("min-size-width", "Minimum face width",
          "Minimum area width to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE_HEIGHT,
      g_param_spec_int ("min-size-height", "Minimum face height",
          "Minimum area height to be recognized as a face", 0, G_MAXINT,
          DEFAULT_MIN_SIZE_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  element_class->send_event = GST_DEBUG_FUNCPTR (gst_speaker_track_send_event);

  gst_element_class_set_static_metadata (element_class,
      "speakertrack",
      "Filter/Effect/Video",
      "Tracks speaker on videos and images, providing speaker positions via bus messages, "
      "initiated from Michael Sheldon's face detection module",
      "Duzy Chan <code@duzy.info>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/*
static gboolean
gst_speaker_track_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
  case GST_EVENT_CUSTOM_UPSTREAM:
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    g_print ("event: %s, %s\n", gst_pad_get_name (pad), GST_EVENT_TYPE_NAME (event));
    break;

  default:
    break;
  }
  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_speaker_track_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
  case GST_EVENT_CUSTOM_UPSTREAM:
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    g_print ("event: %s, %s\n", gst_pad_get_name (pad), GST_EVENT_TYPE_NAME (event));
    break;

  default:
    break;
  }
  return gst_pad_event_default (pad, parent, event);
}
*/

/* initialize the new element
 * initialize instance structure
 */
static void
gst_speaker_track_init (GstSpeakerTrack * filter)
{
#if 1
  /*
     GstPad *sinkpad = gst_element_get_static_pad (GST_ELEMENT (filter), "sink");
     GstPad *srcpad = gst_element_get_static_pad (GST_ELEMENT (filter), "src");
     gst_pad_set_event_function (sinkpad, gst_speaker_track_sink_event);
     gst_pad_set_event_function (srcpad, gst_speaker_track_src_event);
   */
#else
  GstPad *srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GstPad *sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (srcpad, gst_speaker_track_src_event);
  gst_pad_set_event_function (sinkpad, gst_speaker_track_sink_event);
  gst_element_add_pad (GST_ELEMENT (filter), sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), srcpad);
#endif

  filter->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
  filter->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
  filter->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
  filter->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
  filter->display = TRUE;
  filter->scale_factor = DEFAULT_SCALE_FACTOR;
  filter->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  filter->flags = DEFAULT_FLAGS;
  filter->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  filter->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;
  filter->cvSpeakerTrack =
      gst_speaker_track_load_profile (filter, filter->face_profile);
  filter->cvNoseDetect =
      gst_speaker_track_load_profile (filter, filter->nose_profile);
  filter->cvMouthDetect =
      gst_speaker_track_load_profile (filter, filter->mouth_profile);
  filter->cvEyesDetect =
      gst_speaker_track_load_profile (filter, filter->eyes_profile);

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_speaker_track_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_free (filter->face_profile);
      if (filter->cvSpeakerTrack)
        cvReleaseHaarClassifierCascade (&filter->cvSpeakerTrack);
      if (strlen (g_value_get_string (value))) {
        filter->face_profile = g_value_dup_string (value);
      } else {
        filter->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
      }
      filter->cvSpeakerTrack =
          gst_speaker_track_load_profile (filter, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_free (filter->nose_profile);
      if (filter->cvNoseDetect)
        cvReleaseHaarClassifierCascade (&filter->cvNoseDetect);
      if (strlen (g_value_get_string (value))) {
        filter->nose_profile = g_value_dup_string (value);
      } else {
        filter->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
      }
      filter->cvNoseDetect =
          gst_speaker_track_load_profile (filter, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_free (filter->mouth_profile);
      if (filter->cvMouthDetect)
        cvReleaseHaarClassifierCascade (&filter->cvMouthDetect);
      if (strlen (g_value_get_string (value))) {
        filter->mouth_profile = g_value_dup_string (value);
      } else {
        filter->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
      }
      filter->cvMouthDetect =
          gst_speaker_track_load_profile (filter, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_free (filter->eyes_profile);
      if (filter->cvEyesDetect)
        cvReleaseHaarClassifierCascade (&filter->cvEyesDetect);
      if (strlen (g_value_get_string (value))) {
        filter->eyes_profile = g_value_dup_string (value);
      } else {
        filter->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
      }
      filter->cvEyesDetect =
          gst_speaker_track_load_profile (filter, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      filter->display = g_value_get_boolean (value);
      break;
    case PROP_DISPLAY_NOSE:
      filter->display_nose = g_value_get_boolean (value);
      break;
    case PROP_DISPLAY_MOUTH:
      filter->display_mouth = g_value_get_boolean (value);
      break;
    case PROP_DISPLAY_EYES:
      filter->display_eyes = g_value_get_boolean (value);
      break;
    case PROP_DETECT_NOSE:
      filter->detect_nose = g_value_get_boolean (value);
      break;
    case PROP_DETECT_MOUTH:
      filter->detect_mouth = g_value_get_boolean (value);
      break;
    case PROP_DETECT_EYES:
      filter->detect_eyes = g_value_get_boolean (value);
      break;
    case PROP_DETECT_PER_FRAME:
      filter->detect_per_frame = g_value_get_boolean (value);
      break;
    case PROP_SCALE_FACTOR:
      filter->scale_factor = g_value_get_double (value);
      break;
    case PROP_MIN_NEIGHBORS:
      filter->min_neighbors = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_WIDTH:
      filter->min_size_width = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      filter->min_size_height = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      filter->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_speaker_track_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_value_set_string (value, filter->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_value_set_string (value, filter->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_value_set_string (value, filter->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_value_set_string (value, filter->eyes_profile);
      break;
    case PROP_DISPLAY:
      g_value_set_boolean (value, filter->display);
      break;
    case PROP_DISPLAY_NOSE:
      g_value_set_boolean (value, filter->display_nose);
      break;
    case PROP_DISPLAY_MOUTH:
      g_value_set_boolean (value, filter->display_mouth);
      break;
    case PROP_DISPLAY_EYES:
      g_value_set_boolean (value, filter->display_eyes);
      break;
    case PROP_DETECT_NOSE:
      g_value_set_boolean (value, filter->detect_nose);
      break;
    case PROP_DETECT_MOUTH:
      g_value_set_boolean (value, filter->detect_mouth);
      break;
    case PROP_DETECT_EYES:
      g_value_set_boolean (value, filter->detect_eyes);
      break;
    case PROP_DETECT_PER_FRAME:
      g_value_set_boolean (value, filter->detect_per_frame);
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_double (value, filter->scale_factor);
      break;
    case PROP_MIN_NEIGHBORS:
      g_value_set_int (value, filter->min_neighbors);
      break;
    case PROP_MIN_SIZE_WIDTH:
      g_value_set_int (value, filter->min_size_width);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      g_value_set_int (value, filter->min_size_height);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, filter->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_speaker_track_set_caps (GstOpencvVideoFilter * transform, gint in_width,
    gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels)
{
  GstSpeakerTrack *filter;

  filter = GST_SPEAKER_TRACK (transform);

  if (filter->cvGray)
    cvReleaseImage (&filter->cvGray);

  filter->cvGray = cvCreateImage (cvSize (in_width, in_height), IPL_DEPTH_8U,
      1);

  if (!filter->cvStorage)
    filter->cvStorage = cvCreateMemStorage (0);
  else
    cvClearMemStorage (filter->cvStorage);

  return TRUE;
}

static GstMessage *
gst_speaker_track_message_new (GstSpeakerTrack * filter, GstBuffer * buf)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (filter);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));

  s = gst_structure_new ("speakertrack",
      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (buf),
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time,
      "duration", G_TYPE_UINT64, GST_BUFFER_DURATION (buf), NULL);

  return gst_message_new_element (GST_OBJECT (filter), s);
}

static CvSeq *
gst_speaker_track_run_detector (GstSpeakerTrack * filter,
    CvHaarClassifierCascade * detector, gint min_size_width,
    gint min_size_height)
{
  return cvHaarDetectObjects (filter->cvGray, detector,
      filter->cvStorage, filter->scale_factor, filter->min_neighbors,
      filter->flags, cvSize (min_size_width, min_size_height)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
      , cvSize (min_size_width + 2, min_size_height + 2)
#endif
      );
}

static void
gst_speaker_track_draw_rect_spots (IplImage * img, CvRect * r, CvScalar color,
    int thikness CV_DEFAULT (2))
{
  int radius = 1, linetype = 8;
  CvPoint center;
  center.x = cvRound ((r->x + r->width / 2));
  center.y = r->y;
  cvCircle (img, center, radius, color, thikness, linetype, 0);

  center.x = r->x;
  center.y = cvRound ((r->y + r->height / 2));;
  cvCircle (img, center, radius, color, thikness, linetype, 0);

  center.x = cvRound ((r->x + r->width / 2));
  center.y = cvRound ((r->y + r->height));;
  cvCircle (img, center, radius, color, thikness, linetype, 0);

  center.x = cvRound ((r->x + r->width));
  center.y = cvRound ((r->y + r->height / 2));;
  cvCircle (img, center, radius, color, thikness, linetype, 0);
}

static gboolean
gst_speaker_track_lock_detect (GstSpeakerTrack * st, IplImage * img,
    gboolean do_display)
{
  // TODO: detect gesture
  st->locked = TRUE;
  return TRUE;
}

static gboolean
gst_speaker_track_lock (GstSpeakerTrack * st, IplImage * img, GstBuffer * buf,
    GstStructure * face, gint i)
{
  GstMessage *msg = gst_speaker_track_message_new (st, buf);
  GValue face_value = { 0 };

  if (st->active_face) {
    guint dx, dy;
    CvRect r1, r2;
    gst_structure_get_uint (st->active_face, "x", (guint *) & r1.x);
    gst_structure_get_uint (st->active_face, "y", (guint *) & r1.y);
    gst_structure_get_uint (st->active_face, "width", (guint *) & r1.width);
    gst_structure_get_uint (st->active_face, "height", (guint *) & r1.height);
    gst_structure_get_uint (face, "x", (guint *) & r2.x);
    gst_structure_get_uint (face, "y", (guint *) & r2.y);
    gst_structure_get_uint (face, "width", (guint *) & r2.width);
    gst_structure_get_uint (face, "height", (guint *) & r2.height);

    dx = r1.width / 2;
    dy = r1.height / 2;
    if (dx <= abs (r2.x - r1.x)
        || dy <= abs (r2.y - r1.y)
        || dx <= abs (r2.width - r1.width)
        || dy <= abs (r2.height - r1.height)) {
      //if (st->lock_stamp <= 6000)
      return FALSE;
    }

    gst_structure_free (st->active_face);
  }

  st->active_face = gst_structure_copy (face);
  st->lock_stamp = gst_util_get_timestamp ();

  g_value_init (&face_value, GST_TYPE_STRUCTURE);
  g_value_take_boxed (&face_value, face);
  gst_structure_set_value ((GstStructure *) gst_message_get_structure (msg),
      "face", &face_value);
  g_value_unset (&face_value);

  gst_element_post_message (GST_ELEMENT (st), msg);
  return TRUE;
}

static void
gst_speaker_track_select_face (GstSpeakerTrack * filter, gint x, gint y)
{
  g_print ("event: select(%d, %d)\n", x, y);
}

static gboolean
gst_speaker_track_get_face_rect (GstStructure * face, CvRect * rect,
    const gchar * prefix)
{
  gchar name[16];

  g_sprintf (name, "%sx", prefix);
  if (gst_structure_has_field (face, name)) {
    gst_structure_get_uint (face, name, (guint *) & rect->x);

    g_sprintf (name, "%sy", prefix);
    gst_structure_get_uint (face, name, (guint *) & rect->y);

    g_sprintf (name, "%swidth", prefix);
    gst_structure_get_uint (face, name, (guint *) & rect->width);

    g_sprintf (name, "%sheight", prefix);
    gst_structure_get_uint (face, name, (guint *) & rect->height);
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_speaker_track_mark_face (GstSpeakerTrack * filter, IplImage * img,
    GstStructure * face, gint i)
{
  CvPoint center;
  CvRect r, rr;
  gint cb = 255 - ((i & 3) << 7);
  gint cg = 255 - ((i & 12) << 5);
  gint cr = 255 - ((i & 48) << 3);
  gint thikness = 1;
  gboolean have_nose, have_mouth, have_eyes;
  gboolean is_active = FALSE;

  if (!face) {
    return is_active;
  }

  gst_speaker_track_get_face_rect (face, &r, "");

  if (filter->active_face) {
    gst_speaker_track_get_face_rect (filter->active_face, &rr, "");
    if (face == filter->active_face || (r.x == rr.x && r.y == rr.y
            && r.width == rr.width && r.height == rr.height)) {
      cb = 225, cg = 25, cr = 25;
      thikness = 2;
      is_active = TRUE;
    }
  }

  gst_speaker_track_draw_rect_spots (img, &r, CV_RGB (cr, cg, cb), thikness);

  have_nose = gst_speaker_track_get_face_rect (face, &rr, "nose.");
  if (have_nose && filter->display_nose) {
    center.x = cvRound ((rr.x + rr.width / 2));
    center.y = cvRound ((rr.y + rr.height / 2));
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);
  }

  have_mouth = gst_speaker_track_get_face_rect (face, &rr, "mouth.");
  if (have_mouth && filter->display_mouth) {
    gst_speaker_track_draw_rect_spots (img, &rr, CV_RGB (cr, cg, cb), 1);
  }

  have_eyes = gst_speaker_track_get_face_rect (face, &rr, "eyes.");
  if (have_eyes && filter->display_eyes) {
    center.x = rr.x;
    center.y = rr.y;
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);

    center.x = cvRound ((rr.x + rr.width));
    center.y = rr.y;
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);

    center.x = cvRound ((rr.x + rr.width));
    center.y = cvRound ((rr.y + rr.height));
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);

    center.x = rr.x;
    center.y = cvRound ((rr.y + rr.height));
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);

#if 0
    CvSize axes;
    gdouble w, h;
    w = sr->width / 2;
    h = sr->height / 2;
    center.x = cvRound ((rex + sr->x + w));
    center.y = cvRound ((rey + sr->y + h));
    axes.width = w * 1;         /* tweak for eyes form */
    axes.height = h;
    cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
        1, 8, 0);
#endif
  }

  return is_active;
}

static void
gst_speaker_track_mark_faces (GstSpeakerTrack * filter, IplImage * img)
{
  gint i;
  GstStructure *face;
  GList *iter = filter->faces;
  gboolean active_marked = FALSE, b;
  for (i = 0; iter; iter = iter->next, ++i) {
    face = (GstStructure *) iter->data;
    b = gst_speaker_track_mark_face (filter, img, face, i);
    active_marked = active_marked || b;
  }

  if (!active_marked) {
    gst_speaker_track_mark_face (filter, img, filter->active_face, 0);
  }
}

/* 
 * Performs the face detection
 */
static GstFlowReturn
gst_speaker_track_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    IplImage * img)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (base);
  gboolean do_display = FALSE;

  if (filter->display) {
    if (gst_buffer_is_writable (buf)) {
      do_display = TRUE;
    } else {
      GST_LOG_OBJECT (filter, "Buffer is not writable, not drawing faces.");
    }
  }

  if (!filter->detect_per_frame) {
    GstClockTime now = gst_util_get_timestamp ();
    GstClockTime diff = (now - filter->last_detect_stamp);
    //g_print ("%ld\n", (long int)((now - last) / 100000));
    if ((diff / 100000) <= FACE_DETECT_TIME_GAP) {
      if (do_display) {
        gst_speaker_track_mark_faces (filter, img);
      }
      return GST_FLOW_OK;
    }
    filter->last_detect_stamp = now;
  }

  if (filter->cvSpeakerTrack) {
    GstStructure *s;
    CvSeq *faces;
    CvSeq *mouth = NULL, *nose = NULL, *eyes = NULL;
    gint i, numFaces;
    GList *last = NULL;

    cvCvtColor (img, filter->cvGray, CV_RGB2GRAY);
    cvClearMemStorage (filter->cvStorage);

    if (!filter->locked
        && !gst_speaker_track_lock_detect (filter, img, do_display)) {
      return GST_FLOW_OK;
    }

    if (filter->faces) {
      g_list_free_full (filter->faces, (GDestroyNotify) gst_structure_free);
      filter->faces = NULL;
    }

    faces = gst_speaker_track_run_detector (filter, filter->cvSpeakerTrack,
        filter->min_size_width, filter->min_size_height);

    numFaces = (faces ? faces->total : 0);
    for (i = 0; i < numFaces; i++) {
      CvRect *r = (CvRect *) cvGetSeqElem (faces, i);
      guint mw = filter->min_size_width / 8;
      guint mh = filter->min_size_height / 8;
      guint rnx = 0, rny = 0, rnw, rnh;
      guint rmx = 0, rmy = 0, rmw, rmh;
      guint rex = 0, rey = 0, rew, reh;
      gboolean have_nose, have_mouth, have_eyes;        //, locked = FALSE;

      /* detect face features */

      if (filter->cvNoseDetect && filter->detect_nose) {
        rnx = r->x + r->width / 4;
        rny = r->y + r->height / 4;
        rnw = r->width / 2;
        rnh = r->height / 2;
        cvSetImageROI (filter->cvGray, cvRect (rnx, rny, rnw, rnh));
        nose =
            gst_speaker_track_run_detector (filter, filter->cvNoseDetect, mw,
            mh);
        have_nose = (nose && nose->total);
        cvResetImageROI (filter->cvGray);
      } else {
        have_nose = FALSE;
      }

      if (filter->cvMouthDetect && filter->detect_mouth) {
        rmx = r->x;
        rmy = r->y + r->height / 2;
        rmw = r->width;
        rmh = r->height / 2;
        cvSetImageROI (filter->cvGray, cvRect (rmx, rmy, rmw, rmh));
        mouth =
            gst_speaker_track_run_detector (filter, filter->cvMouthDetect, mw,
            mh);
        have_mouth = (mouth && mouth->total);
        cvResetImageROI (filter->cvGray);
      } else {
        have_mouth = FALSE;
      }

      if (filter->cvEyesDetect && filter->detect_eyes) {
        rex = r->x;
        rey = r->y;
        rew = r->width;
        reh = r->height / 2;
        cvSetImageROI (filter->cvGray, cvRect (rex, rey, rew, reh));
        eyes =
            gst_speaker_track_run_detector (filter, filter->cvEyesDetect, mw,
            mh);
        have_eyes = (eyes && eyes->total);
        cvResetImageROI (filter->cvGray);
      } else {
        have_eyes = FALSE;
      }

      GST_LOG_OBJECT (filter,
          "%2d/%2d: x,y = %4u,%4u: w.h = %4u,%4u : features(e,n,m) = %d,%d,%d",
          i, faces->total, r->x, r->y, r->width, r->height,
          have_eyes, have_nose, have_mouth);

      s = gst_structure_new ("face",
          "x", G_TYPE_UINT, r->x,
          "y", G_TYPE_UINT, r->y,
          "width", G_TYPE_UINT, r->width,
          "height", G_TYPE_UINT, r->height, NULL);
      if (have_nose) {
        CvRect *sr = (CvRect *) cvGetSeqElem (nose, 0);
        GST_LOG_OBJECT (filter, "nose/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            nose->total, rnx + sr->x, rny + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "nose.x", G_TYPE_UINT, rnx + sr->x,
            "nose.y", G_TYPE_UINT, rny + sr->y,
            "nose.width", G_TYPE_UINT, sr->width,
            "nose.height", G_TYPE_UINT, sr->height, NULL);
      }
      if (have_mouth) {
        CvRect *sr = (CvRect *) cvGetSeqElem (mouth, 0);
        GST_LOG_OBJECT (filter, "mouth/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            mouth->total, rmx + sr->x, rmy + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "mouth.x", G_TYPE_UINT, rmx + sr->x,
            "mouth.y", G_TYPE_UINT, rmy + sr->y,
            "mouth.width", G_TYPE_UINT, sr->width,
            "mouth.height", G_TYPE_UINT, sr->height, NULL);
      }
      if (have_eyes) {
        CvRect *sr = (CvRect *) cvGetSeqElem (eyes, 0);
        GST_LOG_OBJECT (filter, "eyes/%d: x,y = %4u,%4u: w.h = %4u,%4u",
            eyes->total, rex + sr->x, rey + sr->y, sr->width, sr->height);
        gst_structure_set (s,
            "eyes.x", G_TYPE_UINT, rex + sr->x,
            "eyes.y", G_TYPE_UINT, rey + sr->y,
            "eyes.width", G_TYPE_UINT, sr->width,
            "eyes.height", G_TYPE_UINT, sr->height, NULL);
      }

      filter->faces = g_list_append (filter->faces, s);
      last = filter->faces;
      s = NULL;
    }

    if (last) {
      i = g_list_position (filter->faces, last);
      s = gst_structure_copy ((GstStructure *) last->data);
      gst_speaker_track_lock (filter, img, buf, s, i);
    }

    if (do_display) {
      gst_speaker_track_mark_faces (filter, img);
    }
  }

  return GST_FLOW_OK;
}


static CvHaarClassifierCascade *
gst_speaker_track_load_profile (GstSpeakerTrack * filter, gchar * profile)
{
  CvHaarClassifierCascade *cascade;

  if (!(cascade = (CvHaarClassifierCascade *) cvLoad (profile, 0, 0, 0))) {
    GST_WARNING_OBJECT (filter, "Couldn't load Haar classifier cascade: %s.",
        profile);
  }
  return cascade;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_speaker_track_debug, "speakertrack",
      0,
      "Performs face detection on videos and images, providing detected positions via bus messages");

  return gst_element_register (plugin, "speakertrack", GST_RANK_NONE,
      GST_TYPE_SPEAKER_TRACK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    skeakertrack,
    "GStreamer Speaker Track Plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
