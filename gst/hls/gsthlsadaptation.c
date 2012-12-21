/* GStreamer
 * Copyright (C) 2012, Fluendo S.A <support@fluendo.com>
 *  Author: Andoni Morales Alastruey <amorales@fluendo.com>
 *
 * gsthlscache.h:
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


#include <math.h>
#include "gsthlsadaptation.h"

#define GST_HLS_ADAPTATION_LOCK(a) g_mutex_lock(a->lock)
#define GST_HLS_ADAPTATION_UNLOCK(a) g_mutex_unlock(a->lock)

static GstHLSAdaptationFragment *
gst_hls_adaptation_fragment_new (gsize size, guint64 download_time)
{
  GstHLSAdaptationFragment *frag;

  frag = g_new0 (GstHLSAdaptationFragment, 1);
  frag->size = size;
  frag->download_time = download_time;

  return frag;
}

static void
gst_hls_adaptation_fragment_free (GstHLSAdaptationFragment * frag)
{
  g_free (frag);
}

static GstHLSAdaptationStream *
gst_hls_adaptation_stream_new (guint bandwidth)
{
  GstHLSAdaptationStream *stream;

  stream = g_new0 (GstHLSAdaptationStream, 1);
  stream->bandwidth = bandwidth;

  return stream;
}

static void
gst_hls_adaptation_stream_free (GstHLSAdaptationStream * stream)
{
  g_free (stream);
}

static gint
_compare_bandwidths (GstHLSAdaptationStream * a, GstHLSAdaptationStream * b)
{
  return a->bandwidth - b->bandwidth;
}

/********************
 *    Public API    *
 *******************/

GstHLSAdaptation *
gst_hls_adaptation_new (void)
{
  GstHLSAdaptation *adaptation;

  adaptation = g_new0 (GstHLSAdaptation, 1);
  adaptation->streams = NULL;
  adaptation->fragments = NULL;
  adaptation->max_bitrate = 0;
  adaptation->connection_speed = 0;
  adaptation->max_fragments = 5;
  adaptation->adaptation_func =
      (GstHLSAdaptationAlgorithmFunc) gst_hls_adaptation_bandwidth_estimation;
  adaptation->lock = g_mutex_new ();

  return adaptation;
}

void
gst_hls_adaptation_free (GstHLSAdaptation * adaptation)
{
  gst_hls_adaptation_reset (adaptation);
  g_mutex_free (adaptation->lock);
  g_free (adaptation);
}

void
gst_hls_adaptation_reset (GstHLSAdaptation * adaptation)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  if (adaptation->fragments != NULL) {
    g_list_foreach (adaptation->fragments,
        (GFunc) gst_hls_adaptation_fragment_free, NULL);
    g_list_free (adaptation->fragments);
    adaptation->fragments = NULL;
  }

  if (adaptation->streams != NULL) {
    g_list_foreach (adaptation->streams,
        (GFunc) gst_hls_adaptation_stream_free, NULL);
    g_list_free (adaptation->streams);
    adaptation->streams = NULL;
  }

  adaptation->proportion = 0;

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

void
gst_hls_adaptation_add_stream (GstHLSAdaptation * adaptation, guint bandwidth)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  adaptation->streams = g_list_insert_sorted (adaptation->streams,
      gst_hls_adaptation_stream_new (bandwidth),
      (GCompareFunc) _compare_bandwidths);

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

void
gst_hls_adaptation_add_fragment (GstHLSAdaptation * adaptation, gsize size,
    guint64 download_time)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  if (g_list_length (adaptation->fragments) >= adaptation->max_fragments) {
    gst_hls_adaptation_fragment_free (GST_HLS_ADAPTATION_FRAGMENT (g_list_first
            (adaptation->fragments)->data));
    adaptation->fragments =
        g_list_remove (adaptation->fragments,
        g_list_first (adaptation->fragments)->data);
  }

  adaptation->fragments = g_list_append (adaptation->fragments,
      gst_hls_adaptation_fragment_new (size, download_time));

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

void
gst_hls_adaptation_set_algorithm_func (GstHLSAdaptation * adaptation,
    GstHLSAdaptationAlgorithmFunc func)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  adaptation->adaptation_func = func;

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

