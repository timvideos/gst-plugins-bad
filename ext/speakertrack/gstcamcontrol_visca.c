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
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

G_DEFINE_TYPE (GstCamControllerVisca, gst_cam_controller_visca,
    GST_TYPE_CAM_CONTROLLER);

// @fixme: Better using libvisca, but I can't see this is working with
//         my camera.

//
#define VISCA_COMMAND                    0x01
#define VISCA_CATEGORY_CAMERA1           0x04
#define VISCA_CATEGORY_CAMERA2           0x07
#define VISCA_CATEGORY_PAN_TILTER        0x06
#define VISCA_TERMINATOR                 0xFF

//
#define VISCA_ZOOM                       0x07
#define VISCA_ZOOM_STOP                  0x00
#define VISCA_ZOOM_TELE                  0x02
#define VISCA_ZOOM_WIDE                  0x03
#define VISCA_ZOOM_TELE_SPEED            0x20
#define VISCA_ZOOM_WIDE_SPEED            0x30
#define VISCA_PT_ABSOLUTE_POSITION       0x02
#define VISCA_PT_RELATIVE_POSITION       0x03
#define VISCA_PT_HOME                    0x04
#define VISCA_PT_RESET                   0x05
#define VISCA_PT_LIMITSET                0x07

#define visca_message_init(a) { {0}, 0, (a) }

typedef struct _visca_message
{
  char buffer[32];              // 32 bytes for one command max
  int len;
  int address;
} visca_message;

static void
visca_message_append (visca_message * msg, char c)
{
  msg->buffer[msg->len++] = c;
}

static void
visca_message_reset (visca_message * msg)
{
  msg->len = 0;
  bzero (msg->buffer, sizeof (msg->buffer));
}

static gboolean
visca_message_send (int fd, const visca_message * msg)
{
  int len = 1 + msg->len + 1;
  char b[32];
  if (msg->len <= 0 || sizeof (b) <= len || 7 < msg->address) {
    return FALSE;
  }

  b[0] = 0x80;
  b[0] |= (msg->address << 4);
#if 0
  if (0 < broadcast) {
    b[0] |= (broadcast << 3);
    b[0] &= 0xF8;
  } else {
    b[0] |= camera_address;
  }
#endif

  memcpy (&b[1], msg->buffer, msg->len);
  b[1 + msg->len] = VISCA_TERMINATOR;

  if (write (fd, msg->buffer, msg->len) < msg->len) {
    return FALSE;
  }
  return TRUE;
}

static gboolean
visca_message_reply (int fd, visca_message * reply)
{
  int available_bytes = 0, n = 0;

  do {
    ioctl (fd, FIONREAD, &available_bytes);
    usleep (500);
  } while (available_bytes == 0);

  do {
    if (read (fd, &reply->buffer[n], 1) != 1) {
      return FALSE;
    }
    if (reply->buffer[n] == VISCA_TERMINATOR) {
      break;
    }
    n += 1;
    usleep (1);
  } while (1);

  return TRUE;
}

static gboolean
visca_message_send_with_reply (int fd, const visca_message * msg,
    visca_message * reply)
{
  if (!visca_message_send (fd, msg)) {
    return FALSE;
  }
  return visca_message_reply (fd, reply);
}

static void
gst_cam_controller_visca_init (GstCamControllerVisca * visca)
{
  visca->fd = -1;
  visca->zoom_speed = 100;
  visca->pan_speed = 100;
  visca->tilt_speed = 100;

  bzero (&visca->options, sizeof (visca->options));
}

static void
gst_cam_controller_visca_finalize (GstCamControllerVisca * visca)
{
  G_OBJECT_CLASS (gst_cam_controller_visca_parent_class)
      ->finalize (G_OBJECT (visca));
}

static void
gst_cam_controller_visca_close (GstCamControllerVisca * visca)
{
  g_print ("visca: close()\n");

  if (0 < visca->fd) {
    close (visca->fd);
    visca->fd = -1;

    g_free ((void *) visca->device);
    visca->device = NULL;

    bzero (&visca->options, sizeof (visca->options));
  }
}

