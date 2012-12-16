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


#ifndef __GST_HLS_ADAPTATION_H__
#define __GST_HLS_ADAPTATION_H__

#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstHLSAdaptation GstHLSAdaptation;
typedef struct _GstHLSAdaptationFragment GstHLSAdaptationFragment;
typedef struct _GstHLSAdaptationStream GstHLSAdaptationStream;
typedef guint (*GstHLSAdaptationAlgorithmFunc) (GstHLSAdaptation * adaptation);

#define GST_HLS_ADAPTATION_FRAGMENT(f) ((GstHLSAdaptationFragment*)f)
#define GST_HLS_ADAPTATION_STREAM(b) ((GstHLSAdaptationStream*)b)


struct _GstHLSAdaptationFragment
{
  gsize size;
  guint64 download_time;
};

struct _GstHLSAdaptationStream
{
  guint bandwidth;
};

struct _GstHLSAdaptation
{
  GList *streams;
  GList *fragments;
  gdouble proportion;

  /* Properties */
  GstHLSAdaptationAlgorithmFunc adaptation_func;
  guint max_fragments;
  gint max_bitrate;
  guint connection_speed;

  GMutex *lock;
};

GstHLSAdaptation * gst_hls_adaptation_new       (void);

void gst_hls_adaptation_free                    (GstHLSAdaptation *adaptation);

void gst_hls_adaptation_reset                   (GstHLSAdaptation *adaptation);

void gst_hls_adaptation_add_stream              (GstHLSAdaptation *adaptation, guint bandwidth);

void gst_hls_adaptation_add_fragment            (GstHLSAdaptation *adaptation, gsize size,
                                                 guint64 download_time);

void gst_hls_adaptation_set_algorithm_func      (GstHLSAdaptation *adaptation,
                                                 GstHLSAdaptationAlgorithmFunc func);

void gst_hls_adaptation_set_connection_speed    (GstHLSAdaptation *adaptation, guint speed);

void gst_hls_adaptation_set_max_bitrate         (GstHLSAdaptation *adaptation, guint max_bitrate);

void gst_hls_adaptation_set_max_resolution      (GstHLSAdaptation *adaptation, gchar *resolution);

void gst_hls_adaptation_update_qos_proportion   (GstHLSAdaptation *adaptation, gdouble proportion);

gint gst_hls_adaptation_get_target_bitrate     (GstHLSAdaptation * adaptation);

/* Algorithms */

guint gst_hls_adaptation_always_lowest          (GstHLSAdaptation *adaptation);

guint gst_hls_adaptation_always_highest         (GstHLSAdaptation *adaptation);

guint gst_hls_adaptation_fixed_bitrate          (GstHLSAdaptation *adaptation);

guint gst_hls_adaptation_disabled               (GstHLSAdaptation *adaptation);

guint gst_hls_adaptation_bandwidth_estimation   (GstHLSAdaptation *adaptation);

G_END_DECLS

#endif
