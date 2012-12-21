/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gsthlsdemux.h:
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


#ifndef __GST_HLS_DEMUX_H__
#define __GST_HLS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "m3u8.h"
#include "gstfragmented.h"
#include "gsturidownloader.h"
#include "gsthlsadaptation.h"

G_BEGIN_DECLS
#define GST_TYPE_HLS_DEMUX \
  (gst_hls_demux_get_type())
#define GST_HLS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HLS_DEMUX,GstHLSDemux))
#define GST_HLS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HLS_DEMUX,GstHLSDemuxClass))
#define GST_IS_HLS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HLS_DEMUX))
#define GST_IS_HLS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HLS_DEMUX))
typedef struct _GstHLSDemux GstHLSDemux;
typedef struct _GstHLSDemuxClass GstHLSDemuxClass;
typedef struct _GstHLSDemuxPadData GstHLSDemuxPadData;

/**
 * GstHLSDemuxPadData:
 *
 * Opaque #GstHLSDemuxPadData data structure.
 */
struct _GstHLSDemuxPadData
{
  GstM3U8MediaType type;
  GstPad *pad;
  GstPad *in_pad;
  GQueue *queue;
  gboolean need_segment;
  guint *signal;
  const gchar *desc;
  gboolean pad_added;

  /* Stream selection */
  GHashTable *streams;
  gint current_stream;

  /* Audio stream selection */
  GstPad *active_pad;
  GstPad *main_pad;
  GstPad *alt_pad;
};

/**
 * GstHLSDemux:
 *
 * Opaque #GstHLSDemux data structure.
 */
struct _GstHLSDemux
{
  GstBin parent;

  GstHLSDemuxPadData *video_srcpad;
  GstHLSDemuxPadData *audio_srcpad;
  GstHLSDemuxPadData *subtt_srcpad;
  GstPad *sinkpad;

  GstBuffer *playlist;
  GstUriDownloader *downloader;
  GstM3U8Client *client;        /* M3U8 client */
  gboolean need_cache;          /* Wheter we need to cache some fragments before starting to push data */
  gboolean end_of_playlist;
  GstHLSAdaptation *adaptation;
  GstHLSAdaptationAlgorithmFunc algo_func;
  GMutex *pads_lock;

  /* Trick modes */
  gboolean i_frames_mode;
  gboolean rate;
  gboolean need_init_segment;

  /* Properties */
  guint fragments_cache;        /* number of fragments needed to be cached to start playing */
  gfloat bitrate_limit;         /* limit of the available bitrate to use */
  guint connection_speed;       /* Network connection speed in kbps (0 = unknown) */
  gint adaptation_algo;         /* Algorithm used to select the active stream */
  gchar *max_resolution;        /* Maximum resolution allowed */

  /* Streaming task */
  GstTask *stream_task;
  GStaticRecMutex stream_lock;
  gboolean stop_stream_task;

  /* Updates task */
  GstTask *updates_task;
  GStaticRecMutex updates_lock;
  GMutex *updates_timed_lock;
  GTimeVal next_update;         /* Time of the next update */
  gboolean cancelled;

  /* Bin */
  GstElement *avdemux;          /* Demuxer for the main stream */
  GstElement *ademux;           /* Demuxer for the audio stream */
  GstElement *selector;         /* input selector for the audio stream */
  GstEvent *newsegment;         /* New segment events */
  guint64 start_ts;
  gboolean need_pts_sync;
  GstClockTime pts_diff;
  gboolean signal_no_more_pads;
  gboolean avdemux_added;
  gboolean ademux_added;
  gboolean need_audio_switch;
  gboolean delay_audio_switch;
  GstClockTime next_audio_switch;
};

struct _GstHLSDemuxClass
{
  GstBinClass parent_class;

  /* inform that a stream has been set */
  void (*video_changed) (GstHLSDemux * demux);
  void (*audio_changed) (GstHLSDemux * demux);
  void (*text_changed) (GstHLSDemux * demux);

  /* inform that the streams metadata have changed */
  void (*streams_changed) (GstHLSDemux * demux);

  /* get audio/video/text tags for a stream */
  GstTagList *(*get_video_tags) (GstHLSDemux * demux, gint stream);
  GstTagList *(*get_audio_tags) (GstHLSDemux * demux, gint stream);
  GstTagList *(*get_text_tags) (GstHLSDemux * demux, gint stream);
};

GType gst_hls_demux_get_type (void);

void gst_hls_demux_set_adaptation_algorithm_func (GstHLSDemux *demux,
                                                  GstHLSAdaptationAlgorithmFunc func);

G_END_DECLS
#endif /* __GST_HLS_DEMUX_H__ */
