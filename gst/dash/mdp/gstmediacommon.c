/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstmediacommon.c:
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

#include <glib.h>
#include "gstmediacommon.h"
#include "gstxmlhelper.h"


void
gst_media_common_init (GstMediaCommon * common)
{
  g_return_if_fail (common != NULL);

  common->width = 0;
  common->height = 0;
  common->parx = 0;
  common->pary = 0;
  common->frameRate = 0;
  common->maximumRAPPeriod = 0;
  common->lang = NULL;
  common->numberOfChannels = NULL;
  common->samplingRate = NULL;
  common->mimeType = NULL;
  common->group = 0;
  common->stream_type = STREAM_TYPE_UNKNOWN;
}

void
gst_media_common_free (GstMediaCommon * common)
{
  g_return_if_fail (common != NULL);

  g_list_foreach (common->lang, (GFunc) g_free, NULL);
  g_list_free (common->lang);

  g_list_foreach (common->numberOfChannels, (GFunc) g_free, NULL);
  g_list_free (common->numberOfChannels);

  g_list_foreach (common->samplingRate, (GFunc) g_free, NULL);
  g_list_free (common->samplingRate);

  g_free (common->mimeType);
}

gboolean
gst_media_common_render (GstMediaCommon * common, xmlTextWriterPtr writer)
{
  if (!gst_media_presentation_write_string_attribute (writer, "mimeType",
          common->mimeType))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "width",
          common->width))
    return FALSE;
  if (!gst_media_presentation_write_uint32_attribute (writer, "height",
          common->height))
    return FALSE;
  /* FIXME: disabled since VLC, the only player that support DASH at this
   * moment do not handle these fields correctly */
  /*
     if (!gst_media_presentation_write_uint32_attribute (writer, "parx",
     common->parx))
     return FALSE;
     if (!gst_media_presentation_write_uint32_attribute (writer, "pary",
     common->pary))
     return FALSE;
     if (!gst_media_presentation_write_double_attribute (writer, "frameRate",
     common->frameRate))
     return FALSE;
   */
  if (!gst_media_presentation_write_uint32_attribute (writer, "group",
          common->group))
    return FALSE;
  if (!gst_media_presentation_write_double_attribute (writer,
          "maximumRAPPeriod", common->maximumRAPPeriod))
    return FALSE;

  return TRUE;
}
