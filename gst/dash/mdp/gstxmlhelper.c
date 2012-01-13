/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstxmlhelper.c:
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

#include "gstxmlhelper.h"

#define MDP_SECONDS_FORMAT "PT%u.%03uS"
#define MDP_SECONDS_ARGS(t) \
        (guint) (t / GST_SECOND), \
        (guint) ((t / GST_MSECOND) % 1000)

#define MDP_TIME_FORMAT "PT%uH%02uM%02u.%03uS"
#define MDP_TIME_ARGS(t) \
        (guint) (t / (GST_SECOND * 60 * 60)), \
        (guint) ((t / (GST_SECOND * 60)) % 60), \
        (guint) ((t / GST_SECOND) % 60), \
        (guint) (t % GST_SECOND)




gboolean
gst_media_presentation_start_document (xmlTextWriterPtr * writer,
    xmlBufferPtr * buf)
{
  /* Create a new XML buffer, to which the XML document will be
   * written */
  *buf = xmlBufferCreate ();
  if (buf == NULL) {
    GST_WARNING ("Error creating the xml buffer");
    goto error;
  }

  /* Create a new XmlWriter for memory */
  *writer = xmlNewTextWriterMemory (*buf, 0);
  if (writer == NULL) {
    GST_WARNING ("Error creating the xml writter");
    goto error;
  }

  /* Start the document */
  if (xmlTextWriterStartDocument (*writer, NULL, "UTF-8", NULL) < 0) {
    GST_WARNING ("Error at xmlTextWriterStartDocument");
    goto error;
  }
  return TRUE;

error:
  {
    GST_ERROR ("Error rendering media presentation");
    if (*writer != NULL)
      xmlFreeTextWriter (*writer);
    if (*buf != NULL)
      xmlBufferFree (*buf);
    return FALSE;
  }
}

gchar *
gst_media_presentation_close_document (xmlTextWriterPtr writer,
    xmlBufferPtr buf)
{
  gchar *mdp_str;

  /* End document */
  if (xmlTextWriterEndDocument (writer) < 0) {
    GST_WARNING ("Error at xmlTextWriterEndDocument");
    return NULL;
  }

  mdp_str = g_strdup ((gchar *) buf->content);
  if (writer != NULL)
    xmlFreeTextWriter (writer);
  if (buf != NULL)
    xmlBufferFree (buf);
  return mdp_str;
}

gboolean
gst_media_presentation_write_element (xmlTextWriterPtr writer,
    const gchar * name, const gchar * content)
{
  gboolean ret = TRUE;;

  if (xmlTextWriterWriteFormatElement (writer, BAD_CAST name, "%s",
          content) < 0) {
    GST_WARNING ("Error writting element:%s with content: %s", name, content);
    ret = FALSE;
  }
  GST_LOG ("Writting element:%s with content: %s", name, content);

  return ret;
}

gboolean
gst_media_presentation_write_base_urls (xmlTextWriterPtr writer,
    GList * base_urls)
{
  GList *tmp;

  tmp = g_list_first (base_urls);
  while (tmp != NULL) {
    if (!gst_media_presentation_write_element (writer, "BaseUrl",
            (gchar *) tmp->data))
      return FALSE;
    tmp = g_list_next (tmp);
  }
  return TRUE;
}

static gboolean
gst_media_presentation_write_attribute (xmlTextWriterPtr writer,
    const gchar * name, const gchar * value)
{
  if (xmlTextWriterWriteAttribute (writer, BAD_CAST name, BAD_CAST value) < 0) {
    GST_WARNING ("Error writting attribute %s=\"%s\"", name, value);
    return FALSE;
  }
  GST_LOG ("Writing attribute %s=\"%s\"", name, value);
  return TRUE;
}

gboolean
gst_media_presentation_write_string_attribute (xmlTextWriterPtr writer,
    const gchar * name, const gchar * content)
{
  if (content == NULL)
    return TRUE;
  return gst_media_presentation_write_attribute (writer, name, content);
}

gboolean
gst_media_presentation_write_string_list_attribute (xmlTextWriterPtr writer,
    const gchar * name, GList * strings)
{
  GString *string;
  GList *tmp;
  gboolean first = TRUE;
  gboolean ret = FALSE;

  string = g_string_new ("");
  tmp = g_list_first (strings);
  while (tmp != NULL) {
    g_string_append_printf (string, "%s%s", first ? "" : " ",
        (gchar *) tmp->data);
    tmp = g_list_next (tmp);
    first = FALSE;
  }

  if (!gst_media_presentation_write_string_attribute (writer, name,
          string->str))
    goto quit;

  ret = TRUE;

quit:
  {
    g_string_free (string, TRUE);
    return ret;
  }
}

gboolean
gst_media_presentation_write_uint32_attribute (xmlTextWriterPtr writer,
    const gchar * name, guint32 value)
{
  gchar *value_str;
  gboolean ret;

  if (value == 0)
    return TRUE;

  value_str = g_strdup_printf ("%d", value);
  ret = gst_media_presentation_write_attribute (writer, name, value_str);
  g_free (value_str);
  return ret;
}

gboolean
gst_media_presentation_write_time_attribute (xmlTextWriterPtr writer,
    const gchar * name, guint64 value)
{
  gchar *value_str;
  gboolean ret;

  if (!GST_CLOCK_TIME_IS_VALID (value))
    return TRUE;

  value_str = g_strdup_printf (MDP_TIME_FORMAT, MDP_TIME_ARGS (value));
  ret = gst_media_presentation_write_attribute (writer, name, value_str);
  g_free (value_str);
  return ret;
}

gboolean
gst_media_presentation_write_time_seconds_attribute (xmlTextWriterPtr writer,
    const gchar * name, guint64 value)
{
  gchar *value_str;
  gboolean ret;

  if (!GST_CLOCK_TIME_IS_VALID (value))
    return TRUE;

  value_str = g_strdup_printf (MDP_SECONDS_FORMAT, MDP_SECONDS_ARGS (value));
  ret = gst_media_presentation_write_attribute (writer, name, value_str);
  g_free (value_str);
  return ret;
}

gboolean
gst_media_presentation_write_double_attribute (xmlTextWriterPtr writer,
    const gchar * name, gdouble value)
{
  gchar *value_str;
  gboolean ret;

  if (value == 0)
    return TRUE;

  value_str = g_strdup_printf ("%f", value);
  ret = gst_media_presentation_write_attribute (writer, name, value_str);
  g_free (value_str);
  return ret;
}

gboolean
gst_media_presentation_write_bool_attribute (xmlTextWriterPtr writer,
    const gchar * name, gboolean value)
{
  return gst_media_presentation_write_attribute (writer, name,
      value ? "true" : "false");
}

gboolean
gst_media_presentation_start_element (xmlTextWriterPtr writer,
    const gchar * name)
{
  if (xmlTextWriterStartElement (writer, BAD_CAST name) < 0) {
    GST_WARNING ("Error starting element %s", name);
    return FALSE;
  }
  GST_LOG ("Starting element %s", name);
  return TRUE;
}

gboolean
gst_media_presentation_end_element (xmlTextWriterPtr writer)
{
  if (xmlTextWriterEndElement (writer) < 0) {
    GST_WARNING ("Error ending element");
    return FALSE;
  }
  GST_LOG ("Ending element");
  return TRUE;
}
