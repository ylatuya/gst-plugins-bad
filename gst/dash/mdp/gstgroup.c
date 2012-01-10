/*
 * GStreamer
 * Copyright (C) 2011 Flumotion S.L
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstgroup.c:
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

#include "gstgroup.h"
#include "gstrepresentation.h"
#include "gstmediacommon.h"
#include "gstxmlhelper.h"

GstGroup *
gst_group_new (gchar * mimeType)
{
  GstGroup *group;

  group = g_new (GstGroup, 1);
  gst_media_common_init ((GstMediaCommon *) group);
  g_free (group->common.mimeType);
  group->common.mimeType = g_strdup (mimeType);
  group->minBandwidth = 0;
  group->maxBandwidth = 0;
  group->minWidth = 0;
  group->maxWidth = 0;
  group->minHeight = 0;
  group->maxHeight = 0;
  group->minFramerate = 0;
  group->maxFramerate = 0;
  group->segmentAlignmentFlag = TRUE;
  group->bitStreamSwitchingFlag = TRUE;
  group->subsegmentAlignment = TRUE;
  group->representations = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) gst_representation_free);

  return group;
}

void
gst_group_free (GstGroup * group)
{
  g_return_if_fail (group != NULL);

  gst_media_common_free (&(group->common));

  g_hash_table_remove_all (group->representations);
}

gboolean
gst_group_add_representation (GstGroup * group, gchar * id,
    GstRepresentation * rep)
{
  g_return_val_if_fail (group != NULL, FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (rep != NULL, FALSE);

  if (g_hash_table_lookup (group->representations, id) != NULL)
    return FALSE;

  g_hash_table_insert (group->representations, id, rep);
  return TRUE;
}

gboolean
gst_group_remove_representation (GstGroup * group, gchar * id)
{
  return g_hash_table_remove (group->representations, id);
}

GstRepresentation *
gst_group_get_representation (GstGroup * group, gchar * id)
{
  g_return_val_if_fail (group != NULL, NULL);

  return (GstRepresentation *) g_hash_table_lookup (group->representations, id);
}

gboolean
gst_group_add_media_segment (GstGroup * group, gchar * id,
    GstMediaSegment * segment)
{
  GstRepresentation *rep;

  g_return_val_if_fail (group != NULL, FALSE);

  rep = gst_group_get_representation (group, id);
  if (rep == NULL)
    return FALSE;

  gst_representation_add_media_segment (rep, segment);

  return TRUE;
}

gboolean
gst_group_set_init_segment (GstGroup * group, gchar * id,
    GstMediaSegment * segment)
{
  GstRepresentation *rep;

  g_return_val_if_fail (group != NULL, FALSE);

  rep = gst_group_get_representation (group, id);
  if (rep == NULL)
    return FALSE;

  gst_representation_set_init_segment (rep, segment);

  return TRUE;
}

guint64
gst_group_get_duration (GstGroup * group)
{
  GHashTableIter iter;
  GstRepresentation *rep;
  gchar *id;
  guint64 duration = 0;

  g_return_val_if_fail (group != NULL, 0);

  /* Iterate over all groups and return the biggest duration */
  g_hash_table_iter_init (&iter, group->representations);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &rep)) {
    duration = MAX (duration, gst_representation_get_duration (rep));
  }

  return duration;
}

gboolean
gst_group_render (GstGroup * group, xmlTextWriterPtr writer)
{
  GHashTableIter iter;
  GstRepresentation *rep;
  gchar *id;

  /* Start Group and write attributes */
  if (!gst_media_presentation_start_element (writer, "Group"))
    return FALSE;
  if (!gst_media_common_render (&group->common, writer))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minBandwidth",
          group->minBandwidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxBandwidth",
          group->maxBandwidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minWidth",
          group->minWidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxWidth",
          group->maxWidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minHeight",
          group->minHeight))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxHeight",
          group->maxHeight))
    return FALSE;
  if (!gst_media_presentation_write_double_attribute (writer, "minFramerate",
          group->minFramerate))
    return FALSE;
  if (!gst_media_presentation_write_double_attribute (writer, "minFramerate",
          group->maxFramerate))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "segmentAlignmentFlag", group->segmentAlignmentFlag))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "subsegmentAlignment", group->subsegmentAlignment))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "bitStreamSwitchingFlag", group->bitStreamSwitchingFlag))
    return FALSE;

  /* Write representations */
  g_hash_table_iter_init (&iter, group->representations);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &rep)) {
    if (!gst_representation_render (rep, writer))
      return FALSE;
  }

  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}
