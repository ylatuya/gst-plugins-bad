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
#ifndef _GST_HLS_SINK_H_
#define _GST_HLS_SINK_H_

#include "gstm3u8playlist.h"
#include <gst/gst.h>
#include <gst/baseadaptive/gstbaseadaptivesink.h>

G_BEGIN_DECLS

#define GST_TYPE_HLS_SINK   (gst_hls_sink_get_type())
#define GST_HLS_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HLS_SINK,GstHlsSink))
#define GST_HLS_SINK_CAST(obj)   ((GstHlsSink *) obj)
#define GST_HLS_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HLS_SINK,GstHlsSinkClass))
#define GST_IS_HLS_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HLS_SINK))
#define GST_IS_HLS_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HLS_SINK))

typedef struct _GstHlsSink GstHlsSink;
typedef struct _GstHlsSinkClass GstHlsSinkClass;

struct _GstHlsSink
{
  GstBaseAdaptiveSink parent;

  gchar *stream_title;
  guint max_version;
};

struct _GstHlsSinkClass
{
  GstBaseAdaptiveSinkClass parent_class;
};

GType gst_hls_sink_get_type (void);
gboolean gst_hls_sink_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
