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

#include "gstcamcontrol_canon.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

G_DEFINE_TYPE (GstCamControllerCanon, gst_cam_controller_canon,
    GST_TYPE_CAM_CONTROLLER);

// @fixme: Better using libvisca, but I can't see this is working with
//         my camera.

//
#define CANON_COMMAND                    0x01
#define CANON_CATEGORY_CAMERA1           0x04
#define CANON_CATEGORY_CAMERA2           0x07
#define CANON_CATEGORY_PAN_TILTER        0x06
#define CANON_TERMINATOR                 0xEF

//
#define CANON_ZOOM                       0x07
#define CANON_ZOOM_STOP                  0x00
#define CANON_ZOOM_TELE                  0x02
#define CANON_ZOOM_WIDE                  0x03
#define CANON_ZOOM_TELE_SPEED            0x20
#define CANON_ZOOM_WIDE_SPEED            0x30
#define CANON_PT_ABSOLUTE_POSITION       0x02
#define CANON_PT_RELATIVE_POSITION       0x03
#define CANON_PT_HOME                    0x04
#define CANON_PT_RESET                   0x05
#define CANON_PT_LIMITSET                0x07

//#define canon_message_init(a) { {0}, 0, (a) }
#define canon_message_init(a) { {0}, 0 }

typedef struct _canon_message
{
  char buffer[256];             // 32 bytes for one command max
  int len;
} canon_message;

static void
canon_message_append (canon_message * msg, char c)
{
  msg->buffer[msg->len++] = c;
}

static void
canon_message_reset (canon_message * msg)
{
  msg->len = 0;
  bzero (msg->buffer, sizeof (msg->buffer));
}

static void
canon_message_dump (const canon_message * msg, const gchar * tag)
{
  int n;

  g_print ("%s: ", tag);
  if (msg->len <= 0) {
    g_print ("(empty message)\n");
    return;
  }

  for (n = 0; n < msg->len; ++n) {
    int c = msg->buffer[n];
    g_print ("\\x%02X", (unsigned char) c);
  }
  g_print ("\n");
}

static gboolean
canon_message_send (int fd, const canon_message * msg)
{
  int len = 1 + msg->len + 1, n;
  char b[256];
  if (msg->len <= 0 || sizeof (b) <= len) {
    g_print ("canon_message_send: %d\n", msg->len);
    return FALSE;
  }

  memcpy (&b[1], msg->buffer, msg->len);
  b[1 + msg->len] = CANON_TERMINATOR;

  n = write (fd, msg->buffer, msg->len);
  if (n < msg->len) {
    g_print ("wrote: %d != %d\n", n, msg->len);
    return FALSE;
  }

  canon_message_dump (msg, "sent");
  //g_print ("wrote: %d, %d\n", n, msg->len);
  return TRUE;
}

static int
canon_message_read_some (int fd, char *out, int sz)
{
  int n = 0, num = 0, available_bytes = 0;
  do {
    ioctl (fd, FIONREAD, &available_bytes);
    usleep (100);
  } while (available_bytes == 0);

  //g_print ("available: %d bytes\n", available_bytes);

  do {
    n = read (fd, out, sz);
    if (n < 0) {
      break;
    }
    num += n;
    usleep (1);
  } while (num < available_bytes);

  return num;
}

static gboolean
canon_message_read (int fd, canon_message * reply)
{
  int n = 0;
  gboolean done = FALSE;
  canon_message_reset (reply);

  do {
    n = sizeof (reply->buffer) - reply->len;
    n = canon_message_read_some (fd, &reply->buffer[reply->len], n);
    if (n <= 0) {
      g_print ("read: %d\n", n);
      break;
    }

    reply->len += n;
    //canon_message_dump (reply, "read");

    for (n = 0; n < reply->len; ++n) {
      //g_print ("read: 0x%0x, %d\n", (int) reply->buffer[n] & 0xFF, n);
      if ((reply->buffer[n] & 0xFF) == CANON_TERMINATOR) {
        done = TRUE;
        break;
      }
    }

    usleep (10);
  } while (!done);

  canon_message_dump (reply, "read");
  return TRUE;
}

static gboolean
canon_message_send_with_reply (int fd, const canon_message * msg,
    canon_message * reply)
{
  if (!canon_message_send (fd, msg)) {
    return FALSE;
  }
  return canon_message_read (fd, reply);
}

static void
gst_cam_controller_canon_init (GstCamControllerCanon * canon)
{
  canon->fd = -1;
  canon->zoom = 0.5;
  canon->zoom_speed = 1.0;
  canon->pan = 0;               //0.5;
  canon->pan_speed = 50;        //1.0;
  canon->base.pan_min = -175;
  canon->base.pan_max = 175;
  canon->tilt = 0;              //0.5;
  canon->tilt_speed = 50;       //1.0;
  canon->base.tilt_min = -55;   //-20;
  canon->base.tilt_max = 90;    //90;

  bzero (&canon->options, sizeof (canon->options));
}

