/*
 * GStreamer
 * Copyright (C) 2011 Flumotion S.L.
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstmediapresentation.h:
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

#ifndef __GST_MEDIA_PRESENTATION_H__
#define __GST_MEDIA_PRESENTATION_H__

#include "gstperiod.h"

typedef struct _GstMediaPresentation GstMediaPresentation;

typedef enum MediaPresentationType
{
  MEDIA_PRESENTATION_TYPE_ONDEMAND,
  /* Not supported yet
  MEDIA_PRESENTATION_TYPE_LIVE,
  */
} MediaPresentationType;

typedef enum MediaPresentationProfile
{
  MEDIA_PRESENTATION_PROFILE_ISOFF_ONDEMAND,
  MEDIA_PRESENTATION_PROFILE_MPEGTS_ONDEMAND,
  /* Not supported yet
  MEDIA_PRESENTATION_PROFILE_ISOFF_LIVE,
  MEDIA_PRESENTATION_PROFILE_MPEGTS_LIVE,
  */
} MediaPresentationProfile;

typedef enum StreamType
{
  STREAM_TYPE_AUDIO,
  STREAM_TYPE_VIDEO,
  STREAM_TYPE_UNKOWN,
} StreamType;

/**
 * GstMediaPrepresentation:
 */
struct _GstMediaPresentation
{
  guint64 availabilityStartTime;
  guint64 availabilityEndTime;
  guint64 mediaPresentationDuration;
  guint64 minimumUpdatePeriodMDP;
  guint64 minBufferTime;
  MediaPresentationType type;           /* Media presentation type */
  GList *profiles;                      /* List of MediaPresentationProfile */
  GList *periods;                       /* List of GstPeriod */
  GList *baseUrls;                      /* List of base urls strings */
};

GstMediaPresentation * gst_media_presentation_new(MediaPresentationType type);
void gst_media_presentation_free(GstMediaPresentation *mdp);
GstPeriod * gst_media_presentation_current_period (GstMediaPresentation *mdp);
void gst_media_presentation_clear (GstMediaPresentation *mdp);
gboolean gst_media_presentation_add_stream (GstMediaPresentation *mdp, StreamType type, gchar *id,
    const gchar *mimeType, guint32 width, guint32 height, guint32 parx, guint32 pary,
    gdouble frameRate, gchar* channels, guint32 samplingRate, guint32 bitrate);
gboolean gst_media_presentation_remove_stream (GstMediaPresentation *mdp, gchar *id);
gboolean gst_media_presentation_add_media_segment (GstMediaPresentation *mdp, gchar *id,
    gchar * url, guint index, guint64 start_ts, guint64 duration, guint64 offset,
    guint64 length);
gboolean gst_media_presentation_set_init_segment (GstMediaPresentation *mdp, gchar *id,
    gchar * url, guint64 offset, guint64 length);
gchar * gst_media_presentation_render (GstMediaPresentation *mdp);

#endif
