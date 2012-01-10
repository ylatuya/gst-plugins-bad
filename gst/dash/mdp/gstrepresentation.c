/*
 * GStreamer
 * Copyright (C) 2011 Flumotion S.L.
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstrepresentation.c:
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

#include "gstrepresentation.h"
#include "gstsegmentinfo.h"
#include "gstxmlhelper.h"

GstRepresentation *
gst_representation_new (gchar * id, gchar * mimeType, guint32 width,
    guint32 height, guint32 parx, guint32 pary, gdouble frameRate,
    gchar * channels, guint32 samplingRate)
{
  GstRepresentation *rep;

  rep = g_new0 (GstRepresentation, 1);

  gst_media_common_init (&rep->common);
  g_free (rep->common.mimeType);
  rep->common.mimeType = g_strdup (mimeType);
  rep->common.width = width;
  rep->common.height = height;
  rep->common.parx = parx;
  rep->common.pary = pary;
  rep->common.frameRate = frameRate;
  rep->common.numberOfChannels =
      g_list_append (rep->common.numberOfChannels, g_strdup (channels));
  rep->common.samplingRate =
      g_list_append (rep->common.samplingRate, g_strdup_printf ("%d",
          samplingRate));

  rep->id = g_strdup (id);
  rep->bandwidth = 0;
  rep->startWithRAP = TRUE;
  rep->segment_info = gst_segment_info_new ();

  return rep;
}

void
gst_representation_free (GstRepresentation * rep)
{
  g_return_if_fail (rep != NULL);

  g_free (rep->id);
  gst_segment_info_free (rep->segment_info);
}

void
gst_representation_add_media_segment (GstRepresentation * rep,
    GstMediaSegment * segment)
{
  g_return_if_fail (rep != NULL);
  g_return_if_fail (segment != NULL);

  gst_segment_info_add_media_segment (rep->segment_info, segment);
}

void
gst_representation_set_init_segment (GstRepresentation * rep,
    GstMediaSegment * segment)
{
  g_return_if_fail (rep != NULL);
  g_return_if_fail (segment != NULL);

  gst_segment_info_set_init_segment (rep->segment_info, segment);
}

void
gst_representation_add_base_url (GstRepresentation * rep, gchar * url)
{
  g_return_if_fail (rep != NULL);
  g_return_if_fail (url != NULL);

  gst_segment_info_add_base_url (rep->segment_info, url);
}

gboolean
gst_representation_render (GstRepresentation * rep, xmlTextWriterPtr writer)
{
  /* Start Representation */
  if (!gst_media_presentation_start_element (writer, "Representation"))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "bandwidth",
          rep->bandwidth))
    return FALSE;
  if (!gst_media_presentation_write_string_attribute (writer, "id", rep->id))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer, "startWithRAP",
          rep->startWithRAP))
    return FALSE;
  if (!gst_media_common_render (&rep->common, writer))
    return FALSE;

  /* write SegmentInfo */
  if (!gst_segment_info_render (rep->segment_info, writer))
    return FALSE;

  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}
