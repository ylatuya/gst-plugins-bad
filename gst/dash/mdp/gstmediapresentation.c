/*
 * GStreamer
 * Copyright (C) 2011 Flumotion S.L.
 * Copyright (C) 2011 Andoni Morales Alastruey
 *
 * gstmediapresentation.c:
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
#include "gstmediapresentation.h"
#include "gstxmlhelper.h"

#define SCHEMA_URL "http://www.w3.org/2001/XMLSchema"
#define SCHEMA_2011 "urn:mpeg:mpegB:schema:DASH:MPD:DIS2011"
#define PROFILE_ISOFF_ONDEMAND "urn:mpeg:mpegB:profile:dash:isoff-basic-on-demand:cm"
#define PROFILE_MPEGTS_ONDEMAND "urn:mpeg:mpegB:profile:dash:mpegts-basic-on-demand:cm"
#define ON_DEMAND_PROFILE "urn:mpeg:mpegB:profile:dash:isoff-basic-on-demand:cm"
#define TYPE_ONDEMAND "OnDemand"
#define TYPE_LIVE "Live"
#define MIMETYPE_MP4 "mp4"
#define MIMETYPE_MPEGTS "MP2T"
#define MIMETYPE_MPEGTS_INFO "m2t"

#define ACTIVE_PERIOD(mdp) (g_list_last(mdp->periods) != NULL ?\
    (GstPeriod*)g_list_last(mdp->periods)->data : NULL)

GstMediaPresentation *
gst_media_presentation_new (MediaPresentationType type)
{
  GstMediaPresentation *mdp;

  mdp = g_new0 (GstMediaPresentation, 1);
  mdp->availabilityStartTime = GST_CLOCK_TIME_NONE;
  mdp->availabilityEndTime = GST_CLOCK_TIME_NONE;
  mdp->mediaPresentationDuration = GST_CLOCK_TIME_NONE;
  mdp->minimumUpdatePeriodMDP = GST_CLOCK_TIME_NONE;
  mdp->minBufferTime = GST_CLOCK_TIME_NONE;
  mdp->type = type;
  /* FIXME: Pass profiles as an argument to the constructor */
  mdp->profiles = NULL;
  mdp->profiles = g_list_append (mdp->profiles, g_strdup (ON_DEMAND_PROFILE));
  mdp->periods = NULL;
  mdp->baseUrls = NULL;

  return mdp;
}

void
gst_media_presentation_free (GstMediaPresentation * mdp)
{
  g_return_if_fail (mdp != NULL);

  if (mdp->profiles != NULL)
    g_list_free (mdp->profiles);

  if (mdp->periods != NULL) {
    g_list_foreach (mdp->periods, (GFunc) gst_period_free, NULL);
    g_list_free (mdp->periods);
  }

  if (mdp->baseUrls != NULL) {
    g_list_foreach (mdp->baseUrls, (GFunc) g_free, NULL);
    g_list_free (mdp->baseUrls);
  }

  g_free (mdp);
}

void
gst_media_presentation_clear (GstMediaPresentation * mdp)
{
  g_return_if_fail (mdp != NULL);

  if (mdp->periods != NULL)
    g_list_foreach (mdp->periods, (GFunc) gst_period_free, NULL);
}

