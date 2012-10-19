/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstsegmentinfo.c:
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

#include "gstsegmentlist.h"
#include "gstxmlhelper.h"

GstSegmentList *
gst_segment_list_new (guint fragment_duration, gboolean use_ranges)
{
  GstSegmentList *info;

  info = g_new0 (GstSegmentList, 1);
  info->duration = 0;
  info->start_index = 0;
  info->base_urls = NULL;
  info->init_segment = NULL;
  info->segments = NULL;
  info->use_ranges = use_ranges;
  info->fragment_duration = fragment_duration;

  return info;
}

void
gst_segment_list_free (GstSegmentList * info)
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
gst_segment_list_add_media_segment (GstSegmentList * info,
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
gst_segment_list_set_init_segment (GstSegmentList * info,
    GstMediaSegment * segment)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (segment != NULL);

  if (info->init_segment != NULL)
    gst_media_segment_free (info->init_segment);

  info->init_segment = segment;
}

void
gst_segment_list_add_base_url (GstSegmentList * info, gchar * url)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (url != NULL);

  info->base_urls = g_list_append (info->base_urls, g_strdup (url));
}

static gboolean
gst_media_presentation_write_segment_url (xmlTextWriterPtr writer,
    const gchar * name, const gchar * url_attr, const gchar * range_attr,
    GstMediaSegment * segment, gboolean use_ranges)
{
  if (!gst_media_presentation_start_element (writer, name))
    return FALSE;

  if (!gst_media_presentation_write_string_attribute (writer, url_attr,
          segment->url))
    return FALSE;

  if (use_ranges) {
    gboolean ret;
    gchar *range;

    range = g_strdup_printf ("%" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT,
        segment->offset, segment->offset + segment->size - 1);

    ret = gst_media_presentation_write_string_attribute (writer, range_attr,
        range);
    g_free (range);
    if (!ret)
      return FALSE;
  }

  if (!gst_media_presentation_end_element (writer))
    return FALSE;
  return TRUE;
}

gint32
gst_segment_list_get_average_bitrate (GstSegmentList * info)
{
  GList *tmp;
  guint64 size = 0;

  if (info->duration == 0)
    return 0;

  tmp = g_list_first (info->segments);
  while (tmp != NULL) {
    size += ((GstMediaSegment *) tmp->data)->size;
    tmp = g_list_next (tmp);
  }

  return GST_ROUND_UP_64 (size / (info->duration / GST_SECOND));
}

gboolean
gst_segment_list_render (GstSegmentList * info, xmlTextWriterPtr writer)
{
  GList *tmp;

  tmp = g_list_first (info->segments);
  if (tmp == NULL)
    return TRUE;

  /* Start SegmentList */
  if (!gst_media_presentation_start_element (writer, "SegmentList"))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "timescale",
          1000))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "duration",
          info->fragment_duration / GST_MSECOND))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "start_index",
          info->start_index))
    return FALSE;

  /* Add BaseURL's */
  if (!gst_media_presentation_write_base_urls (writer, info->base_urls))
    return FALSE;

  /* Write initialization segment */
  if (info->init_segment != NULL) {
    if (!gst_media_presentation_write_segment_url (writer, "Initialization",
            "sourceURL", "range", info->init_segment, info->use_ranges))
      return FALSE;
  }

  tmp = g_list_first (info->segments);
  while (tmp != NULL) {
    GstMediaSegment *segment = (GstMediaSegment *) tmp->data;

    if (!gst_media_presentation_write_segment_url (writer, "SegmentURL",
            "media", "mediaRange", segment, info->use_ranges))
      return FALSE;
    tmp = g_list_next (tmp);
  }

  /* End SegmentList */
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
