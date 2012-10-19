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
  period->start = start;
  period->duration = GST_CLOCK_TIME_NONE;
  period->minBufferTime = GST_CLOCK_TIME_NONE;
  period->segmentAlignment = TRUE;
  period->bitstreamSwitching = TRUE;
  period->adaptation_sets = g_hash_table_new_full (g_str_hash, g_str_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) gst_adaptation_set_free);
  period->id_to_adaptation_set = g_hash_table_new (g_str_hash, g_str_equal);
  /* period->representation = NULL; */

  return period;
}

void
gst_period_free (GstPeriod * period)
{
  g_return_if_fail (period != NULL);

  if (period->adaptation_sets != NULL) {
    g_hash_table_destroy (period->adaptation_sets);
    period->adaptation_sets = NULL;
  }

  if (period->id_to_adaptation_set != NULL) {
    g_hash_table_destroy (period->id_to_adaptation_set);
    period->id_to_adaptation_set = NULL;
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
  /* AdaptationSets define a set of alternate representations of the same media.
   * A media representation can consist of several media type (mpegts, isoff, webmf, etc...)
   * which are AdaptationSet in a period.
   */
  GstAdaptationSet *adaptation_set;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (rep != NULL, FALSE);

  /* Get the AdaptationSet for the representation's mime type or create a new one */
  adaptation_set =
      (GstAdaptationSet *) g_hash_table_lookup (period->adaptation_sets,
      rep->common.mimeType);
  if (adaptation_set == NULL) {
    adaptation_set = gst_adaptation_set_new (rep->common.stream_type,
        rep->common.mimeType);
    g_hash_table_insert (period->adaptation_sets,
        g_strdup (rep->common.mimeType), adaptation_set);
  }

  g_hash_table_insert (period->id_to_adaptation_set, rep->id, adaptation_set);
  return gst_adaptation_set_add_representation (adaptation_set, rep);
}

gboolean
gst_period_add_media_segment (GstPeriod * period, gchar * id,
    GstMediaSegment * media_segment)
{
  GstAdaptationSet *adaptation_set;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (media_segment != NULL, FALSE);

  adaptation_set = g_hash_table_lookup (period->id_to_adaptation_set, id);

  if (adaptation_set == NULL) {
    GST_WARNING ("Couldn't find a any adaptation_set for the given id: %s", id);
    return FALSE;
  }

  period->duration = gst_period_get_duration (period);
  return gst_adaptation_set_add_media_segment (adaptation_set, id,
      media_segment);
}

gboolean
gst_period_set_init_segment (GstPeriod * period, gchar * id,
    GstMediaSegment * init_segment)
{
  GstAdaptationSet *adaptation_set;

  g_return_val_if_fail (period != NULL, FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (init_segment != NULL, FALSE);

  adaptation_set = g_hash_table_lookup (period->id_to_adaptation_set, id);

  if (adaptation_set == NULL) {
    GST_WARNING ("Couldn't find a any adaptation_set for the given id: %s", id);
    return FALSE;
  }

  return gst_adaptation_set_set_init_segment (adaptation_set, id, init_segment);
}

guint64
gst_period_get_duration (GstPeriod * period)
{
  GHashTableIter iter;
  GstAdaptationSet *adaptation_set;
  gchar *id;
  guint64 duration = 0;

  /* Iterate over all adaptation_sets and return the biggest duration */
  g_hash_table_iter_init (&iter, period->adaptation_sets);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &adaptation_set)) {
    duration = MAX (duration, gst_adaptation_set_get_duration (adaptation_set));
  }

  return duration;
}

gboolean
gst_period_render (GstPeriod * period, xmlTextWriterPtr writer)
{
  GHashTableIter iter;
  GstAdaptationSet *adaptation_set;
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
          "segmentAlignment", period->segmentAlignment))
    return FALSE;
  if (!gst_media_presentation_write_bool_attribute (writer,
          "bitstreamSwitching", period->bitstreamSwitching))
    return FALSE;

  /* Add adaptation_sets */
  g_hash_table_iter_init (&iter, period->adaptation_sets);
  while (g_hash_table_iter_next (&iter, (void *) &id, (void *) &adaptation_set)) {
    if (!gst_adaptation_set_render (adaptation_set, writer))
      return FALSE;
  }

  /* End period */
  if (!gst_media_presentation_end_element (writer))
    return FALSE;

  return TRUE;
}
