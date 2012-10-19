/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstadaptation_set.h:
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

#ifndef __GST_ADAPTATION_SET_H__
#define __GST_ADAPTATION_SET_H__

#include <glib.h>
#include <libxml/xmlwriter.h>

#include "gstmediacommon.h"
#include "gstrepresentation.h"

typedef struct _GstAdaptationSet GstAdaptationSet;

/**
 * GstAdaptationSet:
 */
struct _GstAdaptationSet
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
  gboolean segmentAlignment;
  gboolean bitStreamSwitching;
  gboolean subsegmentAlignment;
  GHashTable *representations;
};

GstAdaptationSet * gst_adaptation_set_new                 (StreamType stream_type,
		                                           gchar *mimeType);

void gst_adaptation_set_free                              (GstAdaptationSet *adaptation_set);

gboolean gst_adaptation_set_add_representation            (GstAdaptationSet *adaptation_set,
		                                           GstRepresentation *rep);

gboolean gst_adaptation_set_remove_representation         (GstAdaptationSet *adaptation_set,
		                                           gchar *id);

GstRepresentation * gst_adaptation_set_get_representation (GstAdaptationSet  *adaptation_set,
		                                           gchar *id);

gboolean gst_adaptation_set_add_media_segment             (GstAdaptationSet *adaptation_set,
		                                           gchar *id,
                                                           GstMediaSegment *media_segment);

gboolean gst_adaptation_set_set_init_segment              (GstAdaptationSet *adaptation_set,
		                                           gchar *id,
                                                           GstMediaSegment *init_segment);

guint64 gst_adaptation_set_get_duration                   (GstAdaptationSet *adaptation_set);

gboolean gst_adaptation_set_render                        (GstAdaptationSet *adaptation_set,
		                                           xmlTextWriterPtr writer);

#endif
