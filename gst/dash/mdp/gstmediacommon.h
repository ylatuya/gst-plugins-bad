/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstmediacommon.h:
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


#ifndef __GST_MEDIA_COMMON_H__
#define __GST_MEDIA_COMMON_H__

#include <glib.h>
#include <libxml/xmlwriter.h>

typedef struct _GstMediaCommon GstMediaCommon;

typedef enum StreamType
{
  STREAM_TYPE_AUDIO,
  STREAM_TYPE_VIDEO,
  STREAM_TYPE_SUBTITLE,
  STREAM_TYPE_UNKNOWN,
} StreamType;


/**
 * GstMediaCommon:
 */
struct _GstMediaCommon
{
  guint32 width;
  guint32 height;
  guint32 parx;
  guint32 pary;
  gdouble frameRate;
  GList *lang;
  GList *numberOfChannels;
  GList *samplingRate;
  gchar *mimeType;
  guint32 group;
  gdouble maximumRAPPeriod;
  StreamType stream_type;
};

void gst_media_common_init (GstMediaCommon *common);
void gst_media_common_free (GstMediaCommon *common);
gboolean gst_media_common_render (GstMediaCommon *common, xmlTextWriterPtr writer);

#endif
