/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstgroup.h:
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

#ifndef __GST_GROUP_H__
#define __GST_GROUP_H__

#include <glib.h>
#include <libxml/xmlwriter.h>

#include "gstmediacommon.h"
#include "gstrepresentation.h"

typedef struct _GstGroup GstGroup;

/**
 * GstGroup:
 */
struct _GstGroup
{
  GstMediaCommon common;
  guint32 minBandwidth;
  guint32 maxBandwidth;
  guint32 minWidth;
  guint32 maxWidth;
  guint32 minHeight;
  guint32 maxHeight;
  gdouble minFramerate;
  gdouble maxFramerate;
  gboolean segmentAlignmentFlag;
  gboolean bitStreamSwitchingFlag;
  gboolean subsegmentAlignment;
  GHashTable *representations;
};

GstGroup * gst_group_new(gchar *mimeType);
void gst_group_free (GstGroup *group);
gboolean gst_group_add_representation (GstGroup *group, GstRepresentation *rep);
gboolean gst_group_remove_representation (GstGroup *group, gchar *id);
GstRepresentation * gst_group_get_representation (GstGroup  *group, gchar *id);
gboolean gst_group_add_media_segment (GstGroup *group, gchar *id,
    GstMediaSegment *media_segment);
gboolean gst_group_set_init_segment (GstGroup *group, gchar *id,
    GstMediaSegment *init_segment);
guint64 gst_group_get_duration (GstGroup *group);
gboolean gst_group_render (GstGroup *group, xmlTextWriterPtr writer);

#endif