static void
gst_cam_controller_canon_finalize (GstCamControllerCanon * canon)
{
  G_OBJECT_CLASS (gst_cam_controller_canon_parent_class)
      ->finalize (G_OBJECT (canon));
}

static void
gst_cam_controller_canon_close (GstCamControllerCanon * canon)
{
  g_print ("canon: close()\n");

  if (0 < canon->fd) {
    close (canon->fd);
    canon->fd = -1;

    g_free ((void *) canon->device);
    canon->device = NULL;

    bzero (&canon->options, sizeof (canon->options));
  }
}

static gboolean
gst_cam_controller_canon_open (GstCamControllerCanon * canon, const char *dev)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);

  g_print ("canon: open(%s)\n", dev);

  if (0 < canon->fd) {
    gst_cam_controller_canon_close (canon);
  }

  g_free ((void *) canon->device);
  canon->device = g_strdup (dev);
  canon->fd = open (dev, O_RDWR | O_NONBLOCK /*O_NDELAY */  | O_NOCTTY);

  if (canon->fd == -1) {
    g_print ("canon: open(%s) error: %s\n", dev, strerror (errno));
    return FALSE;
  }

  fcntl (canon->fd, F_SETFL, O_RDWR | O_NONBLOCK);
  //tcgetattr (canon->fd, &canon->options);

  /**
 RS-232C Conformity Connector & Pin assignment of connector are referred to 2.2 
 Transmission Mode : Half Duplex (Full duplex for notification) 
 Transfer Speed : 4800, 9600, 14400, 19200bps. (selected through menu window) 
 Data Bit : 8 bit 
 Parity : None 
 Stop Bit : 1 bit or 2 bit (selected through menu window) 
 Handshake : RTS/CTS Control 
   */
  canon->options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
  canon->options.c_lflag = 0;
  canon->options.c_iflag = IGNPAR;
  canon->options.c_oflag = 0;

  //cfmakeraw (&canon->options);

  //cfsetispeed (&canon->options, B2400);
  //cfsetispeed (&canon->options, B4800);
  cfsetispeed (&canon->options, B9600);
  //cfsetispeed (&canon->options, B19200);
  //cfsetispeed (&canon->options, B38400);
  //cfsetispeed (&canon->options, B57600);
  //cfsetispeed (&canon->options, B115200);
  //cfsetispeed (&canon->options, B230400);

  //cfsetospeed (&canon->options, B2400);
  //cfsetospeed (&canon->options, B4800);
  cfsetospeed (&canon->options, B9600);
  //cfsetospeed (&canon->options, B19200);
  //cfsetospeed (&canon->options, B38400);
  //cfsetospeed (&canon->options, B57600);
  //cfsetospeed (&canon->options, B115200);
  //cfsetospeed (&canon->options, B230400);

  canon->options.c_cc[VMIN] = 1;

  tcsetattr (canon->fd, TCSANOW, &canon->options);

#if 0
  tcgetattr (canon->fd, &canon->options);
  g_print ("iflag: %d\n", canon->options.c_iflag);
  g_print ("oflag: %d\n", canon->options.c_oflag);
  g_print ("cflag: %d\n", canon->options.c_cflag);
  g_print ("lflag: %d\n", canon->options.c_lflag);
  g_print ("ispeed: %d\n", cfgetispeed (&canon->options));
  g_print ("ospeed: %d\n", cfgetospeed (&canon->options));
  g_print ("F_GETFL: %d, 0x%x\n", fcntl (canon->fd, F_GETFL), fcntl (canon->fd,
          F_GETFL));
  for (num = 0; num < NCCS; ++num) {
    g_print ("cc[%d]=0x%02x, ", num, canon->options.c_cc[num]);
  }
  g_print ("\n");
#endif

  //////////////////////////////////////////////////

  // 0xff 0x30 0x30 0x00 0x8f 0x30 0xef
  // 0xff 0x30 0x30 0x00 0x8f 0x31 0xef
  // ff 30 31 00 87 ef
  // 0xff 0x30 0x31 0x00 0xa0 0x31 0xef
  // 

  // "Probe device chain"
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x8F);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x8F);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Turn off command finish notification"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x94);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Camera off"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xA0);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  sleep (1);

  // "Camera on"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xA0);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Camera Reset"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xAA);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Turn on command finish notification"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x94);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Get device info"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x87);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // Pan Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x50);
  canon_message_append (&msg, 0x33);
  canon_message_append (&msg, 0x32);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // Tilt Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x51);
  canon_message_append (&msg, 0x33);
  canon_message_append (&msg, 0x32);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Center"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x58);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  ////////////////////////
