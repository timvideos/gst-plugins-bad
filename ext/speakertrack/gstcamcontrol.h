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

/*! @file */

#ifndef __GST_CAM_CONTROL_H__
#define __GST_CAM_CONTROL_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS
#define GST_TYPE_CAM_CONTROLLER (gst_cam_controller_get_type ())
#define GST_CAM_CONTROLLER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GST_TYPE_CAM_CONTROLLER, GstCamController))
#define GST_CAM_CONTROLLER_CLASS(class) (G_TYPE_CHECK_CLASS_CAST ((class), GST_TYPE_CAM_CONTROLLER, GstCamControllerClass))
#define GST_IS_CAM_CONTROLLER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GST_TYPE_CAM_CONTROLLER))
#define GST_IS_CAM_CONTROLLER_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE ((class), GST_TYPE_CAM_CONTROLLER))

enum {
    CAM_RUN_NONE,
    CAM_RUN_LEFT,
    CAM_RUN_LEFT_TOP,
    CAM_RUN_LEFT_BOTTOM,
    CAM_RUN_RIGHT,
    CAM_RUN_RIGHT_TOP,
    CAM_RUN_RIGHT_BOTTOM,
    CAM_RUN_TOP,
    CAM_RUN_BOTTOM,
};

typedef struct _GstCamController
{
  GObject base;
  double pan_min, pan_max, pan_speed_min, pan_speed_max;
  double tilt_min, tilt_max, tilt_speed_min, tilt_speed_max;
  gchar *device_info;
} GstCamController;

typedef struct _GstCamControllerClass
{
  GObjectClass base_class;

  gboolean (*open) (GstCamController *, const char *dev);
  void (*close) (GstCamController *);

  gboolean (*pan) (GstCamController *, double speed, double v);
  gboolean (*tilt) (GstCamController *, double speed, double v);
  gboolean (*move) (GstCamController *, double xspeed, double x, double yspeed, double y);
  gboolean (*run) (GstCamController *, double xspeed, double yspeed, int dir, gboolean start);
  gboolean (*query) (GstCamController * controller, double *pan, double *tilt);
  gboolean (*zoom) (GstCamController *, double speed, double v);
} GstCamControllerClass;

typedef gboolean (*GstCamControllerOpenFunc) (GstCamController *,
    const char *dev);
typedef void (*GstCamControllerCloseFunc) (GstCamController *);
typedef gboolean (*GstCamControllerPanFunc) (GstCamController *, double x,
    double y);
typedef gboolean (*GstCamControllerTiltFunc) (GstCamController *, double x,
    double y);
typedef gboolean (*GstCamControllerMoveFunc) (GstCamController *, double xspeed,
    double y, double yspeed, double z);
typedef gboolean (*GstCamControllerRunFunc) (GstCamController *,
    double xspeed, double yspeed, int dir, gboolean start);
typedef gboolean (*GstCamControllerQueryFunc) (GstCamController * controller,
    double *pan, double *tilt);
typedef gboolean (*GstCamControllerZoomFunc) (GstCamController *, double x,
    double y);

GType gst_cam_controller_get_type (void);

GstCamController *gst_cam_controller_new (const char *protocol);
gboolean gst_cam_controller_open (GstCamController * controller, const char *dev);
void gst_cam_controller_close (GstCamController * controller);
gboolean gst_cam_controller_pan (GstCamController * controller, double speed, double v);
gboolean gst_cam_controller_tilt (GstCamController * controller, double speed, double v);
gboolean gst_cam_controller_move (GstCamController * controller, double xspeed, double x, double yspeed, double y);
gboolean gst_cam_controller_run (GstCamController * controller, double xspeed, double yspeed, int dir, gboolean start);
gboolean gst_cam_controller_query (GstCamController * controller, double *pan, double *tilt);
gboolean gst_cam_controller_zoom (GstCamController * controller, double speed, double v);

#define GST_TYPE_CAM_CONTROL \
  (gst_cam_control_get_type())
#define GST_CAM_CONTROL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAM_CONTROL,GstCamcontrol))
#define GST_CAM_CONTROL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAM_CONTROL,GstCamcontrolClass))
#define GST_IS_CAM_CONTROL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAM_CONTROL))
#define GST_IS_CAM_CONTROL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAM_CONTROL))

typedef struct _GstCamcontrol GstCamcontrol;
typedef struct _GstCamcontrolClass GstCamcontrolClass;

typedef struct _TrackingFace TrackingFace;

struct _GstCamcontrol
{
  GstBaseTransform base;

  const char *device;
  const char *protocol;
  GstCamController *controller;
};

struct _GstCamcontrolClass
{
  GstBaseTransformClass base_class;
};

GType gst_cam_control_get_type (void);

G_END_DECLS
#endif /* __GST_CAM_CONTROL_H__ */
