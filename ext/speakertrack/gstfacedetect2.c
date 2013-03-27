/*
 * GStreamer
 * Copyright (C) 2012,2013 Duzy Chan <code@duzy.info>
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
 * SECTION:element-facedetect
 *
 * Performs face detection on videos and images.
 *
 * The image is scaled down multiple times using the GstFaceDetect2::scale-factor
 * until the size is &lt;= GstFaceDetect2::min-size-width or 
 * GstFaceDetect2::min-size-height.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 autovideosrc ! decodebin2 ! colorspace ! facedetect ! videoconvert ! xvimagesink
 * ]| Detect and show faces
 * |[
 * gst-launch-0.10 autovideosrc ! video/x-raw,width=320,height=240 ! videoconvert ! facedetect min-size-width=60 min-size-height=60 ! colorspace ! xvimagesink
 * ]| Detect large faces on a smaller image 
 *
 * </refsect2>
 *
 * Also see the standard OpenCV facedetect module.
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
#include "gstfacedetect2.h"

GST_DEBUG_CATEGORY_STATIC (gst_face_detect2_debug);
#define GST_CAT_DEFAULT gst_face_detect2_debug

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
#define FACE_DETECT2_TIME_GAP 6000

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MARK,
  PROP_MARK_NOSE,
  PROP_MARK_MOUTH,
  PROP_MARK_EYES,
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
 * GstFaceDetect2Flags:
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_FACE_DETECT2_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstFaceDetect2Flags;

#define GST_TYPE_FACE_DETECT2_FLAGS (gst_face_detect2_flags_get_type())

static void
gst_face_detect2_register_flags (GType * id)
{
  static const GFlagsValue values[] = {
    {(guint) GST_FACE_DETECT2_HAAR_DO_CANNY_PRUNING,
        "Do Canny edge detection to discard some regions", "do-canny-pruning"},
    {0, NULL, NULL}
  };
  *id = g_flags_register_static ("GstFaceDetect2Flags", values);
}

static GType
gst_face_detect2_flags_get_type (void)
{
  static GType id;
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) gst_face_detect2_register_flags, &id);
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

G_DEFINE_TYPE (GstFaceDetect2, gst_face_detect2, GST_TYPE_OPENCV_VIDEO_FILTER);

static CvHaarClassifierCascade *gst_face_detect2_load_profile (GstFaceDetect2
    * detect, gchar * profile);

/* initialize the new element
 * initialize instance structure
 */
static void
gst_face_detect2_init (GstFaceDetect2 * detect)
{
  detect->mark_face = 1;
  detect->mark_eyes = 0;
  detect->mark_nose = 0;
  detect->mark_mouth = 0;
  detect->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
  detect->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
  detect->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
  detect->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
  detect->scale_factor = DEFAULT_SCALE_FACTOR;
  detect->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  detect->flags = DEFAULT_FLAGS;
  detect->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  detect->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;
  detect->cvFacedetect =
      gst_face_detect2_load_profile (detect, detect->face_profile);
  detect->cvNoseDetect =
      gst_face_detect2_load_profile (detect, detect->nose_profile);
  detect->cvMouthDetect =
      gst_face_detect2_load_profile (detect, detect->mouth_profile);
  detect->cvEyesDetect =
      gst_face_detect2_load_profile (detect, detect->eyes_profile);

  detect->focus_face = NULL;
  detect->faces = NULL;

  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (detect),
      TRUE);
}