#if 0
  // "Set pan speed"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x50);
  canon_message_append (&msg, '3');
  canon_message_append (&msg, '0');
  canon_message_append (&msg, '0');
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Pan left"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x60);
  canon_message_append (&msg, '2');
  canon_message_append (&msg, '0');
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  sleep (1.0);

  // "Pan left stop"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x53);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // "Pan right till can't any more"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x60);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
#endif

  return TRUE;
}

static gboolean
gst_cam_controller_canon_move (GstCamControllerCanon * canon, double vxspeed,
    double vx, double vyspeed, double vy)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  const double panmax = 0x8379;
  const double panmin = 0x7C87;
  const double tiltmax = 0x810B;
  const double tiltmin = 0x7EF5;
  const double dps = 0.1125;    // 0.1125 degrees/second
  guint xspeed;                 // = 0x8 + (canon->pan_speed = vxspeed) * 0x320;
  guint yspeed;                 // = 0x8 + (canon->tilt_speed = vyspeed) * 0x26E;
  guint x;                      // = 0x7C87 + (canon->pan = vx) * (0x8379 - 0x7C87);     // 7C87~8379h
  guint y;                      // = 0x7EF5 + (canon->tilt = vy) * (0x810B - 0x7EF5);    // 7eF5~810Bh
  char bufsx[10] = { 0 };
  char bufsy[10] = { 0 };
  char bufx[10] = { 0 };
  char bufy[10] = { 0 };
  double lx = panmax - panmin;
  double ly = tiltmax - tiltmin;
  double px = lx / (canon->base.pan_max - canon->base.pan_min);
  double py = ly / (canon->base.tilt_max - canon->base.tilt_min);

  canon->pan = vx;
  canon->tilt = vy;
  if (canon->pan < canon->base.pan_min)
    canon->pan = canon->base.pan_min;
  if (canon->base.pan_max < canon->pan)
    canon->pan = canon->base.pan_max;
  if (canon->tilt < canon->base.tilt_min)
    canon->tilt = canon->base.tilt_min;
  if (canon->base.tilt_max < canon->tilt)
    canon->tilt = canon->base.tilt_max;
  xspeed = 0x8 + (canon->pan_speed = vxspeed) / dps;
  yspeed = 0x8 + (canon->tilt_speed = vyspeed) / dps;
  x = panmin + lx * 0.5 + canon->pan * px + 0.5;
  y = tiltmin + ly * 0.5 + canon->tilt * py + 0.5;

  //008~320h
  if (xspeed < 0x8)
    xspeed = 0x8;
  if (0x320 < xspeed)
    xspeed = 0x320;

  //008~26Eh
  if (yspeed < 0x8)
    yspeed = 0x8;
  if (0x26E < yspeed)
    yspeed = 0x26E;

  //7C87~8379h
  if (x < 0x7C87)
    x = 0x7C87;
  if (0x8379 < x)
    x = 0x8379;

  //7eF5~810B
  if (y < 0x7EF5)
    y = 0x7EF5;
  if (0x810B < y)
    y = 0x810B;

  sprintf (bufsx, "%03X", xspeed);
  sprintf (bufsy, "%03X", yspeed);
  sprintf (bufx, "%04X", x & 0xFFFF);
  sprintf (bufy, "%04X", y & 0xFFFF);

  g_print
      ("canon: move(%f, %f, %f, %f) -> (pan=(0x%s, 0x%s), tilt=(0x%s, 0x%s))\n",
      vxspeed, vx, vyspeed, vy, bufsx, bufx, bufsy, bufy);

  // Pan Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x50);
  canon_message_append (&msg, bufsx[0]);
  canon_message_append (&msg, bufsx[1]);
  canon_message_append (&msg, bufsx[2]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // Tilt Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x51);
  canon_message_append (&msg, bufsy[0]);
  canon_message_append (&msg, bufsy[1]);
  canon_message_append (&msg, bufsy[2]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // Stop Pan/Tilt
  /*
     canon_message_reset (&msg);
     canon_message_append (&msg, 0xFF);
     canon_message_append (&msg, 0x30);
     canon_message_append (&msg, 0x31);
     canon_message_append (&msg, 0x00);
     canon_message_append (&msg, 0x60);
     canon_message_append (&msg, 0x30);    // pan: 0, 1, 2
     canon_message_append (&msg, 0x30);    // tilt: 0, 1, 2
     canon_message_append (&msg, 0xEF);
     canon_message_send_with_reply (canon->fd, &msg, &reply);
   */

  // Angle Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x62);
  canon_message_append (&msg, bufx[0]);
  canon_message_append (&msg, bufx[1]);
  canon_message_append (&msg, bufx[2]);
  canon_message_append (&msg, bufx[3]);
  canon_message_append (&msg, bufy[0]);
  canon_message_append (&msg, bufy[1]);
  canon_message_append (&msg, bufy[2]);
  canon_message_append (&msg, bufy[3]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  return TRUE;
}

static gboolean
gst_cam_controller_canon_pan (GstCamControllerCanon * canon, double speed,
    double v)
{
  return gst_cam_controller_canon_move (canon, speed, v, canon->tilt_speed,
      canon->tilt);

#if 0
  // Start Pan running
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x60);
  if (v < 0.5)
    canon_message_append (&msg, 0x31);  // pan: 1, 2
  else
    canon_message_append (&msg, 0x32);  // pan: 1, 2
  canon_message_append (&msg, 0x30);    // tilt: 1, 2
  canon_message_append (&msg, 0xEF);
//#else
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x53);
  if (v < 0.5)
    canon_message_append (&msg, 0x31);  // pan: 1, 2
  else
    canon_message_append (&msg, 0x32);  // pan: 1, 2
  canon_message_append (&msg, 0xEF);

  canon_message_send_with_reply (canon->fd, &msg, &reply);
  return TRUE;
#endif
}

