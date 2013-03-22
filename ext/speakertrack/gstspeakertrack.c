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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <glib/gprintf.h>

#include "gstspeakertrack.h"

GST_DEBUG_CATEGORY_STATIC (gst_speaker_track_debug);
#define GST_CAT_DEFAULT gst_speaker_track_debug

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

/*
static void
gst_speaker_track_register_flags (GType * id)
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

  g_once (&once, (GThreadFunc) gst_speaker_track_register_flags, &id);
  return id;
}
*/

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

G_DEFINE_TYPE (GstSpeakerTrack, gst_speaker_track, GST_TYPE_BASE_TRANSFORM);

static void gst_speaker_track_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_speaker_track_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/*
static void gst_speaker_track_select_face (GstSpeakerTrack * filter,
    gint x, gint y);
*/

static void
gst_speaker_track_finalize (GObject * obj)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (obj);

  (void) filter;

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
        if (gst_structure_get_int (s, "x", &x) &&
            gst_structure_get_int (s, "y", &y)) {
          //gst_speaker_track_select_face (GST_SPEAKER_TRACK (element), x, y);
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

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_speaker_track_finalize);
  gobject_class->set_property = gst_speaker_track_set_property;
  gobject_class->get_property = gst_speaker_track_get_property;

  element_class->send_event = GST_DEBUG_FUNCPTR (gst_speaker_track_send_event);

  gst_element_class_set_static_metadata (element_class,
      "speakertrack",
      "Filter/Effect/Video",
      "Tracks speaker on videos and images, providing speaker positions via bus messages",
      "Duzy Chan <code@duzy.info>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_speaker_track_debug, "speakertrack",
      0, "Tracking selected face, report tracking face via bus messages.");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_speaker_track_init (GstSpeakerTrack * filter)
{
}

static void
gst_speaker_track_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpeakerTrack *filter = GST_SPEAKER_TRACK (object);

  (void) filter;

  switch (prop_id) {
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

  (void) filter;

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
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

static gboolean
gst_speaker_track_compare_face (GstSpeakerTrack * filter, GstStructure * face1,
    GstStructure * face2)
{
  CvRect r1, r2, r;
  float a = FACE_MOTION_SCALE, ix, iy, iw, ih;
  float off = FACE_MOTION_SHIFT;

  gst_speaker_track_get_face_rect (face1, &r1, "");
  gst_speaker_track_get_face_rect (face2, &r2, "");
  r.x = max (r1.x, r2.x);
  r.y = max (r1.y, r2.y);
  r.width = min (r1.x + r1.width, r2.x + r2.width) - r1.x;
  r.height = min (r1.y + r1.height, r2.y + r2.height) - r2.y;
  if (r.width < 0 || r.height < 0) {
    r.width = 0, r.height = 0;
  }

  ix = (float) abs (r2.x - r1.x) / (float) r1.width;
  iy = (float) abs (r2.y - r1.y) / (float) r1.height;
  iw = (float) r.width / (float) r1.width;
  ih = (float) r.height / (float) r1.height;
  if (ix <= off && iy <= off && a <= iw && a <= ih) {
    return TRUE;
  }

  return FALSE;
}

static void
gst_speaker_track_select_face (GstSpeakerTrack * filter, gint x, gint y)
{
  CvRect r;
  GList *face;

  if (!filter->faces) {
    g_print ("select: no faces, (%d, %d)\n", x, y);
    return;
  }

  for (face = filter->faces; face; face = g_list_next (face)) {
    if (gst_speaker_track_get_face_rect (GST_STRUCTURE (face->data), &r, "")) {
      if (r.x <= x && r.y <= y && x <= (r.x + r.width) && y <= (r.y + r.height)) {
        filter->active_face = gst_structure_copy (GST_STRUCTURE (face->data));
        g_print ("select: [(%d, %d), %d, %d]\n", r.x, r.y, r.width, r.height);
      }
    }
  }
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
    axes.width = w * 1;
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

static void
gst_speaker_track_update_faces (GstSpeakerTrack * filter, GList * newfaces)
{
  GList *face, *newface;

  if (!filter->faces) {
    filter->faces = newfaces;
    return;
  }

  for (face = filter->faces; face; face = face->next) {
    //g_print ("newfaces: %d\n", g_list_length (newfaces));
    for (newface = newfaces; newface; newface = newface->next) {
      if (!newface->data)
        continue;

      if (gst_speaker_track_compare_face (filter, GST_STRUCTURE (face->data),
              GST_STRUCTURE (newface->data))) {
        gst_structure_free (GST_STRUCTURE (face->data));
        face->data = newface->data;
        newface->data = NULL;
        newface = g_list_remove_link (newfaces, newface);
        break;
      }
    }
    //g_print ("newfaces: %d\n", g_list_length (newfaces));
  }

  for (newface = newfaces; newface; newface = newface->next) {
    if (newface->data) {
      filter->faces = g_list_append (filter->faces, newface->data);
      newface->data = NULL;
    }
    //g_print ("newface: %p\n", newface->data);
  }

  g_list_free (newfaces);

  if (filter->active_face) {
    for (face = filter->faces; face; face = face->next) {
      if (gst_speaker_track_compare_face (filter, filter->active_face,
              GST_STRUCTURE (face->data))) {
        gst_structure_free (filter->active_face);
        filter->active_face = gst_structure_copy (GST_STRUCTURE (face->data));
      }
    }
  }
}
*/

#include "gstfacedetect.h"

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "facedetect", GST_RANK_NONE,
          GST_TYPE_FACE_DETECT)) {
    return FALSE;
  }

  if (!gst_element_register (plugin, "speakertrack", GST_RANK_NONE,
          GST_TYPE_SPEAKER_TRACK)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    skeakertrack,
    "GStreamer Speaker Track Plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
