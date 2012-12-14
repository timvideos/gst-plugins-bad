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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

/**
 * SECTION: element-gsttcpfunnelsrc
 *
 * The tcpfunnelsrc element take streams from network via TCP into one
 * stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v tcpfunnelsrc ! switch name=ss ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttcpfunnelsrc.h"

GST_DEBUG_CATEGORY_STATIC (tcpfunnelsrc_debug);
#define GST_CAT_DEFAULT tcpfunnelsrc_debug

#define TCP_BACKLOG             1       /* client connection queue */
#define TCP_HIGHEST_PORT        65535
#define TCP_DEFAULT_PORT        4953
#define TCP_DEFAULT_HOST        "localhost"
#define TCP_DEFAULT_LISTEN_HOST NULL    /* listen on all interfaces */

#define MAX_READ_SIZE           4 * 1024

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_BOUND_PORT,
};

G_DEFINE_TYPE (GstTCPFunnelSrc, gst_tcp_funnel_src, GST_TYPE_PUSH_SRC);

static void
gst_tcp_funnel_src_finalize (GObject * gobject)
{
  GstTCPFunnelSrc *src = GST_TCP_FUNNEL_SRC (gobject);

  if (src->cancellable)
    g_object_unref (src->cancellable);
  src->cancellable = NULL;

  if (src->server_socket)
    g_object_unref (src->server_socket);
  src->server_socket = NULL;

  if (src->client_socket)
    g_object_unref (src->client_socket);
  src->client_socket = NULL;

  g_free (src->host);
  src->host = NULL;

  G_OBJECT_CLASS (gst_tcp_funnel_src_parent_class)->finalize (gobject);
}

static GstFlowReturn
gst_tcp_funnel_src_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstTCPFunnelSrc *src;
  gssize availableBytes, receivedBytes;
  gsize readBytes;
  GstMapInfo map;
  GError *err = NULL;

  src = GST_TCP_FUNNEL_SRC (psrc);

  if (!GST_OBJECT_FLAG_IS_SET (src, GST_TCP_FUNNEL_SRC_OPEN))
    goto wrong_funnel_src_state;

  if (!src->client_socket) {
    /* wait on server socket for connections */
    src->client_socket = g_socket_accept (src->server_socket,
        src->cancellable, &err);
    if (!src->client_socket)
      goto socket_accept_error;
    /* now read from the socket. */
  }

  /* if we have a client, wait for read */
  GST_LOG_OBJECT (src, "asked for a buffer");

  /* read the buffer header */
  availableBytes = g_socket_get_available_bytes (src->client_socket);
  if (availableBytes < 0) {
    goto socket_get_available_bytes_error;
  } else if (availableBytes == 0) {
    GIOCondition condition;

    if (!g_socket_condition_wait (src->client_socket,
            G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP, src->cancellable, &err))
      goto socket_condition_wait_error;

    condition = g_socket_condition_check (src->client_socket,
        G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP);

    if ((condition & G_IO_ERR))
      goto socket_condition_error;
    else if ((condition & G_IO_HUP))
      goto socket_condition_hup;

    availableBytes = g_socket_get_available_bytes (src->client_socket);
    if (availableBytes < 0)
      goto socket_get_available_bytes_error;
  }

  if (0 < availableBytes) {
    readBytes = MIN (availableBytes, MAX_READ_SIZE);
    *outbuf = gst_buffer_new_and_alloc (readBytes);
    gst_buffer_map (*outbuf, &map, GST_MAP_READWRITE);
    receivedBytes = g_socket_receive (src->client_socket, (gchar *) map.data,
        readBytes, src->cancellable, &err);
  } else {
    /* Connection closed */
    receivedBytes = 0;
    readBytes = 0;
    *outbuf = NULL;
  }

  if (receivedBytes == 0)
    goto socket_connection_closed;
  else if (receivedBytes < 0)
    goto socket_receive_error;

  gst_buffer_unmap (*outbuf, &map);
  gst_buffer_resize (*outbuf, 0, receivedBytes);

  GST_LOG_OBJECT (src,
      "Returning buffer from _get of size %" G_GSIZE_FORMAT
      ", ts %" GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
      ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      gst_buffer_get_size (*outbuf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*outbuf)),
      GST_BUFFER_OFFSET (*outbuf), GST_BUFFER_OFFSET_END (*outbuf));

  g_clear_error (&err);

  return GST_FLOW_OK;

  /* Handling Errors */
wrong_funnel_src_state:
  {
    GST_DEBUG_OBJECT (src, "connection to closed, cannot read data");
    return GST_FLOW_FLUSHING;
  }

socket_get_available_bytes_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Failed to get available bytes from socket"));
    return GST_FLOW_ERROR;
  }

