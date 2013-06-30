/* GstSwitch							    -*- c -*-
 * Copyright (C) 2012,2013 Duzy Chan <code@duzy.info>
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

/*! @file */

#ifndef __GST_CAM_CONTROLLER_CANON_H__by_Duzy_Chan__
#define __GST_CAM_CONTROLLER_CANON_H__by_Duzy_Chan__ 1
#include "gstcamcontrol.h"
#include <termios.h>

#define GST_TYPE_CAM_CONTROLLER_CANON (gst_cam_controller_canon_get_type ())
#define GST_CAM_CONTROLLER_CANON(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GST_TYPE_CAM_CONTROLLER_CANON, GstCamControllerCanon))
#define GST_CAM_CONTROLLER_CANON_CLASS(class) (G_TYPE_CHECK_CLASS_CAST ((class), GST_TYPE_CAM_CONTROLLER_CANON, GstCamControllerCanonClass))
#define GST_IS_CAM_CONTROLLER_CANON(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GST_TYPE_CAM_CONTROLLER_CANON))
#define GST_IS_CAM_CONTROLLER_CANON_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE ((class), GST_TYPE_CAM_CONTROLLER_CANON))

typedef struct _GstCamControllerCanon
{
  GstCamController base;

  const char *device;
  struct termios options;
  gint fd;
  guint zoom_speed;
  guint pan_speed;
  guint tilt_speed;
} GstCamControllerCanon;

typedef struct _GstCamControllerCanonClass
{
  GstCamControllerClass base_class;
} GstCamControllerCanonClass;

GType gst_cam_controller_canon_get_type (void);

#endif //__GST_CAM_CONTROLLER_CANON_H__by_Duzy_Chan__
