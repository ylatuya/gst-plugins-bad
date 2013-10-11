/*
 * GStreamer AMC Video Sink
 * Copyright (C) 2013 Fluendo S.A.
 *   @author: Andoni Morales <amorales@fluendo.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
#  include <config.h>
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/interfaces/xoverlay.h>

#include "gstamcvideosink.h"
#include "gstamc.h"

GST_DEBUG_CATEGORY_STATIC (gst_amc_video_sink_debug);
#define GST_CAT_DEFAULT gst_amc_video_sink_debug

/* Input capabilities. */
static GstStaticPadTemplate gst_amc_video_sink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-amc"));

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

static void gst_amc_video_sink_init_interfaces_and_debug (GType type);

GST_BOILERPLATE_FULL (GstAmcVideoSink, gst_amc_video_sink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_amc_video_sink_init_interfaces_and_debug);


static GstStateChangeReturn
gst_amc_video_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstAmcVideoSink *avs;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  avs = GST_AMC_VIDEO_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (avs->surface == NULL) {
        gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (avs));
        if (avs->surface == NULL) {
          GST_ERROR_OBJECT (avs, "The application didn't provide a surface");
          ret = GST_STATE_CHANGE_FAILURE;
          goto done;
        }
      }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

done:
  return ret;
}

static gboolean
gst_amc_video_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstAmcVideoSink *avs = GST_AMC_VIDEO_SINK (bsink);

  return gst_amc_query_set_surface (query, avs->surface);
}

static GstFlowReturn
gst_amc_video_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstAmcVideoSink *avs;
  GstAmcDRBuffer *drbuf;

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  avs = GST_AMC_VIDEO_SINK (vsink);
  GST_DEBUG_OBJECT (avs, "Got buffer: %p", buf);
  drbuf = (GstAmcDRBuffer *) GST_BUFFER_DATA (buf);
  if (drbuf != NULL) {
    if (!gst_amc_dr_buffer_render (drbuf)) {
      GST_WARNING_OBJECT (avs, "Could not render buffer %p", buf);
    }
  }
  return GST_FLOW_OK;
}

static void
gst_amc_video_sink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "AMC video sink",
      "Sink/Video", "AMC video sink", "Andoni Morales Alastruey");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_amc_video_sink_sink_template_factory));
}

/* initialize the amc_video_sink's class */
static void
gst_amc_video_sink_class_init (GstAmcVideoSinkClass * klass)
{
  GstVideoSinkClass *gstvideosink_class;
  GstBaseSinkClass *gstbasesink_class;
  GstElementClass *gstelement_class;

  gstvideosink_class = (GstVideoSinkClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_amc_video_sink_change_state);

  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_amc_video_sink_query);

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_amc_video_sink_show_frame);
}

static void
gst_amc_video_sink_init (GstAmcVideoSink * amc_video_sink,
    GstAmcVideoSinkClass * gclass)
{
  /* Init defaults */
}

static void
gst_amc_video_sink_set_window_handle (GstXOverlay * overlay, guintptr id)
{
  GstAmcVideoSink *sink = GST_AMC_VIDEO_SINK (overlay);
  GstPad *pad = GST_VIDEO_SINK_PAD (GST_VIDEO_SINK_PAD (sink));

  sink->surface = (gpointer) id;
  if (gst_pad_is_linked (pad)) {
    gst_pad_push_event (GST_VIDEO_SINK_PAD (GST_VIDEO_SINK (sink)),
        gst_amc_event_new_surface (sink->surface));
  }
}

static void
gst_amc_video_sink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_window_handle = gst_amc_video_sink_set_window_handle;
}

static gboolean
gst_amc_video_sink_interface_supported (GstImplementsInterface * iface,
    GType type)
{
  return (type == GST_TYPE_X_OVERLAY);
}

static void
gst_amc_video_sink_implements_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = gst_amc_video_sink_interface_supported;
}

static void
gst_amc_video_sink_init_interfaces_and_debug (GType type)
{
  static const GInterfaceInfo implements_info = {
    (GInterfaceInitFunc) gst_amc_video_sink_implements_init, NULL, NULL
  };

  static const GInterfaceInfo xoverlay_info = {
    (GInterfaceInitFunc) gst_amc_video_sink_xoverlay_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_IMPLEMENTS_INTERFACE,
      &implements_info);
  g_type_add_interface_static (type, GST_TYPE_X_OVERLAY, &xoverlay_info);

  GST_DEBUG_CATEGORY_INIT (gst_amc_video_sink_debug, "amcvideosink", 0,
      "Android MediaCodec video sink");
}
