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
 * SECTION:element-camcontrol
 *
 * Performs face detection on videos and images.
 *
 * The image is scaled down multiple times using the GstCamcontrol::scale-factor
 * until the size is &lt;= GstCamcontrol::min-size-width or 
 * GstCamcontrol::min-size-height. 
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 autovideosrc ! decodebin2 ! colorspace ! facedetect ! camcontrol ! videoconvert ! xvimagesink
 * ]| Detect and show faces
 * |[
 * gst-launch-0.10 autovideosrc ! video/x-raw,width=320,height=240 ! videoconvert ! colorspace ! camcontrol min-size-width=60 min-size-height=60 ! xvimagesink
 * ]| Detect large faces on a smaller image 
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "gstcamcontrol.h"

GST_DEBUG_CATEGORY_STATIC (gst_cam_control_debug);
#define GST_CAT_DEFAULT gst_cam_control_debug

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
 * GstCamcontrolFlags:
 *
 * Flags parameter to OpenCV's cvHaarDetectObjects function.
 */
typedef enum
{
  GST_CAM_CONTROL_HAAR_DO_CANNY_PRUNING = (1 << 0)
} GstCamcontrolFlags;

#define GST_TYPE_CAM_CONTROL_FLAGS (gst_cam_control_flags_get_type())

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

G_DEFINE_TYPE (GstCamcontrol, gst_cam_control, GST_TYPE_BASE_TRANSFORM);

/* initialize the new element
 * initialize instance structure
 */
static void
gst_cam_control_init (GstCamcontrol * track)
{
}

static void
gst_cam_control_finalize (GObject * obj)
{
  G_OBJECT_CLASS (gst_cam_control_parent_class)->finalize (obj);
}

static void
gst_cam_control_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCamcontrol *camctl = GST_CAM_CONTROL (object);
  (void) camctl;

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cam_control_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCamcontrol *camctl = GST_CAM_CONTROL (object);
  (void) camctl;

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cam_control_face_track (GstCamcontrol * camctl, const GstStructure * s)
{
  guint fx, fy, fw, fh, x, y;
  gst_structure_get_uint (s, "x", &fx);
  gst_structure_get_uint (s, "y", &fy);
  gst_structure_get_uint (s, "width", &fw);
  gst_structure_get_uint (s, "height", &fh);

  x = fx + fw * 0.5;
  y = fy + fh * 0.5;

  g_print ("camctl: (%d, %d)\n", x, y);
}

static gboolean
gst_cam_control_send_event (GstElement * element, GstEvent * event)
{
  GstElementClass *element_class =
      GST_ELEMENT_CLASS (gst_cam_control_parent_class);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      const GstStructure *s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "camctl")) {
        // ...
        return TRUE;
      }
    }
      break;

    default:
      break;
  }
  return element_class->send_event (element, event);
}

static gboolean
gst_cam_control_sink_eventfunc (GstBaseTransform * trans, GstEvent * event)
{
  GstCamcontrol *camctl = GST_CAM_CONTROL (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      const GstStructure *s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "facetrack")) {
        gst_cam_control_face_track (camctl, s);
        return TRUE;
      }
    }
      break;

    default:
      break;
  }

  return gst_pad_push_event (trans->srcpad, event);
}

static gboolean
gst_cam_control_src_eventfunc (GstBaseTransform * trans, GstEvent * event)
{
  GstCamcontrol *camctl = GST_CAM_CONTROL (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:{
      const GstStructure *s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "facetrack")) {
        gst_cam_control_face_track (camctl, s);
        return TRUE;
      }
    }
      break;

    default:
      break;
  }

  return gst_pad_push_event (trans->sinkpad, event);
}

/* initialize the camcontrol's class */
static void
gst_cam_control_class_init (GstCamcontrolClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_cam_control_finalize);
  gobject_class->set_property = gst_cam_control_set_property;
  gobject_class->get_property = gst_cam_control_get_property;

  element_class->send_event = GST_DEBUG_FUNCPTR (gst_cam_control_send_event);

  transform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_cam_control_sink_eventfunc);
  transform_class->src_event =
      GST_DEBUG_FUNCPTR (gst_cam_control_src_eventfunc);

  gst_element_class_set_static_metadata (element_class,
      "camcontrol",
      "Filter/Effect/Video",
      "Control PTZ camera according speakertrack.",
      "Duzy Chan <code@duzy.info>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_cam_control_debug, "camcontrol",
      0, "Control camera according speakertrack.");
}
