/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
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
#define PROFILE_ISOFF_ONDEMAND "urn:mpeg:dash:profile:isoff-on-demand:2011"
#define PROFILE_MPEGTS_ONDEMAND "urn:mpeg:dash:profile:mp2t-on-demand:2011"
#define PROFILE_ISOFF_LIVE "urn:mpeg:dash:profile:isoff-live:2011"
#define PROFILE_MPEGTS_LIVE "urn:mpeg:dash:profile:mp2t-live:2011"
#define TYPE_ONDEMAND "static"
#define TYPE_LIVE "dynamic"
#define MIMETYPE_MP4 "mp4"
#define MIMETYPE_MPEGTS "MP2T"
#define MIMETYPE_MPEGTS_INFO "m2t"

#define MPD_XML_INDENT 4

#define ACTIVE_PERIOD(mpd) (g_list_last(mpd->periods) != NULL ?\
    (GstPeriod*)g_list_last(mpd->periods)->data : NULL)

GstMediaPresentation *
gst_media_presentation_new (MediaPresentationType type, gboolean use_ranges,
    guint64 min_buffer_time)
{
  GstMediaPresentation *mpd;

  mpd = g_new0 (GstMediaPresentation, 1);
  mpd->availabilityStartTime = GST_CLOCK_TIME_NONE;
  mpd->availabilityEndTime = GST_CLOCK_TIME_NONE;
  mpd->mediaPresentationDuration = GST_CLOCK_TIME_NONE;
  mpd->minimumUpdatePeriodMPD = GST_CLOCK_TIME_NONE;
  mpd->minBufferTime = min_buffer_time;
  mpd->use_ranges = use_ranges;
  mpd->type = type;
  mpd->profiles = NULL;
  mpd->periods = NULL;
  mpd->baseUrls = NULL;

  return mpd;
}

void
gst_media_presentation_free (GstMediaPresentation * mpd)
{
  g_return_if_fail (mpd != NULL);

  if (mpd->profiles != NULL) {
    g_list_foreach (mpd->profiles, (GFunc) g_free, NULL);
    g_list_free (mpd->profiles);
  }

  if (mpd->periods != NULL) {
    g_list_foreach (mpd->periods, (GFunc) gst_period_free, NULL);
    g_list_free (mpd->periods);
  }

  if (mpd->baseUrls != NULL) {
    g_list_foreach (mpd->baseUrls, (GFunc) g_free, NULL);
    g_list_free (mpd->baseUrls);
  }

  g_free (mpd);
}

void
gst_media_presentation_clear (GstMediaPresentation * mpd)
{
  g_return_if_fail (mpd != NULL);

  if (mpd->periods != NULL)
    g_list_foreach (mpd->periods, (GFunc) gst_period_free, NULL);
}

