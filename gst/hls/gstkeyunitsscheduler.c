/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstkeyunitsscheduler.c:
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
 * SECTION:element-keyunitsscheduler
 *
 * Schedules GstForceKeyUnit events at a fixed interval to create fragments
 * of 'interval' duration, starting always with a keyframe if the encoder
 * supports it.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! keyunitsscheluer interval=10000000000 ! vp8enc ! webmmux ! hlssink
 * ]|
 * </refsect2>
 *
 * Last reviewed on 2010-10-07
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/video/video.h>
#include "gstkeyunitsscheduler.h"

GST_DEBUG_CATEGORY_STATIC (gst_key_units_scheduler_debug);
#define GST_CAT_DEFAULT gst_key_units_scheduler_debug

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static gboolean gst_key_units_scheduler_start (GstBaseTransform * trans);
static GstFlowReturn gst_key_units_scheduler_transform_ip (GstBaseTransform *
    trans, GstBuffer * buffer);

enum
{
  PROP_0,
  PROP_INTERVAL,
  PROP_LAST
};

#define DEFAULT_INTERVAL 10 * GST_SECOND

/* GObject */
static void gst_key_units_scheduler_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_key_units_scheduler_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

GST_BOILERPLATE (GstKeyUnitsScheduler, gst_key_units_scheduler,
    GstBaseTransform, GST_TYPE_BASE_TRANSFORM);

static void
gst_key_units_scheduler_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG_CATEGORY_INIT (gst_key_units_scheduler_debug, "keyunitsscheduler",
      0, "key units scheduler");

  gst_element_class_set_details_simple (element_class,
      "Key Units Scheduler",
      "Demuxer/URIList",
      "Schedule GstForceKeyUnit events at a given interval",
      "Andoni Morales Alastruey <ylatuya@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));
}


static void
gst_key_units_scheduler_class_init (GstKeyUnitsSchedulerClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetrans_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetrans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_key_units_scheduler_set_property;
  gobject_class->get_property = gst_key_units_scheduler_get_property;

  g_object_class_install_property (gobject_class, PROP_INTERVAL,
      g_param_spec_int64 ("interval", "Interval",
          "Interval in ns at which the GstForceKeyUnit events will be "
          "sent downstream",
          0, G_MAXINT64, DEFAULT_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasetrans_class->start = GST_DEBUG_FUNCPTR (gst_key_units_scheduler_start);
  gstbasetrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_key_units_scheduler_transform_ip);
}

static void
gst_key_units_scheduler_init (GstKeyUnitsScheduler * kus,
    GstKeyUnitsSchedulerClass * klass)
{
  /* Properties */
  kus->interval = DEFAULT_INTERVAL;

  kus->last_ts = GST_CLOCK_TIME_NONE;
  kus->count = 0;
}

static void
gst_key_units_scheduler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKeyUnitsScheduler *kus = GST_KEY_UNITS_SCHEDULER (object);

  switch (prop_id) {
    case PROP_INTERVAL:
      kus->interval = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_key_units_scheduler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKeyUnitsScheduler *kus = GST_KEY_UNITS_SCHEDULER (object);

  switch (prop_id) {
    case PROP_INTERVAL:
      g_value_set_int64 (value, kus->interval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_key_units_scheduler_start (GstBaseTransform * trans)
{
  GstKeyUnitsScheduler *kus;

  kus = GST_KEY_UNITS_SCHEDULER (trans);
  kus->last_ts = GST_CLOCK_TIME_NONE;
  kus->count = 0;

  return TRUE;
}

GstFlowReturn
gst_key_units_scheduler_transform_ip (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstKeyUnitsScheduler *kus;
  GstPad *pad;
  GstEvent *event;
  GstClock *clock;
  GstClockTime running_time, stream_time, timestamp;

  kus = GST_KEY_UNITS_SCHEDULER (trans);
  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  if (kus->interval == 0)
    return GST_FLOW_OK;

  /* Ignore buffers without timestamps */
  if (timestamp == GST_CLOCK_TIME_NONE)
    return GST_FLOW_OK;

  /* We need to send a GstForceKeyUnit event always before the first buffer to
   * delimit the start of a new fragment (when last_ts == GST_CLOCK_TIME_NONE),
   * or when the last event was sent before the defined interval */
  if (kus->last_ts != GST_CLOCK_TIME_NONE &&
      (timestamp - kus->last_ts) < kus->interval)
    return GST_FLOW_OK;

  clock = gst_element_get_clock (GST_ELEMENT (trans));
  if (clock) {
    stream_time = gst_clock_get_time (clock);
    running_time =
        stream_time - gst_element_get_base_time (GST_ELEMENT (trans));
  } else {
    stream_time = GST_CLOCK_TIME_NONE;
    running_time = GST_CLOCK_TIME_NONE;
  }

  pad = GST_BASE_TRANSFORM_SRC_PAD (trans);
  event = gst_video_event_new_downstream_force_key_unit (timestamp, stream_time,
      running_time, TRUE, kus->count);
  GST_INFO_OBJECT (kus, "Pushing  GstForceKeyUnit event timestamp: %"
      GST_TIME_FORMAT " count: %d", GST_TIME_ARGS (timestamp), kus->count);
  gst_pad_push_event (pad, event);

  kus->count++;
  kus->last_ts = timestamp;

  return GST_FLOW_OK;
}