socket_accept_error:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled accepting of client");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to accept client: %s", err->message));
    }
    g_clear_error (&err);
    return GST_FLOW_ERROR;
  }

socket_condition_wait_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Select failed: %s", err->message));
    g_clear_error (&err);
    return GST_FLOW_ERROR;
  }

socket_condition_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("Socket in error state"));
    *outbuf = NULL;
    return GST_FLOW_ERROR;
  }

socket_condition_hup:
  {
    GST_DEBUG_OBJECT (src, "Connection closed");
    *outbuf = NULL;
    return GST_FLOW_EOS;
  }

socket_connection_closed:
  {
    GST_DEBUG_OBJECT (src, "Connection closed");
    if (*outbuf) {
      gst_buffer_unmap (*outbuf, &map);
      gst_buffer_unref (*outbuf);
    }
    *outbuf = NULL;
    return GST_FLOW_EOS;
  }

socket_receive_error:
  {
    gst_buffer_unmap (*outbuf, &map);
    gst_buffer_unref (*outbuf);
    *outbuf = NULL;
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled reading from socket");
      return GST_FLOW_FLUSHING;
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
          ("Failed to read from socket: %s", err->message));
      return GST_FLOW_ERROR;
    }
  }
}

static void
gst_tcp_funnel_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTCPFunnelSrc *tcpfunnelsrc = GST_TCP_FUNNEL_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (tcpfunnelsrc->host);
      tcpfunnelsrc->host = g_strdup (g_value_get_string (value));
      break;
    case PROP_PORT:
      tcpfunnelsrc->server_port = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tcp_funnel_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTCPFunnelSrc *tcpfunnelsrc = GST_TCP_FUNNEL_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, tcpfunnelsrc->host);
      break;
    case PROP_PORT:
      g_value_set_int (value, tcpfunnelsrc->server_port);
      break;
    case PROP_BOUND_PORT:
      g_value_set_int (value, g_atomic_int_get (&tcpfunnelsrc->bound_port));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_tcp_funnel_src_stop (GstBaseSrc * bsrc)
{
  GstTCPFunnelSrc *src = GST_TCP_FUNNEL_SRC (bsrc);
  GError *err = NULL;

  if (src->client_socket) {
    GST_DEBUG_OBJECT (src, "closing socket");

    if (!g_socket_close (src->client_socket, &err)) {
      GST_ERROR_OBJECT (src, "Failed to close socket: %s", err->message);
      g_clear_error (&err);
    }
    g_object_unref (src->client_socket);
    src->client_socket = NULL;
  }

  if (src->server_socket) {
    GST_DEBUG_OBJECT (src, "closing socket");

    if (!g_socket_close (src->server_socket, &err)) {
      GST_ERROR_OBJECT (src, "Failed to close socket: %s", err->message);
      g_clear_error (&err);
    }
    g_object_unref (src->server_socket);
    src->server_socket = NULL;

    g_atomic_int_set (&src->bound_port, 0);
    g_object_notify (G_OBJECT (src), "bound-port");
  }

  GST_OBJECT_FLAG_UNSET (src, GST_TCP_FUNNEL_SRC_OPEN);

  return TRUE;
}

/* set up server */
static gboolean
gst_tcp_funnel_src_start (GstBaseSrc * bsrc)
{
  GstTCPFunnelSrc *src = GST_TCP_FUNNEL_SRC (bsrc);
  GError *err = NULL;
  GInetAddress *addr;
  GSocketAddress *saddr;
  GResolver *resolver;
  gint bound_port = 0;

  /* look up name if we need to */
  addr = g_inet_address_new_from_string (src->host);
  if (!addr) {
    GList *results;

    resolver = g_resolver_get_default ();
    results = g_resolver_lookup_by_name (resolver, src->host,
        src->cancellable, &err);
    if (!results)
      goto resolve_no_name;

    addr = G_INET_ADDRESS (g_object_ref (results->data));

    g_resolver_free_addresses (results);
    g_object_unref (resolver);
  }

  {
    gchar *ip = g_inet_address_to_string (addr);
    GST_DEBUG_OBJECT (src, "IP address for host %s is %s", src->host, ip);
    g_free (ip);
  }

  saddr = g_inet_socket_address_new (addr, src->server_port);
  g_object_unref (addr);

  /* create the server listener socket */
  src->server_socket = g_socket_new (g_socket_address_get_family (saddr),
      G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &err);
  if (!src->server_socket)
    goto socket_new_failed;

  GST_DEBUG_OBJECT (src, "opened receiving server socket");

  /* bind it */
  GST_DEBUG_OBJECT (src, "binding server socket to address");
  if (!g_socket_bind (src->server_socket, saddr, TRUE, &err))
    goto socket_bind_failed;

  g_object_unref (saddr);

  GST_DEBUG_OBJECT (src, "listening on server socket");

  g_socket_set_listen_backlog (src->server_socket, TCP_BACKLOG);

  if (!g_socket_listen (src->server_socket, &err))
    goto socket_listen_failed;

  GST_OBJECT_FLAG_SET (src, GST_TCP_FUNNEL_SRC_OPEN);

  if (src->server_port == 0) {
    saddr = g_socket_get_local_address (src->server_socket, NULL);
    bound_port = g_inet_socket_address_get_port ((GInetSocketAddress *) saddr);
    g_object_unref (saddr);
  } else {
    bound_port = src->server_port;
  }

  GST_DEBUG_OBJECT (src, "listening on port %d", bound_port);

  g_atomic_int_set (&src->bound_port, bound_port);
  g_object_notify (G_OBJECT (src), "bound-port");

  return TRUE;

  /* Handling Errors */
resolve_no_name:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled name resolval");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to resolve host '%s': %s", src->host, err->message));
    }
    g_clear_error (&err);
    g_object_unref (resolver);
    return FALSE;
  }