/* Clean up */
static void
gst_face_detect2_finalize (GObject * obj)
{
  GstFaceDetect2 *detect = GST_FACE_DETECT2 (obj);

  if (detect->cvGray)
    cvReleaseImage (&detect->cvGray);
  if (detect->cvStorage)
    cvReleaseMemStorage (&detect->cvStorage);

  g_free (detect->face_profile);
  g_free (detect->nose_profile);
  g_free (detect->mouth_profile);
  g_free (detect->eyes_profile);

  g_list_free_full (detect->faces, (GDestroyNotify) gst_structure_free);
  gst_structure_free (detect->focus_face);

  detect->faces = NULL;
  detect->focus_face = NULL;

  if (detect->cvFacedetect)
    cvReleaseHaarClassifierCascade (&detect->cvFacedetect);
  if (detect->cvNoseDetect)
    cvReleaseHaarClassifierCascade (&detect->cvNoseDetect);
  if (detect->cvMouthDetect)
    cvReleaseHaarClassifierCascade (&detect->cvMouthDetect);
  if (detect->cvEyesDetect)
    cvReleaseHaarClassifierCascade (&detect->cvEyesDetect);

  G_OBJECT_CLASS (gst_face_detect2_parent_class)->finalize (obj);
}

static gboolean
gst_face_detect2_send_event (GstElement * element, GstEvent * event)
{
  GstElementClass *parent_class =
      GST_ELEMENT_CLASS (gst_face_detect2_parent_class);
  switch (GST_EVENT_TYPE (event)) {
    default:
      break;
  }
  return parent_class->send_event (element, event);
}

