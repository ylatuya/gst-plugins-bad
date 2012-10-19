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
#include "gstdashmanager.h"


static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("stream_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/quicktime, variant=(string)dash-fragmented;"
        "video/mpegts, systemstream=(boolean)true"));

static GstStreamsManager
    * gst_dash_sink_create_dash_manager (GstBaseAdaptiveSink * sink);

enum
{
  PROP_0,
  PROP_MIN_BUFFER_TIME,
  PROP_LAST,
};

#define DEFAULT_MIN_BUFFER_TIME  (2 * GST_SECOND)

G_DEFINE_TYPE (GstDashSink, gst_dash_sink, GST_TYPE_BASE_ADAPTIVE_SINK);

static void gst_dash_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dash_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);


static void
gst_dash_sink_class_init (GstDashSinkClass * klass)
{
  GstBaseAdaptiveSinkClass *base_class = GST_BASE_ADAPTIVE_SINK_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  base_class->create_streams_manager = gst_dash_sink_create_dash_manager;

  gobject_class->set_property = gst_dash_sink_set_property;
  gobject_class->get_property = gst_dash_sink_get_property;

  gst_element_class_set_metadata (element_class,
      "DASH Sink", "Sink/File",
      "Dynamic Adaptive Streaming HTTP sink",
      "Andoni Morales Alastruey <ylatuya@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));

  /**
   * GstDashSink:min-buffer-time
   *
   * Sets the "minBufferTime" property of the media presentation
   *
   */
  g_object_class_install_property (gobject_class, PROP_MIN_BUFFER_TIME,
      g_param_spec_uint ("min-buffer-time", "Minimum buffer time in (ms)",
          " Sets the \"minBufferTime\" property of the media presentation",
          0, G_MAXUINT32, DEFAULT_MIN_BUFFER_TIME, G_PARAM_READWRITE));

}

static void
gst_dash_sink_init (GstDashSink * sink)
{
  GstBaseAdaptiveSink *bsink = GST_BASE_ADAPTIVE_SINK (sink);

  bsink->prepend_headers = FALSE;
  bsink->needs_init_segment = TRUE;
  sink->min_buffer_time = DEFAULT_MIN_BUFFER_TIME;
}

static void
gst_dash_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  switch (prop_id) {
    case PROP_MIN_BUFFER_TIME:
      sink->min_buffer_time = g_value_get_uint (value) * GST_MSECOND;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dash_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDashSink *sink = GST_DASH_SINK (object);

  switch (prop_id) {
    case PROP_MIN_BUFFER_TIME:
      g_value_set_uint (value, (uint) (sink->min_buffer_time / GST_MSECOND));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStreamsManager *
gst_dash_sink_create_dash_manager (GstBaseAdaptiveSink * bsink)
{
  GstDashSink *sink = GST_DASH_SINK (bsink);
  GstDashManager *md;

  md = gst_dash_manager_new (bsink->is_live, bsink->chunked,
      sink->min_buffer_time);

  return GST_STREAMS_MANAGER (md);
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    dash, "Dynamic adaptive streaming over HTTP",
    dash_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
