/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstdashsink.h:
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

#ifndef __GST_DASH_SINK_H__
#define __GST_DASH_SINK_H__

#include <gst/gst.h>
#include <gst/baseadaptive/gstbaseadaptivesink.h>

G_BEGIN_DECLS

#define GST_TYPE_DASH_SINK (gst_dash_sink_get_type())
#define GST_DASH_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DASH_SINK,GstDashSink))
#define GST_DASH_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DASH_SINK,GstDashSinkClass))
#define GST_IS_DASH_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DASH_SINK))
#define GST_IS_DASH_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DASH_SINK))

typedef struct _GstDashSink GstDashSink;
typedef struct _GstDashSinkClass GstDashSinkClass;
typedef struct _GstDashSinkClass GstDashSinkClass;

struct _GstDashSink
{
  GstBaseAdaptiveSink adaptive_sink;

  GstClockTime min_buffer_time;
};

struct _GstDashSinkClass
{
  GstBaseAdaptiveSinkClass parent_class;

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};

GType gst_dash_sink_get_type (void);

G_END_DECLS

#endif /* __GST_DASH_SINK_H__ */
