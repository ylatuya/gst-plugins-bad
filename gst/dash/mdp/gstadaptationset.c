/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstadaptation_set.c:
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

#include "gstadaptationset.h"
#include "gstrepresentation.h"
#include "gstmediacommon.h"
#include "gstxmlhelper.h"

GstAdaptationSet *
gst_adaptation_set_new (StreamType stream_type, gchar * mimeType)
{
  GstAdaptationSet *adaptation_set;

  adaptation_set = g_new (GstAdaptationSet, 1);
  gst_media_common_init ((GstMediaCommon *) adaptation_set);
  g_free (adaptation_set->common.mimeType);
  adaptation_set->common.mimeType = g_strdup (mimeType);
  adaptation_set->common.stream_type = stream_type;
  adaptation_set->minBandwidth = G_MAXUINT32;
  adaptation_set->maxBandwidth = 0;
  adaptation_set->minWidth = G_MAXUINT32;
  adaptation_set->maxWidth = 0;
  adaptation_set->minHeight = G_MAXUINT32;
  adaptation_set->maxHeight = 0;
  adaptation_set->minFramerate = G_MAXUINT32;
  adaptation_set->maxFramerate = 0;
  adaptation_set->segmentAlignment = TRUE;
  adaptation_set->bitStreamSwitching = TRUE;
  adaptation_set->subsegmentAlignment = TRUE;
  adaptation_set->representations =
      g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) gst_representation_free);

  return adaptation_set;
}

void
gst_adaptation_set_free (GstAdaptationSet * adaptation_set)
{
  g_return_if_fail (adaptation_set != NULL);

  gst_media_common_free (&(adaptation_set->common));
  g_hash_table_destroy (adaptation_set->representations);
  g_free (adaptation_set);
}

static void
_update_max_min_values (GstAdaptationSet * set)
{
  GHashTableIter iter;
  gpointer key;
  GstRepresentation *rep;

  g_hash_table_iter_init (&iter, set->representations);
  while (g_hash_table_iter_next (&iter, &key, (gpointer) & rep)) {
    if (rep->common.width > set->maxWidth) {
      set->maxWidth = rep->common.width;
    }
    if (rep->common.height > set->maxHeight) {
      set->maxHeight = rep->common.height;
    }
    if (rep->common.frameRate > set->maxFramerate) {
      set->maxFramerate = rep->common.frameRate;
    }
    if (rep->bandwidth > set->maxBandwidth) {
      set->maxBandwidth = rep->bandwidth;
    }
    if (rep->common.width < set->minWidth) {
      set->minWidth = rep->common.width;
    }
    if (rep->common.height < set->minHeight) {
      set->minHeight = rep->common.height;
    }
    if (rep->common.frameRate < set->minFramerate) {
      set->minFramerate = rep->common.frameRate;
    }
    if (rep->bandwidth < set->minBandwidth) {
      set->minBandwidth = rep->bandwidth;
    }
  }
}

gboolean
gst_adaptation_set_add_representation (GstAdaptationSet * adaptation_set,
    GstRepresentation * rep)
{
  g_return_val_if_fail (adaptation_set != NULL, FALSE);
  g_return_val_if_fail (rep != NULL, FALSE);

  if (g_hash_table_lookup (adaptation_set->representations, rep->id) != NULL)
    return FALSE;

  g_hash_table_insert (adaptation_set->representations, rep->id, rep);
  _update_max_min_values (adaptation_set);
  return TRUE;
}

gboolean
gst_adaptation_set_remove_representation (GstAdaptationSet * adaptation_set,
    gchar * id)
{
  gboolean ret;
  ret = g_hash_table_remove (adaptation_set->representations, id);
  if (ret)
    _update_max_min_values (adaptation_set);
  return ret;
}

GstRepresentation *
gst_adaptation_set_get_representation (GstAdaptationSet * adaptation_set,
    gchar * id)
{
  g_return_val_if_fail (adaptation_set != NULL, NULL);

  return (GstRepresentation *)
      g_hash_table_lookup (adaptation_set->representations, id);
}

