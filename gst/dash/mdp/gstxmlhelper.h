/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstxmlhelper.h:
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


#ifndef __GST_XML_HELPER_H__
#define __GST_XML_HELPER_H__

#include <gst/gst.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

gboolean gst_media_presentation_start_document (xmlTextWriterPtr *writer, xmlBufferPtr * buf);
gchar* gst_media_presentation_close_document (xmlTextWriterPtr writer, xmlBufferPtr buf);

gboolean gst_media_presentation_start_element (xmlTextWriterPtr writer, const gchar * name);
gboolean gst_media_presentation_end_element (xmlTextWriterPtr writer);
gboolean gst_media_presentation_write_element (xmlTextWriterPtr writer, const gchar * name, const gchar * content);


gboolean gst_media_presentation_write_string_attribute (xmlTextWriterPtr writer, const gchar * name, const gchar * value);
gboolean gst_media_presentation_write_string_list_attribute (xmlTextWriterPtr writer, const gchar * name, GList *strings);
gboolean gst_media_presentation_write_uint32_attribute (xmlTextWriterPtr writer, const gchar * name, guint32 value);
gboolean gst_media_presentation_write_time_seconds_attribute (xmlTextWriterPtr writer, const gchar * name, guint64 value);
gboolean gst_media_presentation_write_time_attribute (xmlTextWriterPtr writer, const gchar * name, guint64 value);
gboolean gst_media_presentation_write_double_attribute (xmlTextWriterPtr writer, const gchar * name, gdouble value);
gboolean gst_media_presentation_write_bool_attribute (xmlTextWriterPtr writer, const gchar * name, gboolean value);
gboolean gst_media_presentation_write_base_urls (xmlTextWriterPtr writer, GList * base_urls);


#endif