static void
gst_face_detect2_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFaceDetect2 *detect = GST_FACE_DETECT2 (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_free (detect->face_profile);
      if (detect->cvFacedetect)
        cvReleaseHaarClassifierCascade (&detect->cvFacedetect);
      if (strlen (g_value_get_string (value))) {
        detect->face_profile = g_value_dup_string (value);
      } else {
        detect->face_profile = g_strdup (DEFAULT_FACE_PROFILE);
      }
      detect->cvFacedetect =
          gst_face_detect2_load_profile (detect, detect->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_free (detect->nose_profile);
      if (detect->cvNoseDetect)
        cvReleaseHaarClassifierCascade (&detect->cvNoseDetect);
      if (strlen (g_value_get_string (value))) {
        detect->nose_profile = g_value_dup_string (value);
      } else {
        detect->nose_profile = g_strdup (DEFAULT_NOSE_PROFILE);
      }
      detect->cvNoseDetect =
          gst_face_detect2_load_profile (detect, detect->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_free (detect->mouth_profile);
      if (detect->cvMouthDetect)
        cvReleaseHaarClassifierCascade (&detect->cvMouthDetect);
      if (strlen (g_value_get_string (value))) {
        detect->mouth_profile = g_value_dup_string (value);
      } else {
        detect->mouth_profile = g_strdup (DEFAULT_MOUTH_PROFILE);
      }
      detect->cvMouthDetect =
          gst_face_detect2_load_profile (detect, detect->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_free (detect->eyes_profile);
      if (detect->cvEyesDetect)
        cvReleaseHaarClassifierCascade (&detect->cvEyesDetect);
      if (strlen (g_value_get_string (value))) {
        detect->eyes_profile = g_value_dup_string (value);
      } else {
        detect->eyes_profile = g_strdup (DEFAULT_EYES_PROFILE);
      }
      detect->cvEyesDetect =
          gst_face_detect2_load_profile (detect, detect->eyes_profile);
      break;
    case PROP_MARK:
      detect->mark_face = g_value_get_boolean (value);
      break;
    case PROP_MARK_NOSE:
      detect->mark_nose = g_value_get_boolean (value);
      break;
    case PROP_MARK_MOUTH:
      detect->mark_mouth = g_value_get_boolean (value);
      break;
    case PROP_MARK_EYES:
      detect->mark_eyes = g_value_get_boolean (value);
      break;
    case PROP_DETECT_NOSE:
      detect->detect_nose = g_value_get_boolean (value);
      break;
    case PROP_DETECT_MOUTH:
      detect->detect_mouth = g_value_get_boolean (value);
      break;
    case PROP_DETECT_EYES:
      detect->detect_eyes = g_value_get_boolean (value);
      break;
    case PROP_DETECT_PER_FRAME:
      detect->detect_per_frame = g_value_get_boolean (value);
      break;
    case PROP_SCALE_FACTOR:
      detect->scale_factor = g_value_get_double (value);
      break;
    case PROP_MIN_NEIGHBORS:
      detect->min_neighbors = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_WIDTH:
      detect->min_size_width = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      detect->min_size_height = g_value_get_int (value);
      break;
    case PROP_FLAGS:
      detect->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_face_detect2_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFaceDetect2 *detect = GST_FACE_DETECT2 (object);

  switch (prop_id) {
    case PROP_FACE_PROFILE:
      g_value_set_string (value, detect->face_profile);
      break;
    case PROP_NOSE_PROFILE:
      g_value_set_string (value, detect->nose_profile);
      break;
    case PROP_MOUTH_PROFILE:
      g_value_set_string (value, detect->mouth_profile);
      break;
    case PROP_EYES_PROFILE:
      g_value_set_string (value, detect->eyes_profile);
      break;
    case PROP_MARK:
      g_value_set_boolean (value, detect->mark_face);
      break;
    case PROP_MARK_NOSE:
      g_value_set_boolean (value, detect->mark_nose);
      break;
    case PROP_MARK_MOUTH:
      g_value_set_boolean (value, detect->mark_mouth);
      break;
    case PROP_MARK_EYES:
      g_value_set_boolean (value, detect->mark_eyes);
      break;
    case PROP_DETECT_NOSE:
      g_value_set_boolean (value, detect->detect_nose);
      break;
    case PROP_DETECT_MOUTH:
      g_value_set_boolean (value, detect->detect_mouth);
      break;
    case PROP_DETECT_EYES:
      g_value_set_boolean (value, detect->detect_eyes);
      break;
    case PROP_DETECT_PER_FRAME:
      g_value_set_boolean (value, detect->detect_per_frame);
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_double (value, detect->scale_factor);
      break;
    case PROP_MIN_NEIGHBORS:
      g_value_set_int (value, detect->min_neighbors);
      break;
    case PROP_MIN_SIZE_WIDTH:
      g_value_set_int (value, detect->min_size_width);
      break;
    case PROP_MIN_SIZE_HEIGHT:
      g_value_set_int (value, detect->min_size_height);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, detect->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_face_detect2_set_caps (GstOpencvVideoFilter * transform, gint in_width,
    gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels)
{
  GstFaceDetect2 *detect;

  detect = GST_FACE_DETECT2 (transform);

  if (detect->cvGray)
    cvReleaseImage (&detect->cvGray);

  detect->cvGray = cvCreateImage (cvSize (in_width, in_height),
      IPL_DEPTH_8U, 1);

  if (!detect->cvStorage)
    detect->cvStorage = cvCreateMemStorage (0);
  else
    cvClearMemStorage (detect->cvStorage);

  return TRUE;
}

static CvSeq *
gst_face_detect2_run_detector (GstFaceDetect2 * detect,
    CvHaarClassifierCascade * detector, gint min_size_width,
    gint min_size_height)
{
  return cvHaarDetectObjects (detect->cvGray, detector,
      detect->cvStorage, detect->scale_factor, detect->min_neighbors,
      detect->flags, cvSize (min_size_width, min_size_height)
#if (CV_MAJOR_VERSION >= 2) && (CV_MINOR_VERSION >= 2)
      , cvSize (min_size_width + 2, min_size_height + 2)
#endif
      );
}

static void
gst_face_detect2_draw_rect_spots (IplImage * img, CvRect * r, CvScalar color,
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
gst_face_detect2_get_face_rect (GstStructure * face, CvRect * rect,
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

/*
static void
gst_face_detect2_select_face (GstFaceDetect2 * detect, gint x, gint y)
{
  CvRect r;
  GList *face;

  if (!detect->faces) {
    g_print ("select: no faces, (%d, %d)\n", x, y);
    return;
  }

  for (face = detect->faces; face; face = g_list_next (face)) {
    if (gst_face_detect2_get_face_rect (GST_STRUCTURE (face->data), &r, "")) {
      if (r.x <= x && r.y <= y && x <= (r.x + r.width) && y <= (r.y + r.height)) {
        detect->focus_face = gst_structure_copy (GST_STRUCTURE (face->data));
        g_print ("select: [(%d, %d), %d, %d]\n", r.x, r.y, r.width, r.height);
      }
    }
  }
}
*/

static gboolean
gst_face_detect2_mark_face (GstFaceDetect2 * detect, IplImage * img,
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

  gst_face_detect2_get_face_rect (face, &r, "");

  if (detect->focus_face) {
    gst_face_detect2_get_face_rect (detect->focus_face, &rr, "");
    if (face == detect->focus_face || (r.x == rr.x && r.y == rr.y
            && r.width == rr.width && r.height == rr.height)) {
      cb = 225, cg = 25, cr = 25;
      thikness = 2;
      is_active = TRUE;
    }
  }

  if (detect->mark_face) {
    gst_face_detect2_draw_rect_spots (img, &r, CV_RGB (cr, cg, cb), thikness);
  }

  have_nose = gst_face_detect2_get_face_rect (face, &rr, "nose.");
  if (have_nose && detect->mark_nose) {
    center.x = cvRound ((rr.x + rr.width / 2));
    center.y = cvRound ((rr.y + rr.height / 2));
    cvCircle (img, center, 1, CV_RGB (cr, cg, cb), 1, 8, 0);
  }

  have_mouth = gst_face_detect2_get_face_rect (face, &rr, "mouth.");
  if (have_mouth && detect->mark_mouth) {
    gst_face_detect2_draw_rect_spots (img, &rr, CV_RGB (cr, cg, cb), 1);
  }

  have_eyes = gst_face_detect2_get_face_rect (face, &rr, "eyes.");
  if (have_eyes && detect->mark_eyes) {
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
    axes.width = w * 1;
    axes.height = h;
    cvEllipse (img, center, axes, 0.0, 0.0, 360.0, CV_RGB (cr, cg, cb),
        1, 8, 0);
#endif
  }

  return is_active;
}

static void
gst_face_detect2_mark_faces (GstFaceDetect2 * detect, IplImage * img)
{
  gint i;
  GstStructure *face;
  GList *iter = detect->faces;
  gboolean active_marked = FALSE, b;
  for (i = 0; iter; iter = iter->next, ++i) {
    face = GST_STRUCTURE (iter->data);
    b = gst_face_detect2_mark_face (detect, img, face, i);
    active_marked = active_marked || b;
  }

  if (!active_marked) {
    gst_face_detect2_mark_face (detect, img, detect->focus_face, 0);
  }
}


/**
 * @gst_face_detect2_report_faces:
 *
 * Report faces to the sibling element. It will take the ownership of
 * the @newfaces. 
 */
static void
gst_face_detect2_report_faces (GstFaceDetect2 * detect, GList * newfaces)
{
  GList *newface;
  GstIterator *iter = NULL;
  gboolean done = FALSE;
  GValue item = { 0 };

#if 0
  iter = gst_element_iterate_sink_pads (GST_ELEMENT (detect));
  while (!done) {
    switch (gst_iterator_next (iter, &item)) {
      case GST_ITERATOR_OK:
      {
        GstPad *sinkpad = (GstPad *) g_value_get_object (&item);
        /*
           GstClockTime pad_min_latency, pad_max_latency;
           gboolean pad_us_live;

           if (gst_pad_peer_query (sinkpad, query)) {
           gst_query_parse_latency (query, &pad_us_live, &pad_min_latency,
           &pad_max_latency);

           res = TRUE;

           GST_DEBUG_OBJECT (adder, "Peer latency for pad %s: min %"
           GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
           GST_PAD_NAME (sinkpad),
           GST_TIME_ARGS (pad_min_latency), GST_TIME_ARGS (pad_max_latency));

           min_latency = MAX (pad_min_latency, min_latency);
           max_latency = MIN (pad_max_latency, max_latency);
           }
         */
        for (newface = newfaces; newface; newface = g_list_next (newface)) {
          GstStructure *s = gst_structure_copy (GST_STRUCTURE (newface->data));
          GstEvent *ev = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
          GstPad *pad = gst_pad_get_peer (sinkpad);
          g_printf ("%s:%d: %s, %s\n", __FILE__, __LINE__,
              gst_structure_get_name (s),
              gst_element_get_name (GST_ELEMENT (gst_pad_get_parent (pad))));
          if (!gst_pad_send_event (pad, ev)) {
            /*
               GST_WARNING_OBJECT (detect, "face ignored");
             */
          }
          gst_object_unref (pad);
        }
      }
        break;
      case GST_ITERATOR_RESYNC:
        //min_latency = 0;
        //max_latency = G_MAXUINT64;
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_ERROR_OBJECT (detect, "Error looping sink pads");
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
    g_value_reset (&item);
  }
  g_value_unset (&item);
  gst_iterator_free (iter);
#endif

#if 1
  done = FALSE;
  iter = gst_element_iterate_src_pads (GST_ELEMENT (detect));
  while (!done) {
    switch (gst_iterator_next (iter, &item)) {
      case GST_ITERATOR_OK:
      {
        GstPad *srcpad = (GstPad *) g_value_get_object (&item);
        for (newface = newfaces; newface; newface = g_list_next (newface)) {
          GstStructure *s = gst_structure_copy (GST_STRUCTURE (newface->data));
          GstEvent *ev = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);
          GstPad *pad = gst_pad_get_peer (srcpad);
          /*
             g_printf ("%s:%d: %s, %s\n", __FILE__, __LINE__,
             gst_structure_get_name (s),
             gst_element_get_name (GST_ELEMENT (gst_pad_get_parent (pad))));
           */
          if (!gst_pad_send_event (pad, ev)) {
            /*
               GST_WARNING_OBJECT (detect, "face ignored");
             */
          }
          gst_object_unref (pad);
        }
      }
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_ERROR_OBJECT (detect, "Error looping sink pads");
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
    g_value_reset (&item);
  }
  g_value_unset (&item);
  gst_iterator_free (iter);
#endif

  return;
}

static GstMessage *
gst_face_detect2_message_new (GstFaceDetect2 * filter, GstBuffer * buf)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (filter);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));

  s = gst_structure_new ("facedetect",
      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (buf),
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time,
      "duration", G_TYPE_UINT64, GST_BUFFER_DURATION (buf), NULL);

  return gst_message_new_element (GST_OBJECT (filter), s);
}

/* 
 * Performs the face detection
 */
static GstFlowReturn
gst_face_detect2_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    IplImage * img)
{
  GstFaceDetect2 *detect = GST_FACE_DETECT2 (base);
  CvSeq *faces = NULL;
  GList *newfaces = NULL;
  gint i, numFaces;

  if (!detect->cvFacedetect) {
    return GST_FLOW_OK;
  }

  if (!detect->detect_per_frame) {
    GstClockTime now = gst_util_get_timestamp ();
    GstClockTime diff = (now - detect->last_detect_stamp);
    //g_print ("%ld\n", (long int)((now - last) / 100000));
    if ((diff / 100000) <= FACE_DETECT2_TIME_GAP) {
      if (gst_buffer_is_writable (buf)) {
        gst_face_detect2_mark_faces (detect, img);
      }
      return GST_FLOW_OK;
    }
    detect->last_detect_stamp = now;
  }

  cvCvtColor (img, detect->cvGray, CV_RGB2GRAY);
  cvClearMemStorage (detect->cvStorage);

  faces = gst_face_detect2_run_detector (detect, detect->cvFacedetect,
      detect->min_size_width, detect->min_size_height);

  numFaces = (faces ? faces->total : 0);
  for (i = 0; i < numFaces; i++) {
    CvRect *r = (CvRect *) cvGetSeqElem (faces, i);
    guint mw = detect->min_size_width / 8;
    guint mh = detect->min_size_height / 8;
    guint rnx = 0, rny = 0, rnw, rnh;
    guint rmx = 0, rmy = 0, rmw, rmh;
    guint rex = 0, rey = 0, rew, reh;
    CvSeq *mouth = NULL, *nose = NULL, *eyes = NULL;
    gboolean have_nose, have_mouth, have_eyes;
    GstStructure *s = NULL;

    /* detect face features */

    if (detect->cvNoseDetect && detect->detect_nose) {
      rnx = r->x + r->width / 4;
      rny = r->y + r->height / 4;
      rnw = r->width / 2;
      rnh = r->height / 2;
      cvSetImageROI (detect->cvGray, cvRect (rnx, rny, rnw, rnh));
      nose =
          gst_face_detect2_run_detector (detect, detect->cvNoseDetect, mw, mh);
      have_nose = (nose && nose->total);
      cvResetImageROI (detect->cvGray);
    } else {
      have_nose = FALSE;
    }

    if (detect->cvMouthDetect && detect->detect_mouth) {
      rmx = r->x;
      rmy = r->y + r->height / 2;
      rmw = r->width;
      rmh = r->height / 2;
      cvSetImageROI (detect->cvGray, cvRect (rmx, rmy, rmw, rmh));
      mouth =
          gst_face_detect2_run_detector (detect, detect->cvMouthDetect, mw, mh);
      have_mouth = (mouth && mouth->total);
      cvResetImageROI (detect->cvGray);
    } else {
      have_mouth = FALSE;
    }

    if (detect->cvEyesDetect && detect->detect_eyes) {
      rex = r->x;
      rey = r->y;
      rew = r->width;
      reh = r->height / 2;
      cvSetImageROI (detect->cvGray, cvRect (rex, rey, rew, reh));
      eyes =
          gst_face_detect2_run_detector (detect, detect->cvEyesDetect, mw, mh);
      have_eyes = (eyes && eyes->total);
      cvResetImageROI (detect->cvGray);
    } else {
      have_eyes = FALSE;
    }

    GST_LOG_OBJECT (detect,
        "%2d/%2d: x,y = %4u,%4u: w.h = %4u,%4u : features(e,n,m) = %d,%d,%d",
        i, faces->total, r->x, r->y, r->width, r->height,
        have_eyes, have_nose, have_mouth);

    s = gst_structure_new ("face",
        "x", G_TYPE_UINT, r->x,
        "y", G_TYPE_UINT, r->y,
        "width", G_TYPE_UINT, r->width, "height", G_TYPE_UINT, r->height, NULL);
    if (have_nose) {
      CvRect *sr = (CvRect *) cvGetSeqElem (nose, 0);
      GST_LOG_OBJECT (detect, "nose/%d: x,y = %4u,%4u: w.h = %4u,%4u",
          nose->total, rnx + sr->x, rny + sr->y, sr->width, sr->height);
      gst_structure_set (s,
          "nose.x", G_TYPE_UINT, rnx + sr->x,
          "nose.y", G_TYPE_UINT, rny + sr->y,
          "nose.width", G_TYPE_UINT, sr->width,
          "nose.height", G_TYPE_UINT, sr->height, NULL);
    }
    if (have_mouth) {
      CvRect *sr = (CvRect *) cvGetSeqElem (mouth, 0);
      GST_LOG_OBJECT (detect, "mouth/%d: x,y = %4u,%4u: w.h = %4u,%4u",
          mouth->total, rmx + sr->x, rmy + sr->y, sr->width, sr->height);
      gst_structure_set (s,
          "mouth.x", G_TYPE_UINT, rmx + sr->x,
          "mouth.y", G_TYPE_UINT, rmy + sr->y,
          "mouth.width", G_TYPE_UINT, sr->width,
          "mouth.height", G_TYPE_UINT, sr->height, NULL);
    }
    if (have_eyes) {
      CvRect *sr = (CvRect *) cvGetSeqElem (eyes, 0);
      GST_LOG_OBJECT (detect, "eyes/%d: x,y = %4u,%4u: w.h = %4u,%4u",
          eyes->total, rex + sr->x, rey + sr->y, sr->width, sr->height);
      gst_structure_set (s,
          "eyes.x", G_TYPE_UINT, rex + sr->x,
          "eyes.y", G_TYPE_UINT, rey + sr->y,
          "eyes.width", G_TYPE_UINT, sr->width,
          "eyes.height", G_TYPE_UINT, sr->height, NULL);
    }

    newfaces = g_list_append (newfaces, s);
  }

  if (newfaces) {
    GstMessage *msg = gst_face_detect2_message_new (detect, buf);
    GValue facelist = { 0 };
    GList *face = NULL;

    if (detect->faces)
      g_list_free_full (detect->faces, (GDestroyNotify) gst_structure_free);
    gst_face_detect2_report_faces (detect, detect->faces = newfaces);

    g_value_init (&facelist, GST_TYPE_LIST);
    for (face = detect->faces; face; face = g_list_next (face)) {
      GValue facedata = { 0 };
      g_value_init (&facedata, GST_TYPE_STRUCTURE);
      g_value_take_boxed (&facedata,
          gst_structure_copy (GST_STRUCTURE (face->data)));
      gst_value_list_append_value (&facelist, &facedata);
      g_value_unset (&facedata);
    }

    gst_structure_set_value ((GstStructure *) gst_message_get_structure (msg),
        "faces", &facelist);
    g_value_unset (&facelist);

    gst_element_post_message (GST_ELEMENT (detect), msg);
  }

  if (gst_buffer_is_writable (buf)) {
    gst_face_detect2_mark_faces (detect, img);
  }
  return GST_FLOW_OK;
}

static CvHaarClassifierCascade *
gst_face_detect2_load_profile (GstFaceDetect2 * detect, gchar * profile)
{
  CvHaarClassifierCascade *cascade;

  if (!(cascade = (CvHaarClassifierCascade *) cvLoad (profile, 0, 0, 0))) {
    GST_WARNING_OBJECT (detect, "Couldn't load Haar classifier cascade: %s.",
        profile);
  }
  return cascade;
}

/* initialize the facedetect's class */
static void
gst_face_detect2_class_init (GstFaceDetect2Class * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstOpencvVideoFilterClass *gstopencvbasefilter_class =
      (GstOpencvVideoFilterClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_face_detect2_finalize);
  gobject_class->set_property = gst_face_detect2_set_property;
  gobject_class->get_property = gst_face_detect2_get_property;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_face_detect2_transform_ip;
  gstopencvbasefilter_class->cv_set_caps = gst_face_detect2_set_caps;

  g_object_class_install_property (gobject_class, PROP_MARK,
      g_param_spec_boolean ("display", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MARK_NOSE,
      g_param_spec_boolean ("display-nose", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MARK_MOUTH,
      g_param_spec_boolean ("display-mouth", "Display",
          "Sets whether the detected faces should be highlighted in the output",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MARK_EYES,
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
          GST_TYPE_FACE_DETECT2_FLAGS, DEFAULT_FLAGS,
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

  element_class->send_event = GST_DEBUG_FUNCPTR (gst_face_detect2_send_event);

  gst_element_class_set_static_metadata (element_class,
      "facedetect2",
      "Filter/Effect/Video",
      "Detect faces on videos, report faces to downstream elements.",
      "Duzy Chan <code@duzy.info>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_face_detect2_debug, "facedetect2",
      0, "Performs face detection on videos, report faces via bus messages.");
}