gboolean
gst_adaptation_set_add_media_segment (GstAdaptationSet * adaptation_set,
    gchar * id, GstMediaSegment * segment)
{
  GstRepresentation *rep;

  g_return_val_if_fail (adaptation_set != NULL, FALSE);

  rep = gst_adaptation_set_get_representation (adaptation_set, id);
  if (rep == NULL)
    return FALSE;

  gst_representation_add_media_segment (rep, segment);

  return TRUE;
}

gboolean
gst_adaptation_set_set_init_segment (GstAdaptationSet * adaptation_set,
    gchar * id, GstMediaSegment * segment)
{
  GstRepresentation *rep;

  g_return_val_if_fail (adaptation_set != NULL, FALSE);

  rep = gst_adaptation_set_get_representation (adaptation_set, id);
  if (rep == NULL)
    return FALSE;

  gst_representation_set_init_segment (rep, segment);

  return TRUE;
}

guint64
gst_adaptation_set_get_duration (GstAdaptationSet * adaptation_set)
{
  GHashTableIter iter;
  GstRepresentation *rep;
  gchar *id;
  guint64 duration = 0;

  g_return_val_if_fail (adaptation_set != NULL, 0);

  /* Iterate over all adaptation_sets and return the biggest duration */
  g_hash_table_iter_init (&iter, adaptation_set->representations);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &rep)) {
    duration = MAX (duration, gst_representation_get_duration (rep));
  }

  return duration;
}

gboolean
gst_adaptation_set_render (GstAdaptationSet * adaptation_set,
    xmlTextWriterPtr writer)
{
  GHashTableIter iter;
  GstRepresentation *rep;
  gchar *id;

  /* Start AdaptationSet and write attributes */
  if (!gst_media_presentation_start_element (writer, "AdaptationSet"))
    return FALSE;
  if (!gst_media_common_render (&adaptation_set->common, writer))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minBandwidth",
          adaptation_set->minBandwidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxBandwidth",
          adaptation_set->maxBandwidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minWidth",
          adaptation_set->minWidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxWidth",
          adaptation_set->maxWidth))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "minHeight",
          adaptation_set->minHeight))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "maxHeight",
          adaptation_set->maxHeight))
    return FALSE;
  if (!gst_media_presentation_write_double_attribute (writer, "minFramerate",
          adaptation_set->minFramerate))
    return FALSE;
  if (!gst_media_presentation_write_double_attribute (writer, "minFramerate",
          adaptation_set->maxFramerate))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "segmentAlignment", adaptation_set->segmentAlignment))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "subsegmentAlignment", adaptation_set->subsegmentAlignment))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "bitstreamSwitching", adaptation_set->bitStreamSwitching))
    return FALSE;

  switch (adaptation_set->common.stream_type) {
    case STREAM_TYPE_VIDEO:{
      if (!gst_media_presentation_start_element (writer, "ContentComponent"))
        return FALSE;
      if (!gst_media_presentation_write_uint32_attribute (writer, "id", 1))
        return FALSE;
      if (!gst_media_presentation_write_string_attribute (writer,
              "contentType", "video"))
        return FALSE;
      if (!gst_media_presentation_end_element (writer))
        return FALSE;
      break;
    }
    case STREAM_TYPE_AUDIO:{
      if (!gst_media_presentation_start_element (writer, "ContentComponent"))
        return FALSE;
      if (!gst_media_presentation_write_uint32_attribute (writer, "id", 1))
        return FALSE;
      if (!gst_media_presentation_write_string_attribute (writer,
              "contentType", "audio"))
        return FALSE;
      if (!gst_media_presentation_end_element (writer))
        return FALSE;
      break;
    }
    case STREAM_TYPE_SUBTITLE:{
      if (!gst_media_presentation_start_element (writer, "Role"))
        return FALSE;
      if (!gst_media_presentation_write_string_attribute (writer, "schemeIdUri",
              "urn:mpeg:dash:role:2011"))
        return FALSE;
      if (!gst_media_presentation_write_string_attribute (writer,
              "value", "subtitle"))
        return FALSE;
      if (!gst_media_presentation_end_element (writer))
        return FALSE;
      break;
    }
    default:
      break;
  }

  /* Write representations */
  g_hash_table_iter_init (&iter, adaptation_set->representations);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &rep)) {
    if (!gst_representation_render (rep, writer))
      return FALSE;
  }

  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}
