/* GStreamer
 * Copyright (C) 2011 Alessandro Decina <alessandro.d@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthlssink.h"
#include "gstm3u8manager.h"

GST_DEBUG_CATEGORY_STATIC (gst_hls_sink_debug);
#define GST_CAT_DEFAULT gst_hls_sink_debug

#define DEFAULT_OUTPUT_DIR "."
#define DEFAULT_VARIANT_PLAYLIST_NAME "main"
#define DEFAULT_STREAM_TITLE "GStreamer HLS stream"
#define DEFAULT_BASE_URL "http://localhost/"
#define DEFAULT_FRAGMENT_PREFIX "fragment"
#define DEFAULT_TARGET_DURATION 10
#define DEFAULT_FRAGMENT_TEMPLATE "%s-%s-%05d.ts"
#define DEFAULT_MAX_WINDOW 0
#define DEFAULT_MAX_VERSION 2
#define DEFAULT_IS_CHUNKED TRUE
#define DEFAULT_IS_LIVE FALSE
#define DEFAULT_WRITE_TO_DISK TRUE
#define DEFAULT_DELETE_OLD_FILES TRUE

#define SUPPORTED_CAPS "video/x-h264; " \
  "audio/mpeg, mpegversion=1, layer=3;"\
  "audio/mpeg, mpegversion={2, 4}"

enum
{
  PROP_0,
  PROP_STREAM_TITLE,
  PROP_MAX_VERSION,
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

G_DEFINE_TYPE (GstHlsSink, gst_hls_sink, GST_TYPE_BASE_ADAPTIVE_SINK);

static void gst_hls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_hls_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec);


static void
gst_hls_sink_finalize (GObject * object)
{
  GstHlsSink *sink = GST_HLS_SINK_CAST (object);

  if (sink->stream_title != NULL) {
    g_free (sink->stream_title);
    sink->stream_title = NULL;
  }

  G_OBJECT_CLASS (gst_hls_sink_parent_class)->finalize ((GObject *) sink);
}

static GstStreamsManager *
gst_hls_sink_create_streams_manager (GstBaseAdaptiveSink * bsink)
{
  GstM3u8Manager *man;
  GstHlsSink *sink = GST_HLS_SINK_CAST (bsink);

  man = gst_m3u8_manager_new (bsink->base_url, sink->stream_title,
      bsink->fragment_prefix, bsink->output_directory, bsink->is_live,
      bsink->chunked, bsink->max_window, bsink->md_name, sink->max_version,
      FALSE);

  return GST_STREAMS_MANAGER (man);
}

static void
gst_hls_sink_class_init (GstHlsSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseAdaptiveSinkClass *base_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  base_class = GST_BASE_ADAPTIVE_SINK_CLASS (klass);

  base_class->create_streams_manager = gst_hls_sink_create_streams_manager;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));

  gobject_class->finalize = gst_hls_sink_finalize;
  gobject_class->set_property = gst_hls_sink_set_property;
  gobject_class->get_property = gst_hls_sink_get_property;

  gst_element_class_set_metadata (element_class,
      "HTTP Live Streaming sink", "Sink", "HTTP Live Streaming sink",
      "Andoni Morales Alastruey <ylatuya@gmail.com>");

  g_object_class_install_property (gobject_class, PROP_STREAM_TITLE,
      g_param_spec_string ("title", "Stream title",
          "Title of the HLS stream", DEFAULT_STREAM_TITLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_VERSION,
      g_param_spec_uint ("max-version", "Max supported version",
          "Maximum supported version of the HLS protocol (default: 2)",
          0, 4, DEFAULT_MAX_VERSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_hls_sink_init (GstHlsSink * sink)
{
  GstBaseAdaptiveSink *bsink;

  bsink = GST_BASE_ADAPTIVE_SINK (sink);

  gst_base_adaptive_sink_set_output_directory (bsink, DEFAULT_OUTPUT_DIR);
  gst_base_adaptive_sink_set_md_name (bsink, DEFAULT_VARIANT_PLAYLIST_NAME);
  gst_base_adaptive_sink_set_base_url (bsink, DEFAULT_BASE_URL);
  gst_base_adaptive_sink_set_fragment_prefix (bsink, DEFAULT_FRAGMENT_PREFIX);
  bsink->chunked = DEFAULT_IS_CHUNKED;
  bsink->is_live = DEFAULT_IS_LIVE;
  bsink->write_to_disk = DEFAULT_WRITE_TO_DISK;
  bsink->delete_old_files = DEFAULT_DELETE_OLD_FILES;
  bsink->max_window = DEFAULT_MAX_WINDOW * GST_SECOND;
  bsink->fragment_duration = DEFAULT_TARGET_DURATION * GST_SECOND;
  bsink->fragment_tpl = DEFAULT_FRAGMENT_TEMPLATE;
  bsink->append_headers = TRUE;
  bsink->supported_caps = gst_caps_from_string (SUPPORTED_CAPS);

  sink->max_version = DEFAULT_MAX_VERSION;
  sink->stream_title = g_strdup (DEFAULT_STREAM_TITLE);
}

static void
gst_hls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHlsSink *sink = GST_HLS_SINK_CAST (object);

  switch (prop_id) {
    case PROP_MAX_VERSION:
      sink->max_version = g_value_get_uint (value);
      break;
    case PROP_STREAM_TITLE:
      g_free (sink->stream_title);
      sink->stream_title = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_hls_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstHlsSink *sink = GST_HLS_SINK_CAST (object);

  switch (prop_id) {
    case PROP_MAX_VERSION:
      g_value_set_uint (value, sink->max_version);
      break;
    case PROP_STREAM_TITLE:
      g_value_set_string (value, sink->stream_title);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_hls_sink_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_hls_sink_debug, "hlssink", 0, "HlsSink");
  return gst_element_register (plugin, "hlssink", GST_RANK_NONE,
      gst_hls_sink_get_type ());
}
