/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
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


#ifndef __GST_SEGMENT_LIST_H__
#define __GST_SEGMENT_LIST_H__

#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

typedef struct _GstMediaSegment GstMediaSegment;
typedef struct _GstSegmentList GstSegmentList;

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
 * GstSegmentList:
 */
struct _GstSegmentList
{
  guint64 duration;
  gint32 start_index;
  GList *base_urls;
  gboolean use_ranges;
  GstMediaSegment *init_segment;
  GList *segments;
  guint fragment_duration;
};

GstSegmentList *gst_segment_list_new           (guint fragment_duration, gboolean use_ranges);

void gst_segment_list_free                     (GstSegmentList * segment_list);

void gst_segment_list_add_media_segment        (GstSegmentList *info,
                                                GstMediaSegment *segment);

void gst_segment_list_set_init_segment         (GstSegmentList * segment_list,
                                                GstMediaSegment *segment);

void gst_segment_list_add_base_url             (GstSegmentList * segment_list,
                                                gchar *url);

gboolean gst_segment_list_render               (GstSegmentList * segment_list,
                                                xmlTextWriterPtr writer);

GstMediaSegment * gst_media_segment_new        (gchar * url, guint index,
                                                guint64 start_ts, guint64 duration,
                                                guint64 offset, guint64 size);

gint32 gst_segment_list_get_average_bitrate    (GstSegmentList * info);

void gst_media_segment_free                    (GstMediaSegment * segment);

#endif