socket_new_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Failed to create socket: %s", err->message));
    g_clear_error (&err);
    g_object_unref (saddr);
    return FALSE;
  }

socket_bind_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled binding");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to bind on host '%s:%d': %s", src->host, src->server_port,
              err->message));
    }
    g_clear_error (&err);
    g_object_unref (saddr);
    gst_tcp_funnel_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }

socket_listen_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled listening");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to listen on host '%s:%d': %s", src->host,
              src->server_port, err->message));
    }
    g_clear_error (&err);
    gst_tcp_funnel_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
}

/* will be called only between calls to start() and stop() */
static gboolean
gst_tcp_funnel_src_unlock (GstBaseSrc * bsrc)
{
  GstTCPFunnelSrc *src = GST_TCP_FUNNEL_SRC (bsrc);

  g_cancellable_cancel (src->cancellable);

  return TRUE;
}

static gboolean
gst_tcp_funnel_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstTCPFunnelSrc *src = GST_TCP_FUNNEL_SRC (bsrc);

  g_cancellable_reset (src->cancellable);

  return TRUE;
}

static void
gst_tcp_funnel_src_class_init (GstTCPFunnelSrcClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstBaseSrcClass *basesrc_class;
  GstPushSrcClass *push_src_class;

  object_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  basesrc_class = (GstBaseSrcClass *) klass;
  push_src_class = (GstPushSrcClass *) klass;

  object_class->set_property = gst_tcp_funnel_src_set_property;
  object_class->get_property = gst_tcp_funnel_src_get_property;
  object_class->finalize = gst_tcp_funnel_src_finalize;

  g_object_class_install_property (object_class, PROP_HOST,
      g_param_spec_string ("host", "Host", "The hostname to listen as",
          TCP_DEFAULT_LISTEN_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "The port to listen to (0=random)",
          0, TCP_HIGHEST_PORT, TCP_DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_BOUND_PORT,
      g_param_spec_int ("bound-port", "BoundPort",
          "The port number the socket is currently bound to", 0,
          TCP_HIGHEST_PORT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&srctemplate));

  gst_element_class_set_static_metadata (element_class,
      "TCP funnel server source", "Source/Network",
      "Receive data as a server over the network via TCP from multiple clients",
      "Duzy Chan <code@duzy.info>");

  basesrc_class->start = gst_tcp_funnel_src_start;
  basesrc_class->stop = gst_tcp_funnel_src_stop;
  basesrc_class->unlock = gst_tcp_funnel_src_unlock;
  basesrc_class->unlock_stop = gst_tcp_funnel_src_unlock_stop;

  push_src_class->create = gst_tcp_funnel_src_create;
}

static void
gst_tcp_funnel_src_init (GstTCPFunnelSrc * src)
{
  src->server_port = TCP_DEFAULT_PORT;
  src->host = g_strdup (TCP_DEFAULT_HOST);
  src->server_socket = NULL;
  src->client_socket = NULL;
  src->cancellable = g_cancellable_new ();

  GST_OBJECT_FLAG_UNSET (src, GST_TCP_FUNNEL_SRC_OPEN);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (tcpfunnelsrc_debug, "tcpfunnelsrc", 0,
      "Performs face detection on videos and images, providing "
      "detected positions via bus messages");

  return gst_element_register (plugin, "tcpfunnelsrc", GST_RANK_NONE,
      GST_TYPE_TCP_FUNNEL_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    tcpfunnelsrc, "GStreamer TCP Funnel Source Plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