static gchar *
gst_media_presentation_format_mime_type (GstMediaPresentation * mdp,
    StreamType type, const gchar * mimeType)
{
  const gchar *mdp_mime_type;

  if (!g_strcmp0 (mimeType, "video/quicktime"))
    mdp_mime_type = MIMETYPE_MP4;
  else if (!g_strcmp0 (mimeType, "video/mpegts"))
    mdp_mime_type = MIMETYPE_MPEGTS;
  else
    return NULL;

  switch (type) {
    case STREAM_TYPE_AUDIO:
      return g_strdup_printf ("%s/%s", "audio", mdp_mime_type);
    case STREAM_TYPE_VIDEO:
      return g_strdup_printf ("%s/%s", "video", mdp_mime_type);
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

gboolean
gst_media_presentation_add_stream (GstMediaPresentation * mdp, StreamType type,
    gchar * id, const gchar * mimeType, guint32 width, guint32 height,
    guint32 parx, guint32 pary, gdouble frameRate, gchar * channels,
    guint32 samplingRate)
{
  GstPeriod *active_period;
  GstRepresentation *rep;
  gchar *mdp_mime_type;

  g_return_val_if_fail (mdp != NULL, FALSE);

  mdp_mime_type = gst_media_presentation_format_mime_type (mdp, type, mimeType);
  if (mdp_mime_type == NULL)
    return FALSE;

  /* Get the current period or create a new one if it doesn't exists */
  active_period = ACTIVE_PERIOD (mdp);
  if (G_UNLIKELY (active_period == NULL)) {
    active_period = gst_period_new (GST_CLOCK_TIME_NONE);
    mdp->periods = g_list_append (mdp->periods, active_period);
    GST_DEBUG ("Creating new period for the media presentation");
  }

  /* Add a new presentation to the active period */
  rep =
      gst_representation_new (id, (gchar *) mdp_mime_type, width, height, parx,
      pary, frameRate, channels, samplingRate);
  return gst_period_add_representation (active_period, id, rep);
}

gboolean
gst_media_presentation_remove_stream (GstMediaPresentation * mdp, gchar * id)
{
  /* FIXME: At this moment, we only support one period. In a future, we should
   * support adding/removing new streams dynamically by closing the current
   * period and opening a new one with the new list of representations
   */
  return FALSE;
}

gboolean
gst_media_presentation_add_media_segment (GstMediaPresentation * mdp,
    gchar * id, gchar * url, guint index, guint64 start_ts, guint64 duration,
    guint64 offset, guint64 length)
{
  GstPeriod *active_period;
  GstMediaSegment *segment;

  g_return_val_if_fail (mdp != NULL, FALSE);

  active_period = ACTIVE_PERIOD (mdp);
  if (active_period == NULL) {
    GST_WARNING
        ("At least one stream must be added before adding media segments.");
  }

  /* Create a new media segment and add it to the current period */
  segment =
      gst_media_segment_new (url, index, start_ts, duration, offset, length);
  return gst_period_add_media_segment (active_period, id, segment);
}

gboolean
gst_media_presentation_set_init_segment (GstMediaPresentation * mdp, gchar * id,
    gchar * url, guint64 offset, guint64 length)
{
  GstPeriod *active_period;
  GstMediaSegment *segment_info;

  g_return_val_if_fail (mdp != NULL, FALSE);

  active_period = ACTIVE_PERIOD (mdp);
  if (active_period == NULL) {
    GST_WARNING
        ("At least one stream must be added before adding media segments.");
  }

  segment_info = gst_media_segment_new (url, 0, 0, 0, offset, length);
  return gst_period_set_init_segment (active_period, id, segment_info);
}

gchar *
gst_media_presentation_render (GstMediaPresentation * mdp)
{
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  gchar *mdp_str = NULL;

  /* Create a new XML buffer, to which the XML document will be
   * written */
  buf = xmlBufferCreate ();
  if (buf == NULL) {
    GST_WARNING_OBJECT (mdp, "Error creating the xml buffer");
    goto error;
  }

  /* Create a new XmlWriter for memory, with no compression.
   * Remark: there is no compression for this kind of xmlTextWriter */
  writer = xmlNewTextWriterMemory (buf, 0);
  if (writer == NULL) {
    GST_WARNING_OBJECT (mdp, "Error creating the xml writter");
    goto error;
  }

  /* Start the document */
  if (xmlTextWriterStartDocument (writer, NULL, "UTF-8", NULL) < 0) {
    GST_WARNING_OBJECT (mdp, "Error at xmlTextWriterStartDocument");
    goto error;
  }

  /* Start root element named "MPD" */
  if (!gst_media_presentation_start_element (writer, "MDP"))
    goto error;

  /* Write MDP attributes */
  if (!gst_media_presentation_write_string_attribute (writer, "xmlns:xsi",
          SCHEMA_URL))
    goto error;

  if (!gst_media_presentation_write_string_attribute (writer, "xmlns",
          SCHEMA_2011))
    goto error;

  if (!gst_media_presentation_write_string_attribute (writer,
          "xsi:schemaLocation", SCHEMA_2011))
    goto error;

  if (!gst_media_presentation_write_string_list_attribute (writer, "profiles",
          mdp->profiles))
    goto error;

  /* FIXME: Only ondemand is supported for now */
  if (!gst_media_presentation_write_string_attribute (writer, "type",
          TYPE_ONDEMAND))
    goto error;

  if (!gst_media_presentation_write_time_attribute (writer,
          "mediaPresentationDuration", mdp->mediaPresentationDuration))
    goto error;

  if (!gst_media_presentation_write_time_attribute (writer,
          "minBufferTime", mdp->minBufferTime))
    goto error;

  if (!gst_media_presentation_write_time_attribute (writer,
          "minimumUpdatePeriodMDP", mdp->minimumUpdatePeriodMDP))
    goto error;

  /* FIXME: To be implemented
     if (!gst_media_presentation_write_date_attribute (writer,
     "availabilityStartTime", mdp->availabilityStartTime))
     goto error;

     if (!gst_media_presentation_write_date_attribute (writer,
     "availabilityEndTime", mdp->availabilityEndTime))
     goto error;
   */

  if (mdp->periods != NULL) {
    GList *tmp = g_list_first (mdp->periods);

    while (tmp != NULL) {
      if (!gst_period_render ((GstPeriod *) tmp->data, writer))
        goto error;
      tmp = g_list_next (tmp);
    }
  }

  /* Close MPD */
  if (!gst_media_presentation_end_element (writer))
    goto error;

  /* End document */
  if (xmlTextWriterEndDocument (writer) < 0) {
    GST_WARNING_OBJECT (mdp, "Error at xmlTextWriterEndDocument");
    goto error;
  }

  mdp_str = g_strdup ((gchar *) buf->content);
  goto exit;

error:
  {
    GST_ERROR_OBJECT (mdp, "Error rendering media presentation");
  }

exit:
  {
    if (writer != NULL)
      xmlFreeTextWriter (writer);
    if (buf != NULL)
      xmlBufferFree (buf);
    return mdp_str;
  }
}
