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
#include <ctype.h>
//#include <glib/gstring.h>
#include <glib.h>

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

const double dps = 0.1125;      // 0.1125 degrees/second

struct range_t
{
  const gchar *name;
  double pan_min, pan_max;
  double tilt_min, tilt_max;
  guint u_pan_min, u_pan_max;
  guint u_tilt_min, u_tilt_max;
  guint u_zoom_min, u_zoom_max;
  guint u_pan_speed_min, u_pan_speed_max;
  guint u_tilt_speed_min, u_tilt_speed_max;
  guint u_zoom_speed_min, u_zoom_speed_max;
};
static const struct range_t range_table[] = {
  {"C50i", -100, 100, -59, 60, 0x7C87, 0x8379, 0x7EF5, 0x810B, 0, 0x07A8, 0x8,
      0x320, 0x8, 0x26E, 0, 7},
  {NULL, 0, 0, 0, 0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x0},
};

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
  canon->tilt = 0;              //0.5;
  canon->tilt_speed = 50;       //1.0;
  canon->base.pan_min = -175;
  canon->base.pan_max = 175;
  canon->base.tilt_min = -55;   //-20;
  canon->base.tilt_max = 90;    //90;
  canon->base.device_info = NULL;

  bzero (&canon->options, sizeof (canon->options));
}