static gboolean
gst_cam_controller_visca_open (GstCamControllerVisca * visca, const char *dev)
{
  g_print ("visca: open(%s)\n", dev);

  if (0 < visca->fd) {
    gst_cam_controller_visca_close (visca);
  }

  g_free ((void *) visca->device);
  visca->device = g_strdup (dev);
  visca->fd = open (dev, O_RDWR | O_NDELAY | O_NOCTTY);

  if (visca->fd == -1) {
    g_print ("visca: open(%s) error: %s\n", dev, strerror (errno));
    return FALSE;
  }

  fcntl (visca->fd, F_SETFL, 0);
  /* Setting port parameters */
  tcgetattr (visca->fd, &visca->options);

  /* control flags */
  //cfsetispeed(&visca->options, B2400);
  //cfsetispeed(&visca->options, B4800);
  cfsetispeed (&visca->options, B9600);
  //cfsetispeed(&visca->options, B19200);
  //cfsetispeed(&visca->options, B38400);
  //cfsetispeed(&visca->options, B57600);
  //cfsetispeed(&visca->options, B115200);
  //cfsetispeed(&visca->options, B230400);
  cfsetospeed (&visca->options, B9600);
  visca->options.c_cflag &= ~PARENB;    /* No parity  */
  visca->options.c_cflag &= ~CSTOPB;    /*            */
  visca->options.c_cflag &= ~CSIZE;     /* 8bit       */
  visca->options.c_cflag |= CS8;        /*            */
  visca->options.c_cflag &= ~CRTSCTS;   /* No hdw ctl */

  /* local flags */
  visca->options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);    /* raw input */

  /* input flags */
  /*
     visca->options.c_iflag &= ~(INPCK | ISTRIP); // no parity
     visca->options.c_iflag &= ~(IXON | IXOFF | IXANY); // no soft ctl
   */
  /* patch: bpflegin: set to 0 in order to avoid invalid pan/tilt return values */
  visca->options.c_iflag = 0;

  /* output flags */
  visca->options.c_oflag &= ~OPOST;     /* raw output */

  tcsetattr (visca->fd, TCSANOW, &visca->options);
  return TRUE;
}

static gboolean
gst_cam_controller_visca_move (GstCamControllerVisca * visca, gint x, gint y)
{
  visca_message msg = visca_message_init (0);
  visca_message reply = visca_message_init (0);
  guint pan_pos = x & 0xFFFF;
  guint tilt_pos = y & 0xFFFF;

  g_print ("visca: move(%d, %d)\n", x, y);

  visca_message_append (&msg, VISCA_COMMAND);
  visca_message_append (&msg, VISCA_CATEGORY_PAN_TILTER);
  visca_message_append (&msg, VISCA_PT_ABSOLUTE_POSITION);
  visca_message_append (&msg, visca->pan_speed);
  visca_message_append (&msg, visca->tilt_speed);
  visca_message_append (&msg, (pan_pos & 0xF000) >> 12);
  visca_message_append (&msg, (pan_pos & 0x0F00) >> 8);
  visca_message_append (&msg, (pan_pos & 0x00F0) >> 4);
  visca_message_append (&msg, (pan_pos & 0x000F) >> 0);
  visca_message_append (&msg, (tilt_pos & 0xF000) >> 12);
  visca_message_append (&msg, (tilt_pos & 0x0F00) >> 8);
  visca_message_append (&msg, (tilt_pos & 0x00F0) >> 4);
  visca_message_append (&msg, (tilt_pos & 0x000F) >> 0);
  return visca_message_send_with_reply (visca->fd, &msg, &reply);
}

/**
 *  @brief Zooming camera.
 *  @param z Zero means stop zooming,
 *         z < 0 means TELE zoom,
 *         0 < z means WIDE zoom
 *  @return TRUE if commands sent.
 */
static gboolean
gst_cam_controller_visca_zoom (GstCamControllerVisca * visca, gint z)
{
  visca_message msg = visca_message_init (0);
  visca_message reply = visca_message_init (0);

  g_print ("visca: zoom(%d)\n", z);

  visca_message_append (&msg, VISCA_COMMAND);
  visca_message_append (&msg, VISCA_CATEGORY_CAMERA1);
  visca_message_append (&msg, VISCA_ZOOM);
  if (z == 0) {
    visca_message_append (&msg, VISCA_ZOOM_STOP);
  } else if (z < 0) {
    visca_message_append (&msg,
        VISCA_ZOOM_TELE_SPEED | (visca->zoom_speed & 0x07));
    if (!visca_message_send_with_reply (visca->fd, &msg, &reply)) {
      return FALSE;
    }

    visca_message_reset (&msg);
    visca_message_reset (&reply);
    visca_message_append (&msg, VISCA_COMMAND);
    visca_message_append (&msg, VISCA_CATEGORY_CAMERA1);
    visca_message_append (&msg, VISCA_ZOOM);
    visca_message_append (&msg, VISCA_ZOOM_TELE);
  } else if (0 < z) {
    visca_message_append (&msg,
        VISCA_ZOOM_WIDE_SPEED | (visca->zoom_speed & 0x07));
    if (!visca_message_send_with_reply (visca->fd, &msg, &reply)) {
      return FALSE;
    }

    visca_message_reset (&msg);
    visca_message_reset (&reply);
    visca_message_append (&msg, VISCA_COMMAND);
    visca_message_append (&msg, VISCA_CATEGORY_CAMERA1);
    visca_message_append (&msg, VISCA_ZOOM);
    visca_message_append (&msg, VISCA_ZOOM_WIDE);
  }

  return visca_message_send_with_reply (visca->fd, &msg, &reply);
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
