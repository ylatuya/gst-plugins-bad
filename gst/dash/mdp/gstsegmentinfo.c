/* GStreamer
 * Copyright (C) 2011 Flumotion S.L
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstrepresentationsegment.c:
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

#include "gstsegmentinfo.h"
#include "gstxmlhelper.h"

GstSegmentInfo *
gst_segment_info_new (void)
{
  GstSegmentInfo *info;

  info = g_new0 (GstSegmentInfo, 1);
  info->duration = 0;
  info->start_index = 0;
  info->base_urls = NULL;
  info->init_segment = NULL;
  info->segments = NULL;

  return info;
}

void
gst_segment_info_free (GstSegmentInfo * info)
{
  g_return_if_fail (info != NULL);

  g_list_foreach (info->base_urls, (GFunc) g_free, NULL);
  g_list_free (info->base_urls);

  if (info->init_segment != NULL)
    gst_media_segment_free (info->init_segment);

  g_list_foreach (info->segments, (GFunc) gst_media_segment_free, NULL);
  g_list_free (info->segments);

  g_free (info);
}

void
gst_segment_info_add_media_segment (GstSegmentInfo * info,
    GstMediaSegment * segment)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (segment != NULL);

  if (G_UNLIKELY (g_list_length (info->segments) == 0)) {
    info->start_index = segment->index;
  }

  info->segments = g_list_append (info->segments, segment);
  if (segment->duration != GST_CLOCK_TIME_NONE)
    info->duration += segment->duration;
}

void
gst_segment_info_set_init_segment (GstSegmentInfo * info,
    GstMediaSegment * segment)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (segment != NULL);

  if (info->init_segment != NULL)
    gst_media_segment_free (info->init_segment);

  info->init_segment = segment;
}

void
gst_segment_info_add_base_url (GstSegmentInfo * info, gchar * url)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (url != NULL);

  info->base_urls = g_list_append (info->base_urls, g_strdup (url));
}

static gboolean
gst_media_presentation_write_url (xmlTextWriterPtr writer, const gchar * name,
    const gchar * url)
{
  if (!gst_media_presentation_start_element (writer, name))
    return FALSE;

  if (!gst_media_presentation_write_string_attribute (writer, "sourceURL", url))
    return FALSE;

  if (!gst_media_presentation_end_element (writer))
    return FALSE;
  return TRUE;
}

gint32
gst_segment_info_get_average_bitrate (GstSegmentInfo * info)
{
  GList *tmp;
  guint64 size = 0;

  tmp = g_list_first (info->segments);
  while (tmp != NULL) {
    size += ((GstMediaSegment *) tmp->data)->size;
    tmp = g_list_next (tmp);
  }

  return GST_ROUND_UP_64 (size / (info->duration / GST_SECOND));
}

gboolean
gst_segment_info_render (GstSegmentInfo * info, xmlTextWriterPtr writer)
{
  GList *tmp;
  guint64 duration;

  tmp = g_list_first (info->segments);
  if (tmp == NULL)
    return TRUE;

  duration = ((GstMediaSegment *) tmp->data)->duration;

  /* Start SegmentInfo */
  if (!gst_media_presentation_start_element (writer, "SegmentInfo"))
    return FALSE;
  if (!gst_media_presentation_write_time_seconds_attribute (writer, "duration",
          duration))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "start_index",
          info->start_index))
    return FALSE;

  /* Add BaseURL's */
  if (!gst_media_presentation_write_base_urls (writer, info->base_urls))
    return FALSE;

  /* Write initialization segment */
  if (info->init_segment != NULL) {
    if (!gst_media_presentation_write_url (writer, "InitialisationSegmentURL",
            (const gchar *) info->init_segment->url))
      return FALSE;
  }

  tmp = g_list_first (info->segments);
  while (tmp != NULL) {
    GstMediaSegment *segment = (GstMediaSegment *) tmp->data;

    if (!gst_media_presentation_write_url (writer, "Url",
            (const gchar *) segment->url))
      return FALSE;
    tmp = g_list_next (tmp);
  }

  /* End SegmentInfo */
  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}

GstMediaSegment *
gst_media_segment_new (gchar * url, guint index,
    guint64 start_ts, guint64 duration, guint64 offset, guint64 size)
{
  GstMediaSegment *segment;

  segment = g_new0 (GstMediaSegment, 1);
  segment->url = g_strdup (url);
  segment->index = index;
  segment->start_ts = start_ts;
  segment->duration = duration;
  segment->offset = offset;
  segment->size = size;
  return segment;
}

void
gst_media_segment_free (GstMediaSegment * segment)
{
  g_return_if_fail (segment != NULL);

  g_free (segment->url);
  g_free (segment);
}