static void
gst_cam_controller_canon_finalize (GstCamControllerCanon * canon)
{
  G_OBJECT_CLASS (gst_cam_controller_canon_parent_class)
      ->finalize (G_OBJECT (canon));
  g_free (canon->base.device_info);
  g_free (canon->range);
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
  const char *valueFmt = "\xFE\x30\x31\x30\x30%4X\xEF";
  const char *valueFmt2 = "\xFE\x30\x31\x30\x30%3X\xEF";
  guint m, scanMin = 0, scanMax = 0;

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

  // "Turn on/off command finish notification"
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x94);
  canon_message_append (&msg, 0x30);
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
  if (5 < reply.len) {
    int n = 0;
    reply.buffer[reply.len - 2] = 0;
    canon->base.device_info = &reply.buffer[5];
    canon->base.device_info = g_strdup (canon->base.device_info);
    g_print ("device: %s\n", canon->base.device_info);
    for (n = 0;; ++n) {
      const struct range_t *range = &range_table[n];
      if (range->name == NULL)
        break;
      if (g_str_has_prefix (canon->base.device_info, range->name)
          /*|| g_str_has_prefix (range->name, canon->base.device_info) */
          ) {
        g_print ("device: found range: %s\n", range->name);
        canon->range = g_new0 (struct range_t, 1);
        *canon->range = *range;
        break;
      }
    }
  }

  if (canon->range == NULL) {
    int n;
    for (n = 0;; ++n) {
      const struct range_t *range = &range_table[n];
      if (range->name == NULL) {
        canon->range = g_new0 (struct range_t, 1);
        *canon->range = *range;
        break;
      }
    }
  }
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

  // Pan Min
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x5C);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &scanMin) == 1) {
    g_print ("Pan-Min: %d (%04x)\n", scanMin, scanMin);
  }
  // Pan Max
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x5C);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &scanMax) == 1) {
    g_print ("Pan-Max: %d (%04x)\n", scanMax, scanMax);
  }

  if (scanMin != 0 && scanMax != 0) {
    canon->range->u_pan_min = scanMin;
    canon->range->u_pan_max = scanMax;
    m = scanMin + (scanMax - scanMin) / 2;
    canon->range->pan_min = -(double) (m - scanMin) * 0.1125;
    canon->range->pan_max = (double) (scanMax - m) * 0.1125;
    g_print ("range: [%f, %f]\n", canon->range->pan_min, canon->range->pan_max);
    scanMin = scanMax = 0;
  }
  // Tilt Min
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x5C);
  canon_message_append (&msg, 0x32);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &scanMin) == 1) {
    g_print ("Tilt-Min: %d (%04x)\n", scanMin, scanMin);
  }
  // Tilt Max
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x5C);
  canon_message_append (&msg, 0x33);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &scanMax) == 1) {
    g_print ("Tilt-Max: %d (%04x)\n", scanMax, scanMax);
  }

  if (scanMin != 0 && scanMax != 0) {
    canon->range->u_tilt_min = scanMin;
    canon->range->u_tilt_max = scanMax;
    m = scanMin + (scanMax - scanMin) / 2;
    canon->range->tilt_min = -(double) (m - scanMin) * 0.1125;
    canon->range->tilt_max = (double) (scanMax - m) * 0.1125;
    g_print ("range: [%f, %f]\n", canon->range->tilt_min,
        canon->range->tilt_max);
    scanMin = scanMax = 0;
  }
  // Zoom Max
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0xB4);
  canon_message_append (&msg, 0x33);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &scanMax) == 1) {
    g_print ("Zoom-Max: %d (%04x)\n", scanMax, scanMax);
  }

  if (scanMax != 0) {
    canon->range->u_zoom_max = scanMax;
    scanMin = scanMax = 0;
  }
  // Pan Speed Min
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x59);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt2, &scanMin) == 1) {
    g_print ("Pan-Speed-Min: %d (%03x)\n", scanMin, scanMin);
  }
  // Pan Speed Max
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x59);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt2, &scanMax) == 1) {
    g_print ("Pan-Speed-Max: %d (%03x)\n", scanMax, scanMax);
  }

  if (scanMin != 0 && scanMax != 0) {
    canon->range->u_pan_speed_min = scanMin;
    canon->range->u_pan_speed_max = scanMax;
    g_print ("pan-speed-range: [%d, %d]\n", canon->range->u_pan_speed_min,
        canon->range->u_pan_speed_max);
    scanMin = scanMax = 0;
  }
  // Tilt Speed Min
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x59);
  canon_message_append (&msg, 0x32);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt2, &scanMin) == 1) {
    g_print ("Tilt-Speed-Min: %d (%03x)\n", scanMin, scanMin);
  }
  // Tilt Speed Max
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x59);
  canon_message_append (&msg, 0x33);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt2, &scanMax) == 1) {
    g_print ("Tilt-Speed-Max: %d (%03x)\n", scanMax, scanMax);
  }

  if (scanMin != 0 && scanMax != 0) {
    canon->range->u_tilt_speed_min = scanMin;
    canon->range->u_tilt_speed_max = scanMax;
    g_print ("tilt-speed-range: [%d, %d]\n", canon->range->u_pan_speed_min,
        canon->range->u_pan_speed_max);
    scanMin = scanMax = 0;
  }

  canon->base.pan_min = canon->range->pan_min;
  canon->base.pan_max = canon->range->pan_max;
  canon->base.pan_speed_min = (double) canon->range->u_pan_speed_min * dps;
  canon->base.pan_speed_max = (double) canon->range->u_pan_speed_max * dps;
  canon->base.tilt_min = canon->range->tilt_min;
  canon->base.tilt_max = canon->range->tilt_max;
  canon->base.tilt_speed_min = (double) canon->range->u_tilt_speed_min * dps;
  canon->base.tilt_speed_max = (double) canon->range->u_tilt_speed_max * dps;

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
gst_cam_controller_canon_query (GstCamControllerCanon * canon,
    double *pan, double *tilt)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  const char *valueFmt = "\xFE\x30\x31\x30\x30%4X%4X\xEF";
  double ox = 0, oy = 0;
  guint x = 0, y = 0;

  // Pan Speed Assignment
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x63);
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);

  reply.buffer[reply.len] = 0;
  if (sscanf (reply.buffer, valueFmt, &x, &y) == 2) {
    double lx = canon->range->u_pan_max - canon->range->u_pan_min;
    double ly = canon->range->u_tilt_max - canon->range->u_tilt_min;
    double px = lx / (canon->base.pan_max - canon->base.pan_min);
    double py = ly / (canon->base.tilt_max - canon->base.tilt_min);
    //ox = canon->range->u_pan_min + lx * 0.5 + canon->pan * px + 0.5;
    //oy = canon->range->u_tilt_min + ly * 0.5 + canon->tilt * py + 0.5;
    ox = (double) (x - canon->range->u_pan_min - lx * 0.5) / px;
    oy = (double) (y - canon->range->u_tilt_min - ly * 0.5) / py;
    g_print ("canon: query(%d, %d) (%x, %x) -> (%f, %f)\n", x, y, x, y, ox, oy);
    *pan = ox, *tilt = oy;
  }

  return TRUE;
}

