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

#include "gstcamcontrol_visca_sony.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

G_DEFINE_TYPE (GstCamControllerViscaSony, gst_cam_controller_visca_sony,
    GST_TYPE_CAM_CONTROLLER);

static void
gst_cam_controller_visca_sony_init (GstCamControllerViscaSony * visca_sony)
{
  visca_sony->sony = sony_visca_new ();
}

static void
gst_cam_controller_visca_sony_finalize (GstCamControllerViscaSony * visca_sony)
{
  sony_visca_close (visca_sony->sony);
  sony_visca_free (visca_sony->sony);
  visca_sony->sony = NULL;

  G_OBJECT_CLASS (gst_cam_controller_visca_sony_parent_class)
      ->finalize (G_OBJECT (visca_sony));
}

static void
gst_cam_controller_visca_sony_close (GstCamControllerViscaSony * visca_sony)
{
  g_print ("visca_sony: close()\n");
  sony_visca_close (visca_sony->sony);
}

static gboolean
gst_cam_controller_visca_sony_open (GstCamControllerViscaSony * visca_sony,
    const char *dev)
{
  g_print ("visca_sony: open(%s)\n", dev);
  sony_visca_open (visca_sony->sony, dev);
  return TRUE;
}

static gboolean
gst_cam_controller_visca_sony_pan (GstCamControllerViscaSony * visca_sony,
    gint speed, gint v)
{
  double d = ((double) v) / 100.0;
  g_print ("pan: %f\n", d);
  sony_visca_pan (visca_sony->sony, d);
  return TRUE;
}

static gboolean
gst_cam_controller_visca_sony_tilt (GstCamControllerViscaSony * visca_sony,
    gint speed, gint v)
{
  double d = 1.0 - ((double) v) / 100.0;
  g_print ("tilt: %f\n", d);
  sony_visca_tilt (visca_sony->sony, d);
  return TRUE;
}

static gboolean
gst_cam_controller_visca_sony_move (GstCamControllerViscaSony * visca_sony,
    gint speed, gint x, gint y)
{
  double dx = ((double) x) / 100.0;
  double dy = 1.0 - ((double) y) / 100.0;
  g_print ("move: %f, %f\n", dx, dy);
  sony_visca_pan (visca_sony->sony, dx);
  sony_visca_tilt (visca_sony->sony, dy);
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
gst_cam_controller_visca_sony_zoom (GstCamControllerViscaSony * visca_sony,
    gint speed, gint z)
{
  sony_visca_zoom (visca_sony->sony, z);
  return TRUE;
}

static void
gst_cam_controller_visca_sony_class_init (GstCamControllerViscaSonyClass *
    visca_sonyclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (visca_sonyclass);
  GstCamControllerClass *camctl_class =
      GST_CAM_CONTROLLER_CLASS (visca_sonyclass);
  object_class->finalize = GST_DEBUG_FUNCPTR ((GObjectFinalizeFunc)
      gst_cam_controller_visca_sony_finalize);
  camctl_class->open =
      (GstCamControllerOpenFunc) gst_cam_controller_visca_sony_open;
  camctl_class->close =
      (GstCamControllerCloseFunc) gst_cam_controller_visca_sony_close;
  camctl_class->pan =
      (GstCamControllerPanFunc) gst_cam_controller_visca_sony_pan;
  camctl_class->tilt =
      (GstCamControllerTiltFunc) gst_cam_controller_visca_sony_tilt;
  camctl_class->move =
      (GstCamControllerMoveFunc) gst_cam_controller_visca_sony_move;
  camctl_class->zoom =
      (GstCamControllerZoomFunc) gst_cam_controller_visca_sony_zoom;
}
