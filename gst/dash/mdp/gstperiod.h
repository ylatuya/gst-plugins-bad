/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstperiod.h:
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

#ifndef __GST_PERIOD_H__
#define __GST_PERIOD_H__

#include <glib.h>
#include <libxml/xmlwriter.h>

#include "gstadaptationset.h"
#include "gstrepresentation.h"

typedef struct _GstPeriod GstPeriod;

/**
 * GstPeriod:
 */
struct _GstPeriod
{
  guint64 start;
  guint64 duration;
  guint64 minBufferTime;
  gboolean segmentAlignment;
  gboolean bitstreamSwitching;
  GHashTable *adaptation_sets;                /* mimeType->AdaptationSet hash table */
  /* GList *representation; */

  /* <private> */
  GHashTable *id_to_adaptation_set;
};


GstPeriod * gst_period_new(guint64 start);
void gst_period_free(GstPeriod *period);
gboolean gst_period_add_representation (GstPeriod *period, GstRepresentation *rep);
gboolean gst_period_add_media_segment (GstPeriod *period, gchar* id,
    GstMediaSegment *segment);
gboolean gst_period_set_init_segment (GstPeriod * period, gchar* id,
    GstMediaSegment *segment_info);
guint64 gst_period_get_duration (GstPeriod *period);
gboolean gst_period_render (GstPeriod *period, xmlTextWriterPtr writer);

#endif