gint
gst_hls_adaptation_get_target_bitrate (GstHLSAdaptation * adaptation)
{
  gint ret = 0;

  GST_HLS_ADAPTATION_LOCK (adaptation);

  if (adaptation->adaptation_func == NULL)
    goto exit;

  ret = adaptation->adaptation_func (adaptation);

exit:
  GST_HLS_ADAPTATION_UNLOCK (adaptation);
  return ret;
}

void
gst_hls_adaptation_set_connection_speed (GstHLSAdaptation * adaptation,
    guint connection_speed)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  adaptation->connection_speed = connection_speed;

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

void
gst_hls_adaptation_set_max_bitrate (GstHLSAdaptation * adaptation,
    guint max_bitrate)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  adaptation->max_bitrate = max_bitrate;

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

void
gst_hls_adaptation_update_qos_proportion (GstHLSAdaptation * adaptation,
    gdouble proportion)
{
  GST_HLS_ADAPTATION_LOCK (adaptation);

  adaptation->proportion = proportion;

  GST_HLS_ADAPTATION_UNLOCK (adaptation);
}

/* Returns always the lowest bitrate */
guint
gst_hls_adaptation_always_lowest (GstHLSAdaptation * adaptation)
{
  if (adaptation->streams == NULL)
    return -1;

  return GST_HLS_ADAPTATION_STREAM (g_list_first (adaptation->streams)->
      data)->bandwidth;
}

/* Returns always the highest bitrate, but smaller than connection
 * speed if it's set */
guint
gst_hls_adaptation_always_highest (GstHLSAdaptation * adaptation)
{
  guint bitrate = 0;

  if (adaptation->streams == NULL)
    return -1;

  bitrate =
      GST_HLS_ADAPTATION_STREAM (g_list_last (adaptation->streams)->
      data)->bandwidth;

  /* If user specifies a connection speed never use a playlist with a bandwidth
   * superior than it */
  if (adaptation->connection_speed != 0) {
    GList *prev = g_list_last (adaptation->streams);

    while (bitrate > adaptation->connection_speed) {
      prev = g_list_previous (prev);
      if (prev != NULL)
        bitrate = GST_HLS_ADAPTATION_STREAM (prev->data)->bandwidth;
    }
  }

  return bitrate;
}

/* Returns always a fixed bitrate, the one set by connection speed */
guint
gst_hls_adaptation_fixed_bitrate (GstHLSAdaptation * adaptation)
{
  return adaptation->connection_speed;
}

guint
gst_hls_adaptation_disabled (GstHLSAdaptation * adaptation)
{
  return -1;
}

/* Returns an estimation of the average bandwidth, based on a pondered
 * mean of the last downloaded fragments */
guint
gst_hls_adaptation_bandwidth_estimation (GstHLSAdaptation * adaptation)
{
  guint avg_bitrate, ret;
  gdouble bitrates_sum = 0;
  gdouble weights_sum = 0;
  gint i, length;

  if (adaptation->fragments == NULL)
    return -1;

  avg_bitrate = 0;
  length = g_list_length (adaptation->fragments);

  /* Get the estimate bandwith based on a pondered average of the last
   * downloaded fragments */
  for (i = 0; i < length; i++) {
    GstHLSAdaptationFragment *frag;
    gdouble weight = 1.0 / exp (i - 1);

    frag =
        (GstHLSAdaptationFragment *) g_list_nth_data (adaptation->fragments,
        length - i - 1);

    bitrates_sum +=
        frag->size * 8 / ((gdouble) frag->download_time / GST_SECOND) * weight;
    weights_sum += weight;
  }

  /* If we don't have any fragment yet, return the connection speed, which might
   * be 0. */
  if (bitrates_sum == 0 && weights_sum == 0)
    avg_bitrate = adaptation->connection_speed;
  else
    avg_bitrate = bitrates_sum / weights_sum;

  if (adaptation->connection_speed != 0
      && avg_bitrate > adaptation->connection_speed)
    ret = adaptation->connection_speed;
  else
    ret = avg_bitrate;

  if (adaptation->max_bitrate != 0)
    ret *= adaptation->max_bitrate;

  return ret;
}
