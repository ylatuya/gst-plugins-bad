/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstperiod.c:
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

#include <gst/gst.h>

#include "gstperiod.h"
#include "gstxmlhelper.h"

GstPeriod *
gst_period_new (guint64 start)
{
  GstPeriod *period;

  period = g_new0 (GstPeriod, 1);
  period->start = GST_CLOCK_TIME_NONE;
  period->duration = GST_CLOCK_TIME_NONE;
  period->minBufferTime = GST_CLOCK_TIME_NONE;
  period->segmentAlignmentFlag = TRUE;
  period->bitstreamSwitchingFlag = TRUE;
  period->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) gst_group_free);
  period->id_to_group = g_hash_table_new (g_str_hash, g_str_equal);
  /* period->representation = NULL; */

  return period;
}

void
gst_period_free (GstPeriod * period)
{
  g_return_if_fail (period != NULL);

  if (period->groups != NULL) {
    g_hash_table_destroy (period->groups);
    period->groups = NULL;
  }

  if (period->id_to_group != NULL) {
    g_hash_table_destroy (period->id_to_group);
    period->id_to_group = NULL;
  }
  /*
     g_list_foreach(period->representation, gst_representation_free, NULL);
     g_list_free (period->representation);
     period->representation = NULL;
   */

  g_free (period);
}

gboolean
gst_period_add_representation (GstPeriod * period, GstRepresentation * rep)
{
  /* Groups define a set of alternate representations of the same media.
   * A media representation can consist of several media type (mpegts, isoff, webmf, etc...)
   * which are grouped in a period.
   */
  GstGroup *group;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (rep != NULL, FALSE);

  /* Get the group for the representation's mime type or create a new one */
  group =
      (GstGroup *) g_hash_table_lookup (period->groups, rep->common.mimeType);
  if (group == NULL) {
    group = gst_group_new (rep->common.mimeType);
    g_hash_table_insert (period->groups, g_strdup (rep->common.mimeType),
        group);
  }

  g_hash_table_insert (period->id_to_group, rep->id, group);
  return gst_group_add_representation (group, rep);
}

gboolean
gst_period_add_media_segment (GstPeriod * period, gchar * id,
    GstMediaSegment * media_segment)
{
  GstGroup *group;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (media_segment != NULL, FALSE);

  group = g_hash_table_lookup (period->id_to_group, id);

  if (group == NULL) {
    GST_WARNING ("Couldn't find a any group for the given id: s%", id);
    return FALSE;
  }

  return gst_group_add_media_segment (group, id, media_segment);
}

gboolean
gst_period_set_init_segment (GstPeriod * period, gchar * id,
    GstMediaSegment * init_segment)
{
  GstGroup *group;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (init_segment != NULL, FALSE);

  group = g_hash_table_lookup (period->id_to_group, id);

  if (group == NULL) {
    GST_WARNING ("Couldn't find a any group for the given id: s%", id);
    return FALSE;
  }

  return gst_group_set_init_segment (group, id, init_segment);
}

guint64
gst_period_get_duration (GstPeriod * period)
{
  GHashTableIter iter;
  GstGroup *group;
  gchar *id;
  guint64 duration = 0;

  /* Iterate over all groups and return the biggest duration */
  g_hash_table_iter_init (&iter, period->groups);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &group)) {
    duration = MAX (duration, gst_group_get_duration (group));
  }

  return duration;
}

gboolean
gst_period_render (GstPeriod * period, xmlTextWriterPtr writer)
{
  GHashTableIter iter;
  GstGroup *group;
  gchar *id;

  /* Start Period */
  if (!gst_media_presentation_start_element (writer, "Period"))
    return FALSE;

  /* Write Period attributes */
  if (!gst_media_presentation_write_time_seconds_attribute (writer, "start",
          period->start))
    return FALSE;
  if (!gst_media_presentation_write_time_seconds_attribute (writer, "duration",
          period->duration))
    return FALSE;
  if (!gst_media_presentation_write_time_seconds_attribute (writer,
          "minBufferTime", period->minBufferTime))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "segmentAlignmentFlag", period->segmentAlignmentFlag))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "bitstreamSwitchingFlag", period->bitstreamSwitchingFlag))
    return FALSE;

  /* Add groups */
  g_hash_table_iter_init (&iter, period->groups);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &group)) {
    if (!gst_group_render (group, writer))
      return FALSE;
  }

  /* End period */
  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}
