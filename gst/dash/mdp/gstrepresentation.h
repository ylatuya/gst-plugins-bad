/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstrepresentation.h:
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

#ifndef __GST_REPRESENTATION_H__
#define __GST_REPRESENTATION_H__

#include "gstmediacommon.h"
#include "gstsegmentlist.h"

typedef struct _GstRepresentation GstRepresentation;

/**
 * GstRepresentation:
 */
struct _GstRepresentation
{
  GstMediaCommon common;
  gchar *id;
  guint32 bandwidth;
  gboolean startWithRAP;
  GstSegmentList *segment_list;
};


GstRepresentation * gst_representation_new  (gchar *id, StreamType stream_type,
                                             const gchar *mimeType, guint32 width,
                                             guint32 height, guint32 parx, guint32 pary,
                                             gdouble frameRate, gchar* channels,
                                             guint32 samplingRate, guint32 bitrate,
                                             const gchar  *lang, gboolean use_rangesm,
                                             guint fragment_duration);

void gst_representation_free                 (GstRepresentation *rep);

void gst_representation_add_media_segment    (GstRepresentation *rep,
                                              GstMediaSegment *segment);

void gst_representation_set_init_segment     (GstRepresentation * rep,
                                              GstMediaSegment *segment);

void gst_representation_add_base_url         (GstRepresentation * rep, gchar *url);

guint64 gst_representation_get_duration      (GstRepresentation *mdp);

gboolean gst_representation_render           (GstRepresentation *rep,
                                              xmlTextWriterPtr writer);

#endif