static gboolean
gst_cam_controller_canon_tilt (GstCamControllerCanon * canon, double speed,
    double v)
{
  return gst_cam_controller_canon_move (canon, canon->pan_speed, canon->pan,
      speed, v);

#if 0
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  guint uspeed = 0x8 + speed * 0x26E;
  gint y = v * 267;             // 7eF5~810Bh
  char buf[10] = { 0 };
  char bufy[10] = { 0 };

  //008~26Eh
  if (uspeed < 0x8)
    uspeed = 0x8;
  if (0x26E < uspeed)
    uspeed = 0x26E;

  //7eF5~810B
  if (y < 0x7EF5)
    y = 0x7EF5;
  if (0x810B < y)
    y = 0x810B;

  sprintf (buf, "%3X", uspeed);
  sprintf (bufy, "%4X", y & 0xFFFF);

  g_print ("canon: tilt(%f, %f) (%s)\n", speed, v, buf);

  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x51);
  canon_message_append (&msg, buf[0]);
  canon_message_append (&msg, buf[1]);
  canon_message_append (&msg, buf[2]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x62);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, bufy[0]);
  canon_message_append (&msg, bufy[1]);
  canon_message_append (&msg, bufy[2]);
  canon_message_append (&msg, bufy[3]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  return TRUE;
#endif
}

/**
 *  @brief Zooming camera.
 *  @param z Zero means stop zooming,
 *         z < 0 means TELE zoom,
 *         0 < z means WIDE zoom
 *  @return TRUE if commands sent.
 */
static gboolean
gst_cam_controller_canon_zoom (GstCamControllerCanon * canon, double vzspeed,
    double vz)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  guint zspeed = (canon->zoom_speed = vzspeed) * 7;
  guint z = (canon->zoom = vz) * 0x07A8;        // 0000~07A8h
  char bufs[10] = { 0 };
  char bufz[10] = { 0 };

  //0~7
  if (zspeed < 0)
    zspeed = 0;
  if (7 < zspeed)
    zspeed = 7;

  //0~07A8h
  if (z < 0)
    z = 0;
  if (0x07A8 < z)
    z = 0x07A8;

  sprintf (bufs, "%1X", zspeed);
  sprintf (bufz, "%04X", z & 0xFFFF);

  g_print
      ("canon: zoom(%f, %f) -> (zoom=(0x%s, 0x%s))\n", vzspeed, vz, bufs, bufz);

  // Zoom Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xB4);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, bufs[0]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  // Angle Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xB3);
  canon_message_append (&msg, bufz[0]);
  canon_message_append (&msg, bufz[1]);
  canon_message_append (&msg, bufz[2]);
  canon_message_append (&msg, bufz[3]);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  return TRUE;
}

static void
gst_cam_controller_canon_class_init (GstCamControllerCanonClass * canonclass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (canonclass);
  GstCamControllerClass *camctl_class = GST_CAM_CONTROLLER_CLASS (canonclass);
  object_class->finalize = GST_DEBUG_FUNCPTR (
      (GObjectFinalizeFunc) gst_cam_controller_canon_finalize);
  camctl_class->open = (GstCamControllerOpenFunc) gst_cam_controller_canon_open;
  camctl_class->close =
      (GstCamControllerCloseFunc) gst_cam_controller_canon_close;
  camctl_class->pan = (GstCamControllerPanFunc) gst_cam_controller_canon_pan;
  camctl_class->tilt = (GstCamControllerTiltFunc) gst_cam_controller_canon_tilt;
  camctl_class->move = (GstCamControllerMoveFunc) gst_cam_controller_canon_move;
  camctl_class->zoom = (GstCamControllerZoomFunc) gst_cam_controller_canon_zoom;
}
