/* GStreamer Opus Encoder
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_OPUS_ENC_H__
#define __GST_OPUS_ENC_H__


#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include <opus/opus.h>

G_BEGIN_DECLS

#define GST_TYPE_OPUS_ENC \
  (gst_opus_enc_get_type())
#define GST_OPUS_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPUS_ENC,GstOpusEnc))
#define GST_OPUS_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPUS_ENC,GstOpusEncClass))
#define GST_IS_OPUS_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPUS_ENC))
#define GST_IS_OPUS_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPUS_ENC))

#define MAX_FRAME_SIZE 2000*2
#define MAX_FRAME_BYTES 2000

typedef struct _GstOpusEnc GstOpusEnc;
typedef struct _GstOpusEncClass GstOpusEncClass;

struct _GstOpusEnc {
  GstElement            element;

  /* pads */
  GstPad                *sinkpad;
  GstPad                *srcpad;

  //OpusHeader            header;
  //OpusMode             *mode;
  OpusEncoder          *state;
  GstAdapter           *adapter;

  /* properties */
  gboolean              audio_or_voip;
  gint                  bitrate;
  gint                  bandwidth;
  gint                  frame_size;
  gboolean              cbr;
  gboolean              constrained_vbr;
  gint                  complexity;
  gboolean              inband_fec;
  gboolean              dtx;
  gint                  packet_loss_percentage;

  int frame_samples;

  gint                  n_channels;
  gint                  sample_rate;

  gboolean              setup;
  gboolean              header_sent;
  gboolean              eos;

  guint64               samples_in;
  guint64               bytes_out;

  guint64               frameno;
  guint64               frameno_out;

  GstClockTime     start_ts;
  GstClockTime     next_ts;
  guint64          granulepos_offset;
};

struct _GstOpusEncClass {
  GstElementClass parent_class;

  /* signals */
  void (*frame_encoded) (GstElement *element);
};

GType gst_opus_enc_get_type (void);

G_END_DECLS

#endif /* __GST_OPUS_ENC_H__ */