static gchar *
gst_media_presentation_format_mime_type (GstMediaPresentation * mpd,
    StreamType type, const gchar * mimeType)
{
  const gchar *mpd_mime_type;

  if (!g_strcmp0 (mimeType, "video/quicktime"))
    mpd_mime_type = MIMETYPE_MP4;
  else if (!g_strcmp0 (mimeType, "video/mpegts"))
    mpd_mime_type = MIMETYPE_MPEGTS;
  else
    return NULL;

  switch (type) {
    case STREAM_TYPE_AUDIO:
      return g_strdup_printf ("%s/%s", "audio", mpd_mime_type);
    case STREAM_TYPE_VIDEO:
      return g_strdup_printf ("%s/%s", "video", mpd_mime_type);
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static void
gst_media_presentation_add_profile (GstMediaPresentation * mdp,
    const gchar * mime_type)
{
  const gchar *profile = NULL;

  if (!g_strcmp0 (mime_type, "video/quicktime")) {
    if (mdp->type == MEDIA_PRESENTATION_TYPE_LIVE) {
      profile = PROFILE_ISOFF_LIVE;
    } else {
      profile = PROFILE_ISOFF_ONDEMAND;
    }
  } else if (!g_strcmp0 (mime_type, "video/mpegts")) {
    if (mdp->type == MEDIA_PRESENTATION_TYPE_LIVE) {
      profile = PROFILE_MPEGTS_ONDEMAND;
    } else {
      profile = PROFILE_MPEGTS_LIVE;
    }
  }

  if (profile == NULL)
    return;

  if (g_list_find_custom (mdp->profiles, profile,
          (GCompareFunc) g_strcmp0) == NULL) {
    mdp->profiles = g_list_append (mdp->profiles, g_strdup (profile));
  }
}

gboolean
gst_media_presentation_add_stream (GstMediaPresentation * mpd, StreamType type,
    gchar * id, const gchar * mimeType, guint32 width, guint32 height,
    guint32 parx, guint32 pary, gdouble frameRate, gchar * channels,
    guint32 samplingRate, guint32 bitrate, const gchar * lang,
    guint fragment_duration)
{
  GstPeriod *active_period;
  GstRepresentation *rep;
  gchar *mpd_mime_type;

  g_return_val_if_fail (mpd != NULL, FALSE);

  mpd_mime_type = gst_media_presentation_format_mime_type (mpd, type, mimeType);
  if (mpd_mime_type == NULL)
    return FALSE;

  gst_media_presentation_add_profile (mpd, mimeType);

  /* Get the current period or create a new one if it doesn't exists */
  active_period = ACTIVE_PERIOD (mpd);
  if (G_UNLIKELY (active_period == NULL)) {
    active_period = gst_period_new (0);
    mpd->periods = g_list_append (mpd->periods, active_period);
    GST_DEBUG ("Creating new period for the media presentation");
  }

  /* Add a new presentation to the active period */
  rep =
      gst_representation_new (id, type, mpd_mime_type, width, height, parx,
      pary, frameRate, channels, samplingRate, bitrate, lang, mpd->use_ranges,
      fragment_duration);
  g_free (mpd_mime_type);

  return gst_period_add_representation (active_period, rep);
}

gboolean
gst_media_presentation_remove_stream (GstMediaPresentation * mpd, gchar * id)
{
  /* FIXME: At this moment, we only support one period. In a future, we should
   * support adding/removing new streams dynamically by closing the current
   * period and opening a new one with the new list of representations
   */
  return FALSE;
}

static guint64
gst_media_presentation_get_duration (GstMediaPresentation * mpd)
{
  GList *tmp;
  guint64 duration = 0;

  /* Iterate over all periods */
  if (mpd->periods != NULL) {
    tmp = g_list_first (mpd->periods);

    /* Get the maximum duration from all the segment inside this period */
    while (tmp != NULL) {
      duration += gst_period_get_duration ((GstPeriod *) tmp->data);
      tmp = g_list_next (tmp);
    }
  }

  return duration;
}

gboolean
gst_media_presentation_add_media_segment (GstMediaPresentation * mpd,
    gchar * id, gchar * url, guint index, guint64 start_ts, guint64 duration,
    guint64 offset, guint64 length)
{
  GstPeriod *active_period;
  GstMediaSegment *segment;
  gboolean ret;

  g_return_val_if_fail (mpd != NULL, FALSE);

  active_period = ACTIVE_PERIOD (mpd);
  if (active_period == NULL) {
    GST_WARNING
        ("At least one stream must be added before adding media segments.");
    return FALSE;
  }

  /* Create a new media segment and add it to the current period */
  segment =
      gst_media_segment_new (url, index, start_ts, duration, offset, length);
  ret = gst_period_add_media_segment (active_period, id, segment);

  mpd->mediaPresentationDuration = gst_media_presentation_get_duration (mpd);

  return ret;
}

gboolean
gst_media_presentation_set_init_segment (GstMediaPresentation * mpd, gchar * id,
    gchar * url, guint64 offset, guint64 length)
{
  GstPeriod *active_period;
  GstMediaSegment *segment_info;

  g_return_val_if_fail (mpd != NULL, FALSE);

  active_period = ACTIVE_PERIOD (mpd);
  if (active_period == NULL) {
    GST_WARNING
        ("At least one stream must be added before adding media segments.");
  }

  segment_info = gst_media_segment_new (url, 0, 0, 0, offset, length);
  return gst_period_set_init_segment (active_period, id, segment_info);
}

void
gst_media_presentation_set_base_urls (GstMediaPresentation * mpd,
    GList * base_urls)
{
  if (mpd->baseUrls != NULL) {
    g_list_foreach (mpd->baseUrls, (GFunc) g_free, NULL);
    g_list_free (mpd->baseUrls);
  }
  mpd->baseUrls = base_urls;
}

static const gchar *
gst_media_presentation_render_type (GstMediaPresentation * mpd)
{
  switch (mpd->type) {
    case MEDIA_PRESENTATION_TYPE_ONDEMAND:
      return TYPE_ONDEMAND;
    default:
      return NULL;
  }
}

gchar *
gst_media_presentation_render (GstMediaPresentation * mpd)
{
  xmlTextWriterPtr writer = NULL;
  xmlBufferPtr buf = NULL;
  gchar *mpd_str = NULL;

  /* Create a new XML buffer, to which the XML document will be
   * written */
  buf = xmlBufferCreate ();
  if (buf == NULL) {
    GST_WARNING_OBJECT (mpd, "Error creating the xml buffer");
    goto error;
  }

  /* Create a new XmlWriter for memory, with no compression.
   * Remark: there is no compression for this kind of xmlTextWriter */
  writer = xmlNewTextWriterMemory (buf, 0);
  if (writer == NULL) {
    GST_WARNING_OBJECT (mpd, "Error creating the xml writter");
    goto error;
  }

  /* Start the document */
  if (xmlTextWriterStartDocument (writer, NULL, "UTF-8", NULL) < 0) {
    GST_WARNING_OBJECT (mpd, "Error at xmlTextWriterStartDocument");
    goto error;
  }

  /* Set xml indentation */
  xmlTextWriterSetIndent (writer, MPD_XML_INDENT);

  /* Start root element named "MPD" */
  if (!gst_media_presentation_start_element (writer, "MPD"))
    goto error;

  /* Write MPD attributes */
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
          mpd->profiles))
    goto error;

  if (!gst_media_presentation_write_string_attribute (writer, "type",
          gst_media_presentation_render_type (mpd)))
    goto error;

  if (!gst_media_presentation_write_time_attribute (writer,
          "mediaPresentationDuration", mpd->mediaPresentationDuration))
    goto error;

  if (!gst_media_presentation_write_time_seconds_attribute (writer,
          "minBufferTime", mpd->minBufferTime))
    goto error;

  if (!gst_media_presentation_write_time_seconds_attribute (writer,
          "minimumUpdatePeriodMPD", mpd->minimumUpdatePeriodMPD))
    goto error;

  /* FIXME: To be implemented
     if (!gst_media_presentation_write_date_attribute (writer,
     "availabilityStartTime", mpd->availabilityStartTime))
     goto error;

     if (!gst_media_presentation_write_date_attribute (writer,
     "availabilityEndTime", mpd->availabilityEndTime))
     goto error;
   */

  /* BaseUrl */
  if (!gst_media_presentation_write_base_urls (writer, mpd->baseUrls))
    return FALSE;

  /* Period */
  if (mpd->periods != NULL) {
    GList *tmp = g_list_first (mpd->periods);

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
    GST_WARNING_OBJECT (mpd, "Error at xmlTextWriterEndDocument");
    goto error;
  }

  mpd_str = g_strdup ((gchar *) buf->content);
  goto exit;

error:
  {
    GST_ERROR_OBJECT (mpd, "Error rendering media presentation");
  }

exit:
  {
    if (writer != NULL)
      xmlFreeTextWriter (writer);
    if (buf != NULL)
      xmlBufferFree (buf);
    return mpd_str;
  }
}
