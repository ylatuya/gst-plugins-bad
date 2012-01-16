/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstsink.c:
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
/**
 * SECTION:element-sink
 * @see_also: #GstBaseSink
 *
 * Fragments incoming data delimited by GstForceKeyUnit events or time intervals
 * into fragments, streamable using the HTTP Live Streaming draft, and creates a
 * playlist that compiles with the draft to serve them.
 *
 * The fragments and the playlist can be written to disk or retrieved using the
 * availables signal actions
 *
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstdashsink.h"
#include "gstmediamanager.h"

#define SUPPORTED_STREAMS "video/x-h264; "\
    "audio/mpeg, mpegversion={2, 4}; "\
    "audio/mpeg, mpegversion=1, layer=3"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("stream_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/quicktime, variant=(string)iso-fragmented"));

static GstBaseMediaManager
    * gst_dash_sink_create_media_manager (GstBaseAdaptiveSink * sink);

GST_BOILERPLATE (GstDashSink, gst_dash_sink, GstBaseAdaptiveSink,
    GST_TYPE_BASE_ADAPTIVE_SINK);

static void
gst_dash_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "DASH Sink", "Sink/File",
      "Dynamic Adaptive Streaming HTTP sink",
      "Andoni Morales Alastruey <ylatuya@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
}

static void
gst_dash_sink_class_init (GstDashSinkClass * klass)
{
  GstBaseAdaptiveSinkClass *base_class = GST_BASE_ADAPTIVE_SINK_CLASS (klass);

  base_class->create_media_manager = gst_dash_sink_create_media_manager;
}

static void
gst_dash_sink_init (GstDashSink * sink, GstDashSinkClass * g_class)
{
  GstBaseAdaptiveSink *b_sink = GST_BASE_ADAPTIVE_SINK (sink);

  b_sink->supported_caps = gst_caps_from_string (SUPPORTED_STREAMS);
}

static GstBaseMediaManager *
gst_dash_sink_create_media_manager (GstBaseAdaptiveSink * sink)
{
  GstMediaManager *md;

  md = gst_media_manager_new ();
  return GST_BASE_MEDIA_MANAGER (md);
}

GST_DEBUG_CATEGORY_STATIC (dash_debug);

static gboolean
dash_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (dash_debug, "dashsink", 0, "dashsink");

  if (!gst_element_register (plugin, "dashsink", GST_RANK_SECONDARY,
          GST_TYPE_DASH_SINK) || FALSE)
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dash",
    "Dynamic adaptive streaming over HTTP",
    dash_init, VERSION, "LGPL", PACKAGE_NAME, "http://www.gstreamer.org/")