static gboolean
gst_cam_controller_canon_run (GstCamControllerCanon * canon,
    double vxspeed, double vyspeed, int dir, gboolean start)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  guint xspeed;                 // = 0x8 + (canon->pan_speed = vxspeed) * 0x320;
  guint yspeed;                 // = 0x8 + (canon->tilt_speed = vyspeed) * 0x26E;
  char bufsx[10] = { 0 };
  char bufsy[10] = { 0 };
  char buf[10] = { 0 };
  double sx, sy;

  canon->pan_speed = vxspeed;
  canon->tilt_speed = vyspeed;
  if (canon->pan_speed < canon->base.pan_speed_min)
    canon->pan_speed = canon->base.pan_speed_min;
  if (canon->base.pan_speed_max < canon->pan_speed)
    canon->pan_speed = canon->base.pan_speed_max;
  if (canon->tilt_speed < canon->base.tilt_speed_min)
    canon->tilt_speed = canon->base.tilt_speed_min;
  if (canon->base.tilt_speed_max < canon->tilt_speed)
    canon->tilt_speed = canon->base.tilt_speed_max;

  sx = (canon->range->u_pan_speed_max - canon->range->u_pan_speed_min)
      / (canon->base.pan_speed_max - canon->base.pan_speed_min);
  sy = (canon->range->u_tilt_speed_max - canon->range->u_tilt_speed_min)
      / (canon->base.tilt_speed_max - canon->base.tilt_speed_min);
  xspeed = canon->range->u_pan_speed_min + canon->pan_speed * sx;
  yspeed = canon->range->u_tilt_speed_min + canon->tilt_speed * sy;

  //008~320h
  if (xspeed < canon->range->u_pan_speed_min /*0x8 */ )
    xspeed = canon->range->u_pan_speed_min /*0x8 */ ;
  if (canon->range->u_pan_speed_max /*0x320 */  < xspeed)
    xspeed = canon->range->u_pan_speed_max /*0x320 */ ;

  //008~26Eh
  if (yspeed < canon->range->u_tilt_speed_min /*0x8 */ )
    yspeed = canon->range->u_tilt_speed_min /*0x8 */ ;
  if (canon->range->u_tilt_speed_max /*0x26E */  < yspeed)
    yspeed = canon->range->u_tilt_speed_max /*0x26E */ ;

  sprintf (bufsx, "%03X", xspeed);
  sprintf (bufsy, "%03X", yspeed);

  switch (dir) {
    case CAM_RUN_NONE:
      buf[0] = buf[1] = '0';
      break;
    case CAM_RUN_LEFT:
      buf[0] = start ? '2' : '0', buf[1] = '0';
      break;
    case CAM_RUN_LEFT_TOP:
      buf[0] = start ? '2' : '0', buf[1] = start ? '1' : '0';
      break;
    case CAM_RUN_LEFT_BOTTOM:
      buf[0] = start ? '2' : '0', buf[1] = start ? '2' : '0';
      break;
    case CAM_RUN_RIGHT:
      buf[0] = start ? '1' : '0', buf[1] = '0';
      break;
    case CAM_RUN_RIGHT_TOP:
      buf[0] = start ? '1' : '0', buf[1] = start ? '1' : '0';
      break;
    case CAM_RUN_RIGHT_BOTTOM:
      buf[0] = start ? '1' : '0', buf[1] = start ? '2' : '0';
      break;
    case CAM_RUN_TOP:
      buf[0] = '0', buf[1] = start ? '1' : '0';
      break;
    case CAM_RUN_BOTTOM:
      buf[0] = '0', buf[1] = start ? '2' : '0';
      break;
  }

  buf[3] = 0;
  g_print
      ("canon: run(%f, %f, %d, %d) -- %s\n", vxspeed, vyspeed, dir, start, buf);

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
  canon_message_reset (&msg);
  canon_message_append (&msg, 0xFF);
  canon_message_append (&msg, 0x30);
  canon_message_append (&msg, 0x31);
  canon_message_append (&msg, 0x00);
  canon_message_append (&msg, 0x60);
  canon_message_append (&msg, buf[0]);  // pan: 0, 1, 2
  canon_message_append (&msg, buf[1]);  // tilt: 0, 1, 2
  //canon_message_append (&msg, 0x53);
  //canon_message_append (&msg, 0x30);    // pan/tilt: 0/1
  canon_message_append (&msg, 0xEF);
  canon_message_send_with_reply (canon->fd, &msg, &reply);
  return TRUE;
}

