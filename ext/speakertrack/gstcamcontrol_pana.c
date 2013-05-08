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

#include "gstcamcontrol_pana.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <ctype.h>

G_DEFINE_TYPE (GstCamControllerPana, gst_cam_controller_pana,
    GST_TYPE_CAM_CONTROLLER);

#define pana_message_init(a) { {0}, 0, (a) }

typedef struct _pana_message
{
  char buffer[1024];            // 64 bytes for one command max
  int len;
} pana_message;

static void
pana_message_dump (const pana_message * msg, const gchar * tag)
{
  int n;

  g_print ("%s: ", tag);
  if (msg->len <= 0) {
    g_print ("(empty message)\n");
    return;
  }

  for (n = 0; n < msg->len; ++n) {
    int c = msg->buffer[n];
    if (c == '\r')
      g_print ("\\r");
    else if (isprint (c))
      g_print ("%c", c);
    else
      g_print ("\\x%02x", c);
  }
  g_print ("\n");
}

static void
pana_message_append (pana_message * msg, char c)
{
  if (0 < msg->len && msg->len < sizeof (msg->buffer) - 1) {
    msg->buffer[msg->len++] = c;
    msg->buffer[msg->len] = 0;
  }
}

static void
pana_message_reset (pana_message * msg)
{
  msg->len = 0;
  bzero (msg->buffer, sizeof (msg->buffer));
}

static gboolean
pana_message_send (int fd, const pana_message * msg)
{
  if (msg->len <= 0) {
    g_print ("send: (empty message)\n");
    return FALSE;
  }

  if (write (fd, msg->buffer, msg->len) < msg->len) {
    perror ("write");
    return FALSE;
  }

  pana_message_dump (msg, "send");
  return TRUE;
}

static gboolean
pana_message_reply (int fd, pana_message * reply, char terminator)
{
  int available_bytes = 0, n = 0;

  do {
    ioctl (fd, FIONREAD, &available_bytes);
    usleep (500);
  } while (available_bytes == 0);

  do {
    if (read (fd, &reply->buffer[n], 1) != 1) {
      perror ("read");
      return FALSE;
    }
    if (reply->buffer[n] == terminator /*PANA_TERMINATOR */ ) {
      break;
    }
    n += 1;
    usleep (1);
  } while (n < sizeof (reply->buffer) - 1);

  reply->len = n;

  pana_message_dump (reply, "read");
  return TRUE;
}

static gboolean
pana_message_send_with_reply (int fd, const pana_message * msg,
    pana_message * reply)
{
  if (!pana_message_send (fd, msg)) {
    g_print ("send failed\n");
    return FALSE;
  }
  return pana_message_reply (fd, reply, '\x03');
}

static void
gst_cam_controller_pana_init (GstCamControllerPana * pana)
{
}

static void
gst_cam_controller_pana_finalize (GstCamControllerPana * pana)
{
  G_OBJECT_CLASS (gst_cam_controller_pana_parent_class)
      ->finalize (G_OBJECT (pana));
}

static void
gst_cam_controller_pana_close (GstCamControllerPana * pana)
{
  g_print ("pana: close()\n");

  if (0 < pana->fd) {
    close (pana->fd);
    pana->fd = -1;

    g_free ((void *) pana->device);
    pana->device = NULL;

    bzero (&pana->options, sizeof (pana->options));
  }
}

