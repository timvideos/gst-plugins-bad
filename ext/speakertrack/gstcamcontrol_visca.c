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

#include "gstcamcontrol_visca.h"

G_DEFINE_TYPE (GstCamControllerVisca, gst_cam_controller_visca,
    GST_TYPE_CAM_CONTROLLER);

static void
gst_cam_controller_visca_init (GstCamControllerVisca * visca)
{
}

static void
gst_cam_controller_visca_finalize (GstCamControllerVisca * visca)
{
  G_OBJECT_CLASS (gst_cam_controller_visca_parent_class)
      ->finalize (G_OBJECT (visca));
}

static gboolean
gst_cam_controller_visca_open (GstCamControllerVisca * visca, const char *dev)
{
  g_print ("visca: open(%s)\n", dev);
  return FALSE;
}

static void
gst_cam_controller_visca_close (GstCamControllerVisca * visca)
{
  g_print ("visca: close()\n");
}

static gboolean
gst_cam_controller_visca_move (GstCamControllerVisca * visca, gint x, gint y)
{
  g_print ("visca: move(%d, %d)\n", x, y);
  return FALSE;
}

static gboolean
gst_cam_controller_visca_zoom (GstCamControllerVisca * visca, gint z)
{
  g_print ("visca: zoom(%d)\n", z);
  return FALSE;
}

static void
gst_cam_controller_visca_class_init (GstCamControllerViscaClass * viscaclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (viscaclass);
  GstCamControllerClass *camctl_class = GST_CAM_CONTROLLER_CLASS (viscaclass);
  object_class->finalize = GST_DEBUG_FUNCPTR (
      (GObjectFinalizeFunc) gst_cam_controller_visca_finalize);
  camctl_class->open = (GstCamControllerOpenFunc) gst_cam_controller_visca_open;
  camctl_class->close =
      (GstCamControllerCloseFunc) gst_cam_controller_visca_close;
  camctl_class->move = (GstCamControllerMoveFunc) gst_cam_controller_visca_move;
  camctl_class->zoom = (GstCamControllerZoomFunc) gst_cam_controller_visca_zoom;
}
