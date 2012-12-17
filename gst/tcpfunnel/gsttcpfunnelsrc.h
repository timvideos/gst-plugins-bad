/* GStreamer
 * Copyright (C) 2012 Duzy Chan <code@duzy.info>
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

#ifndef __GST_TCP_FUNNEL_SRC_H__
#define __GST_TCP_FUNNEL_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gio/gio.h>

G_END_DECLS

#define GST_TYPE_TCP_FUNNEL_SRC \
  (gst_tcp_funnel_src_get_type())
#define GST_TCP_FUNNEL_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TCP_FUNNEL_SRC,GstTCPFunnelSrc))
#define GST_TCP_FUNNEL_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TCP_FUNNEL_SRC,GstTCPFunnelSrcClass))
#define GST_IS_TCP_FUNNEL_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TCP_FUNNEL_SRC))
#define GST_IS_TCP_FUNNEL_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TCP_FUNNEL_SRC))

typedef struct _GstTCPFunnelSrc GstTCPFunnelSrc;
typedef struct _GstTCPFunnelSrcClass GstTCPFunnelSrcClass;

typedef enum {
  GST_TCP_FUNNEL_SRC_OPEN       = (GST_BASE_SRC_FLAG_LAST << 0),
  GST_TCP_FUNNEL_SRC_FLAG_LAST  = (GST_BASE_SRC_FLAG_LAST << 2)
} GstTCPFunnelSrcFlags;

struct _GstTCPFunnelSrc {
  GstPushSrc base;

  gchar *host;
  int server_port;
  int bound_port;          /* currently bound-to port, or 0 */ /* ATOMIC */

  GCancellable *cancellable;
  GSocket *server_socket;
  GList *clients;
  GList *incoming;

  GMutex clients_mutex;
  GMutex incoming_mutex;
  GMutex wait_mutex;

  GCond has_incoming;
  GCond wait_thread_end;

  GThread *wait_thread;
};

struct _GstTCPFunnelSrcClass {
  GstPushSrcClass base_class;
};

GType gst_tcp_funnel_src_get_type (void);

G_BEGIN_DECLS

#endif /* __GST_TCP_FUNNEL_SRC_H__ */
