/* GStreamer
 *
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_DECKLINK_VIDEO_SINK_H__
#define __GST_DECKLINK_VIDEO_SINK_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include "gstdecklink.h"

G_BEGIN_DECLS

#define GST_TYPE_DECKLINK_VIDEO_SINK \
  (gst_decklink_video_sink_get_type())
#define GST_DECKLINK_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DECKLINK_VIDEO_SINK, GstDecklinkVideoSink))
#define GST_DECKLINK_VIDEO_SINK_CAST(obj) \
  ((GstDecklinkVideoSink*)obj)
#define GST_DECKLINK_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DECKLINK_VIDEO_SINK, GstDecklinkVideoSinkClass))
#define GST_IS_DECKLINK_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DECKLINK_VIDEO_SINK))
#define GST_IS_DECKLINK_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DECKLINK_VIDEO_SINK))

typedef struct _GstDecklinkVideoSink GstDecklinkVideoSink;
typedef struct _GstDecklinkVideoSinkClass GstDecklinkVideoSinkClass;

struct _GstDecklinkVideoSink
{
  GstBaseSink parent;

  GstDecklinkModeEnum mode;
  gint device_number;

  GstVideoInfo info;

  GstClockTime internal_base_time;
  GstClockTime external_base_time;
  GstClockTime last_render_time;

  GstDecklinkOutput *output;
};

struct _GstDecklinkVideoSinkClass
{
  GstBaseSinkClass parent_class;
};

GType gst_decklink_video_sink_get_type (void);

G_END_DECLS

#endif /* __GST_DECKLINK_VIDEO_SINK_H__ */