static gboolean
gst_cam_controller_pana_open (GstCamControllerPana * pana, const char *dev)
{
  pana_message msg = { {0}, 0 };
  pana_message reply = { {0}, 0 };
  g_print ("pana: open(%s)\n", dev);

  if (0 < pana->fd) {
    gst_cam_controller_pana_close (pana);
  }

  g_free ((void *) pana->device);
  pana->device = g_strdup (dev);
  pana->fd = open (dev, O_RDWR | O_NDELAY | O_NOCTTY);

  if (pana->fd == -1) {
    g_print ("pana: open(%s) error: %s\n", dev, strerror (errno));
    return FALSE;
  }
  //fcntl (pana->fd, F_SETFL, 0);
  tcgetattr (pana->fd, &pana->options);
  cfsetispeed (&pana->options, B9600);
  cfsetospeed (&pana->options, B9600);
  pana->options.c_cflag &= ~PARENB;     /* No parity  */
  pana->options.c_cflag &= ~CSTOPB;     /*            */
  pana->options.c_cflag &= ~CSIZE;      /* 8bit       */
  pana->options.c_cflag |= CS8; /*            */
  pana->options.c_cflag &= ~CRTSCTS;    /* No hdw ctl */

  pana->options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);     /* raw input */

  /*
     pana->options.c_iflag &= ~(INPCK | ISTRIP); // no parity
     pana->options.c_iflag &= ~(IXON | IXOFF | IXANY); // no soft ctl
   */
  pana->options.c_iflag = 0;

  pana->options.c_oflag &= ~OPOST;      /* raw output */

  tcsetattr (pana->fd, TCSANOW, &pana->options);

  // initialization commands
  pana_message_reset (&msg);
  pana_message_append (&msg, '\x02');
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'V');
  pana_message_append (&msg, '?');
  pana_message_append (&msg, '\x03');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    g_print ("send init commands failed, close %d\n", pana->fd);
    gst_cam_controller_pana_close (pana);
    return FALSE;
  }

  pana_message_reset (&msg);
  pana_message_append (&msg, '\x02');
  pana_message_append (&msg, 'O');
  pana_message_append (&msg, 'S');
  pana_message_append (&msg, 'A');
  pana_message_append (&msg, ':');
  pana_message_append (&msg, '8');
  pana_message_append (&msg, '7');
  pana_message_append (&msg, ':');
  pana_message_append (&msg, '2');
  pana_message_append (&msg, '\x03');
  pana_message_append (&msg, '\x02');
  pana_message_append (&msg, 'Q');
  pana_message_append (&msg, 'S');
  pana_message_append (&msg, 'A');
  pana_message_append (&msg, ':');
  pana_message_append (&msg, '8');
  pana_message_append (&msg, '7');
  pana_message_append (&msg, '\x03');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    g_print ("send init commands failed, close %d", pana->fd);
    gst_cam_controller_pana_close (pana);
    return FALSE;
  }

  pana_message_reset (&reply);
  if (!pana_message_reply (pana->fd, &reply, '\x03')) {
    g_print ("read init reply failed, close %d\n", pana->fd);
    gst_cam_controller_pana_close (pana);
    return FALSE;
  }
  g_print ("open: %s\n", reply.buffer);

  pana_message_reset (&reply);
  if (!pana_message_reply (pana->fd, &reply, '\x03')) {
    g_print ("read init reply failed, close %d\n", pana->fd);
    gst_cam_controller_pana_close (pana);
    return FALSE;
  }
  g_print ("open: %s\n", reply.buffer);
  return TRUE;
}

static gboolean
gst_cam_controller_pana_pan (GstCamControllerPana * pana, gint speed, gint v)
{
  pana_message msg = { {0}, 0 };        //, reply;
  char buf[10];

  g_print ("pana: pan(%d, %d)\n", speed, v);

  sprintf (buf, "%02d", v);
  pana_message_reset (&msg);
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'P');
  pana_message_append (&msg, buf[0]);
  pana_message_append (&msg, buf[1]);
  pana_message_append (&msg, '\r');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    return FALSE;
  }

  /*
     sprintf (buf, "%02d%02d", v, 0);
     pana_message_reset (&msg);
     pana_message_append (&msg, '#');
     pana_message_append (&msg, 'U');
     pana_message_append (&msg, buf[0]);
     pana_message_append (&msg, buf[1]);
     pana_message_append (&msg, buf[2]);
     pana_message_append (&msg, buf[3]);
     pana_message_append (&msg, '\r');
   */
  //return pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ );
  return TRUE;
}

static gboolean
gst_cam_controller_pana_tilt (GstCamControllerPana * pana, gint speed, gint v)
{
  pana_message msg = { {0}, 0 };        //, reply;
  char buf[10];

  g_print ("pana: tilt(%d, %d)\n", speed, v);

  sprintf (buf, "%02d", v);
  pana_message_reset (&msg);
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'T');
  pana_message_append (&msg, buf[0]);
  pana_message_append (&msg, buf[1]);
  pana_message_append (&msg, '\r');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    return FALSE;
  }

  /*
     sprintf (buf, "%02d%02d", 0, v);
     pana_message_reset (&msg);
     pana_message_append (&msg, '#');
     pana_message_append (&msg, 'U');
     pana_message_append (&msg, buf[0]);
     pana_message_append (&msg, buf[1]);
     pana_message_append (&msg, buf[2]);
     pana_message_append (&msg, buf[3]);
     pana_message_append (&msg, '\r');
   */
  //return pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ );
  return TRUE;
}

