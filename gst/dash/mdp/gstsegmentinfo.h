/*
 * GStreamer
 * Copyright (C) 2011 Flumotion S.L
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstsegmentinfo.h:
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


#ifndef __GST_SEGMENT_INFO_H__
#define __GST_SEGMENT_INFO_H__

#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

typedef struct _GstMediaSegment GstMediaSegment;
typedef struct _GstSegmentInfo GstSegmentInfo;

/**
 * GstMediaSegment:
 */
struct _GstMediaSegment
{
  gchar *url;
  int index;
  guint64 start_ts;
  guint64 duration;
  guint64 offset;
  guint64 size;
};

/**
 * GstSegmentInfo:
 */
struct _GstSegmentInfo
{
  guint64 duration;
  gint32 start_index;
  GList *base_urls;
  GstMediaSegment *init_segment;
  GList *segments;
};

GstSegmentInfo *gst_segment_info_new (void);
void gst_segment_info_free (GstSegmentInfo * segment_info);
void gst_segment_info_add_media_segment (GstSegmentInfo *info,
                                         GstMediaSegment *segment);
void gst_segment_info_set_init_segment (GstSegmentInfo * segment_info,
                                        GstMediaSegment *segment);
void gst_segment_info_add_base_url (GstSegmentInfo * segment_info,
                                    gchar *url);
gboolean gst_segment_info_render (GstSegmentInfo * segment_info,
                                  xmlTextWriterPtr writer);
GstMediaSegment * gst_media_segment_new (gchar * url, guint index,
    guint64 start_ts, guint64 duration, guint64 offset, guint64 size);
gint32 gst_segment_info_get_average_bitrate (GstSegmentInfo * info);
void gst_media_segment_free (GstMediaSegment * segment);

#endif
