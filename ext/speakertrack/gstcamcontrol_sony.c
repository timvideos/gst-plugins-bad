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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcamcontrol_sony.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

G_DEFINE_TYPE (GstCamControllerSony, gst_cam_controller_sony,
    GST_TYPE_CAM_CONTROLLER);

static void
gst_cam_controller_sony_init (GstCamControllerSony * sony)
{
  sony->sony = sony_visca_new ();
}

static void
gst_cam_controller_sony_finalize (GstCamControllerSony * sony)
{
  sony_visca_close (sony->sony);
  sony_visca_free (sony->sony);
  sony->sony = NULL;

  G_OBJECT_CLASS (gst_cam_controller_sony_parent_class)
      ->finalize (G_OBJECT (sony));
}

static void
gst_cam_controller_sony_close (GstCamControllerSony * sony)
{
  g_print ("sony: close()\n");
  sony_visca_close (sony->sony);
}

static gboolean
gst_cam_controller_sony_open (GstCamControllerSony * sony, const char *dev)
{
  g_print ("sony: open(%s)\n", dev);
  sony_visca_open (sony->sony, dev);
  return TRUE;
}

static gboolean
gst_cam_controller_sony_pan (GstCamControllerSony * sony,
    double speed, double v)
{
  double d = v;                 // ((double) v) / 100.0;
  g_print ("pan: %f\n", d);
  sony_visca_pan (sony->sony, d);
  return TRUE;
}

static gboolean
gst_cam_controller_sony_tilt (GstCamControllerSony * sony,
    double speed, double v)
{
  double d = 1.0 - v;           //((double) v) / 100.0;
  g_print ("tilt: %f\n", d);
  sony_visca_tilt (sony->sony, d);
  return TRUE;
}

static gboolean
gst_cam_controller_sony_move (GstCamControllerSony * sony,
    double xspeed, double x, double yspeed, double y)
{
  double dx = x;                //((double) x) / 100.0;
  double dy = 1.0 - y;          //((double) y) / 100.0;
  g_print ("move: %f, %f\n", dx, dy);
  sony_visca_pan (sony->sony, dx);
  sony_visca_tilt (sony->sony, dy);
  return TRUE;
}

/**
 *  @brief Zooming camera.
 *  @param z Zero means stop zooming,
 *         z < 0 means TELE zoom,
 *         0 < z means WIDE zoom
 *  @return TRUE if commands sent.
 */
static gboolean
gst_cam_controller_sony_zoom (GstCamControllerSony * sony,
    double speed, double z)
{
  sony_visca_zoom (sony->sony, z);
  return TRUE;
}

static void
gst_cam_controller_sony_class_init (GstCamControllerSonyClass * sonyclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (sonyclass);
  GstCamControllerClass *camctl_class = GST_CAM_CONTROLLER_CLASS (sonyclass);
  object_class->finalize = GST_DEBUG_FUNCPTR ((GObjectFinalizeFunc)
      gst_cam_controller_sony_finalize);
  camctl_class->open = (GstCamControllerOpenFunc) gst_cam_controller_sony_open;
  camctl_class->close =
      (GstCamControllerCloseFunc) gst_cam_controller_sony_close;
  camctl_class->pan = (GstCamControllerPanFunc) gst_cam_controller_sony_pan;
  camctl_class->tilt = (GstCamControllerTiltFunc) gst_cam_controller_sony_tilt;
  camctl_class->move = (GstCamControllerMoveFunc) gst_cam_controller_sony_move;
  camctl_class->zoom = (GstCamControllerZoomFunc) gst_cam_controller_sony_zoom;
}