static gboolean
gst_cam_controller_canon_move (GstCamControllerCanon * canon, double vxspeed,
    double vx, double vyspeed, double vy)
{
  canon_message msg = canon_message_init (0);
  canon_message reply = canon_message_init (0);
  guint xspeed;                 // = 0x8 + (canon->pan_speed = vxspeed) * 0x320;
  guint yspeed;                 // = 0x8 + (canon->tilt_speed = vyspeed) * 0x26E;
  guint x;                      // = 0x7C87 + (canon->pan = vx) * (0x8379 - 0x7C87);     // 7C87~8379h
  guint y;                      // = 0x7EF5 + (canon->tilt = vy) * (0x810B - 0x7EF5);    // 7eF5~810Bh
  char bufsx[10] = { 0 };
  char bufsy[10] = { 0 };
  char bufx[10] = { 0 };
  char bufy[10] = { 0 };
  double lx = canon->range->u_pan_max - canon->range->u_pan_min;
  double ly = canon->range->u_tilt_max - canon->range->u_tilt_min;
  double px = lx / (canon->base.pan_max - canon->base.pan_min);
  double py = ly / (canon->base.tilt_max - canon->base.tilt_min);
  double sx, sy;

  canon->pan_speed = vxspeed;
  canon->pan = vx;
  canon->tilt_speed = vyspeed;
  canon->tilt = vy;
  if (canon->pan < canon->base.pan_min)
    canon->pan = canon->base.pan_min;
  if (canon->base.pan_max < canon->pan)
    canon->pan = canon->base.pan_max;
  if (canon->tilt < canon->base.tilt_min)
    canon->tilt = canon->base.tilt_min;
  if (canon->base.tilt_max < canon->tilt)
    canon->tilt = canon->base.tilt_max;

  if (canon->pan_speed < canon->base.pan_speed_min)
    canon->pan_speed = canon->base.pan_speed_min;
  if (canon->base.pan_speed_max < canon->pan_speed)
    canon->pan_speed = canon->base.pan_speed_max;
  if (canon->tilt_speed < canon->base.tilt_speed_min)
    canon->tilt_speed = canon->base.tilt_speed_min;
  if (canon->base.tilt_speed_max < canon->tilt_speed)
    canon->tilt_speed = canon->base.tilt_speed_max;

  sx = (canon->range->u_pan_speed_max - canon->range->u_pan_speed_min)
      / (canon->base.pan_speed_max - canon->base.pan_speed_min);
  sy = (canon->range->u_tilt_speed_max - canon->range->u_tilt_speed_min)
      / (canon->base.tilt_speed_max - canon->base.tilt_speed_min);
  xspeed = canon->range->u_pan_speed_min + canon->pan_speed * sx;
  yspeed = canon->range->u_tilt_speed_min + canon->tilt_speed * sy;
  x = canon->range->u_pan_min + lx * 0.5 + canon->pan * px + 0.5;
  y = canon->range->u_tilt_min + ly * 0.5 + canon->tilt * py + 0.5;

  //008~320h
  if (xspeed < canon->range->u_pan_speed_min /*0x8 */ )
    xspeed = canon->range->u_pan_speed_min /*0x8 */ ;
  if (canon->range->u_pan_speed_max /*0x320 */  < xspeed)
    xspeed = canon->range->u_pan_speed_max /*0x320 */ ;

  //008~26Eh
  if (yspeed < canon->range->u_tilt_speed_min /*0x8 */ )
    yspeed = canon->range->u_tilt_speed_min /*0x8 */ ;
  if (canon->range->u_tilt_speed_max /*0x26E */  < yspeed)
    yspeed = canon->range->u_tilt_speed_max /*0x26E */ ;

  //7C87~8379h
  if (x < canon->range->u_pan_min /*0x7C87 */ )
    x = canon->range->u_pan_min /*0x7C87 */ ;
  if (canon->range->u_pan_max /*0x8379 */  < x)
    x = canon->range->u_pan_max /*0x8379 */ ;

  //7eF5~810B
  if (y < canon->range->u_tilt_min /*0x7EF5 */ )
    y = canon->range->u_tilt_min /*0x7EF5 */ ;
  if (canon->range->u_tilt_max /*0x810B */  < y)
    y = canon->range->u_tilt_max /*0x810B */ ;

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
}

static gboolean
gst_cam_controller_canon_tilt (GstCamControllerCanon * canon, double speed,
    double v)
{
  return gst_cam_controller_canon_move (canon, canon->pan_speed, canon->pan,
      speed, v);
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
  guint z = canon->range->u_zoom_min + (canon->zoom = vz) * canon->range->u_zoom_max;   // 0000~07A8h
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
  if (canon->range->u_zoom_max /*0x07A8 */  < z)
    z = canon->range->u_zoom_max /*0x07A8 */ ;

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
  camctl_class->run = (GstCamControllerRunFunc) gst_cam_controller_canon_run;
  camctl_class->query =
      (GstCamControllerQueryFunc) gst_cam_controller_canon_query;
  camctl_class->zoom = (GstCamControllerZoomFunc) gst_cam_controller_canon_zoom;
}