static gboolean
gst_cam_controller_pana_move (GstCamControllerPana * pana, gint speed, gint x,
    gint y)
{
  pana_message msg = { {0}, 0 };        //, reply;
  char buf[10];

  g_print ("pana: move(%d, %d, %d)\n", speed, x, y);

  sprintf (buf, "%02d", x);
  pana_message_reset (&msg);
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'P');
  pana_message_append (&msg, buf[0]);
  pana_message_append (&msg, buf[1]);
  pana_message_append (&msg, '\r');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    return FALSE;
  }

  sprintf (buf, "%02d", y);
  pana_message_reset (&msg);
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'T');
  pana_message_append (&msg, buf[0]);
  pana_message_append (&msg, buf[1]);
  pana_message_append (&msg, '\r');
  if (!pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ )) {
    return FALSE;
  }

  /*
     sprintf (buf, "%02d%02d", x, y);
     pana_message_reset (&msg);
     pana_message_append (&msg, '#');
     pana_message_append (&msg, 'U');
     pana_message_append (&msg, buf[0]);
     pana_message_append (&msg, buf[1]);
     pana_message_append (&msg, buf[2]);
     pana_message_append (&msg, buf[3]);
     pana_message_append (&msg, '\r');
   */
  //return pana_message_send /*_with_reply*/ (pana->fd, &msg /*, &reply */ );
  return TRUE;
}

static gboolean
gst_cam_controller_pana_zoom (GstCamControllerPana * pana, gint speed, gint z)
{
  pana_message msg = { {0}, 0 };
  pana_message reply = { {0}, 0 };
  char buf[10];

  g_print ("pana: zoom(%d)\n", z);

  sprintf (buf, "%02d", speed);
  pana_message_reset (&msg);
  pana_message_append (&msg, '#');
  pana_message_append (&msg, 'Z');
  pana_message_append (&msg, buf[0]);
  pana_message_append (&msg, buf[1]);
  pana_message_append (&msg, '\r');

  if (z == 0) {
  } else if (z < 0) {
    sprintf (buf, "%02d", -z);
    pana_message_reset (&msg);
    pana_message_append (&msg, '#');
    pana_message_append (&msg, 'A');
    pana_message_append (&msg, 'X');
    pana_message_append (&msg, 'Z');
    pana_message_append (&msg, buf[0]);
    pana_message_append (&msg, buf[1]);
    pana_message_append (&msg, '\r');
  } else if (0 < z) {
    sprintf (buf, "%02d", z);
    pana_message_reset (&msg);
    pana_message_append (&msg, '#');
    pana_message_append (&msg, 'A');
    pana_message_append (&msg, 'Y');
    pana_message_append (&msg, 'Z');
    pana_message_append (&msg, buf[0]);
    pana_message_append (&msg, buf[1]);
    pana_message_append (&msg, '\r');
  }

  return pana_message_send_with_reply (pana->fd, &msg, &reply);
}

static void
gst_cam_controller_pana_class_init (GstCamControllerPanaClass * panaclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (panaclass);
  GstCamControllerClass *camctl_class = GST_CAM_CONTROLLER_CLASS (panaclass);
  object_class->finalize = GST_DEBUG_FUNCPTR (
      (GObjectFinalizeFunc) gst_cam_controller_pana_finalize);
  camctl_class->open = (GstCamControllerOpenFunc) gst_cam_controller_pana_open;
  camctl_class->close =
      (GstCamControllerCloseFunc) gst_cam_controller_pana_close;
  camctl_class->pan = (GstCamControllerPanFunc) gst_cam_controller_pana_pan;
  camctl_class->tilt = (GstCamControllerTiltFunc) gst_cam_controller_pana_tilt;
  camctl_class->move = (GstCamControllerMoveFunc) gst_cam_controller_pana_move;
  camctl_class->zoom = (GstCamControllerZoomFunc) gst_cam_controller_pana_zoom;
}
