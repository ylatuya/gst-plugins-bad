/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Florian Langlois <florian.langlois@fr.thalesgroup.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkvideosrc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_decklink_video_src_debug);
#define GST_CAT_DEFAULT gst_decklink_video_src_debug

#define DEFAULT_MODE (GST_DECKLINK_MODE_AUTO)
#define DEFAULT_CONNECTION (GST_DECKLINK_CONNECTION_AUTO)
#define DEFAULT_BUFFER_SIZE (5)

enum
{
  PROP_0,
  PROP_MODE,
  PROP_CONNECTION,
  PROP_DEVICE_NUMBER,
  PROP_BUFFER_SIZE
};

typedef struct
{
  IDeckLinkVideoInputFrame *frame;
  GstClockTime capture_time, capture_duration;
  GstDecklinkModeEnum mode;
} CaptureFrame;

static void
capture_frame_free (void *data)
{
  CaptureFrame *frame = (CaptureFrame *) data;

  frame->frame->Release ();
  g_free (frame);
}

typedef struct
{
  IDeckLinkVideoInputFrame *frame;
  IDeckLinkInput *input;
} VideoFrame;

static void
video_frame_free (void *data)
{
  VideoFrame *frame = (VideoFrame *) data;

  frame->frame->Release ();
  frame->input->Release ();
  g_free (frame);
}

static void gst_decklink_video_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_decklink_video_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_decklink_video_src_finalize (GObject * object);

static GstStateChangeReturn
gst_decklink_video_src_change_state (GstElement * element,
    GstStateChange transition);
static GstClock *gst_decklink_video_src_provide_clock (GstElement * element);

static gboolean gst_decklink_video_src_set_caps (GstBaseSrc * bsrc,
    GstCaps * caps);
static GstCaps *gst_decklink_video_src_get_caps (GstBaseSrc * bsrc);
static gboolean gst_decklink_video_src_query (GstBaseSrc * bsrc,
    GstQuery * query);
static gboolean gst_decklink_video_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_decklink_video_src_unlock_stop (GstBaseSrc * bsrc);

static GstFlowReturn gst_decklink_video_src_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

static gboolean gst_decklink_video_src_open (GstDecklinkVideoSrc * self);
static gboolean gst_decklink_video_src_close (GstDecklinkVideoSrc * self);

static void gst_decklink_video_src_start_streams (GstElement * element);

#define parent_class gst_decklink_video_src_parent_class
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_decklink_video_src_debug, "decklinksrc", 0, \
      "debug category for decklinksrc element");

GST_BOILERPLATE_FULL (GstDecklinkVideoSrc, gst_decklink_video_src, GstPushSrc,
    GST_TYPE_PUSH_SRC, DEBUG_INIT);

static void
gst_decklink_video_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstCaps *templ_caps;

  templ_caps = gst_decklink_mode_get_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, templ_caps));

  gst_element_class_set_details_simple (element_class, "Decklink source",
      "Source/Video", "DeckLink Source", "David Schleef <ds@entropywave.com>");
}

static void
gst_decklink_video_src_class_init (GstDecklinkVideoSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_decklink_video_src_set_property;
  gobject_class->get_property = gst_decklink_video_src_get_property;
  gobject_class->finalize = gst_decklink_video_src_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_decklink_video_src_change_state);
  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_decklink_video_src_provide_clock);

  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_decklink_video_src_get_caps);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_decklink_video_src_set_caps);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_decklink_video_src_query);
  basesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_decklink_video_src_unlock);
  basesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_decklink_video_src_unlock_stop);

  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_decklink_video_src_create);

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Playback Mode",
          "Video Mode to use for playback",
          GST_TYPE_DECKLINK_MODE, DEFAULT_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_CONNECTION,
      g_param_spec_enum ("connection", "Connection",
          "Video input connection to use",
          GST_TYPE_DECKLINK_CONNECTION, DEFAULT_CONNECTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NUMBER,
      g_param_spec_int ("device-number", "Device number",
          "Output device instance to use", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size",
          "Size of internal buffer in number of video frames", 1,
          G_MAXINT, DEFAULT_BUFFER_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_decklink_video_src_init (GstDecklinkVideoSrc * self, GstDecklinkVideoSrcClass *klass)
{
  self->mode = DEFAULT_MODE;
  self->caps_mode = GST_DECKLINK_MODE_AUTO;
  self->connection = DEFAULT_CONNECTION;
  self->device_number = 0;
  self->buffer_size = DEFAULT_BUFFER_SIZE;

  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  g_queue_init (&self->current_frames);
}

void
gst_decklink_video_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      self->mode = (GstDecklinkModeEnum) g_value_get_enum (value);
      break;
    case PROP_CONNECTION:
      self->connection = (GstDecklinkConnectionEnum) g_value_get_enum (value);
      break;
    case PROP_DEVICE_NUMBER:
      self->device_number = g_value_get_int (value);
      break;
    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;
    case PROP_CONNECTION:
      g_value_set_enum (value, self->connection);
      break;
    case PROP_DEVICE_NUMBER:
      g_value_set_int (value, self->device_number);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_src_finalize (GObject * object)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (object);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_decklink_video_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);
  GstCaps *current_caps;
  const GstDecklinkMode *mode;
  BMDVideoInputFlags flags;
  HRESULT ret;

  GST_DEBUG_OBJECT (self, "Setting caps %" GST_PTR_FORMAT, caps);

  if ((current_caps = gst_pad_get_negotiated_caps (GST_BASE_SRC_PAD (bsrc)))) {
    GST_DEBUG_OBJECT (self, "Pad already has caps %" GST_PTR_FORMAT, caps);

    if (!gst_caps_is_equal (caps, current_caps)) {
      GST_ERROR_OBJECT (self, "New caps are not equal to old caps");
      gst_caps_unref (current_caps);
      return FALSE;
    } else {
      gst_caps_unref (current_caps);
      return TRUE;
    }
  }

  if (!gst_video_get_size_from_caps (caps, &self->buf_size))
    return FALSE;

  if (self->input->config && self->connection != GST_DECKLINK_CONNECTION_AUTO) {
    ret = self->input->config->SetInt (bmdDeckLinkConfigVideoInputConnection,
        gst_decklink_get_connection (self->connection));
    if (ret != S_OK) {
      GST_ERROR_OBJECT (self, "Failed to set configuration (input source)");
      return FALSE;
    }

    if (self->connection == GST_DECKLINK_CONNECTION_COMPOSITE) {
      ret = self->input->config->SetInt (bmdDeckLinkConfigAnalogVideoInputFlags,
          bmdAnalogVideoFlagCompositeSetup75);
      if (ret != S_OK) {
        GST_ERROR_OBJECT (self,
            "Failed to set configuration (composite setup)");
        return FALSE;
      }
    }
  }

  flags = bmdVideoInputFlagDefault;
  if (self->mode == GST_DECKLINK_MODE_AUTO) {
    bool autoDetection = false;

    if (self->input->attributes) {
      ret =
          self->input->
          attributes->GetFlag (BMDDeckLinkSupportsInputFormatDetection,
          &autoDetection);
      if (ret != S_OK) {
        GST_ERROR_OBJECT (self, "Failed to get attribute (autodetection)");
        return FALSE;
      }
      if (autoDetection)
        flags |= bmdVideoInputEnableFormatDetection;
    }
    if (!autoDetection) {
      GST_ERROR_OBJECT (self, "Failed to activate auto-detection");
      return FALSE;
    }
  }

  mode = gst_decklink_get_mode (self->mode);
  g_assert (mode != NULL);

  ret = self->input->input->EnableVideoInput (mode->mode,
      bmdFormat8BitYUV, flags);
  if (ret != S_OK) {
    GST_WARNING_OBJECT (self, "Failed to enable video input");
    return FALSE;
  }

  g_mutex_lock (&self->input->lock);
  self->input->mode = mode;
  self->input->video_enabled = TRUE;
  if (self->input->start_streams)
    self->input->start_streams (self->input->videosrc);
  g_mutex_unlock (&self->input->lock);

  return TRUE;
}

static GstCaps *
gst_decklink_video_src_get_caps (GstBaseSrc * bsrc)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);
  GstCaps *mode_caps, *caps;

  g_mutex_lock (&self->lock);
  if (self->caps_mode != GST_DECKLINK_MODE_AUTO)
    mode_caps = gst_decklink_mode_get_caps (self->caps_mode);
  else
    mode_caps = gst_decklink_mode_get_caps (self->mode);
  g_mutex_unlock (&self->lock);

  caps = mode_caps;

  return caps;
}

void
gst_decklink_video_src_convert_to_external_clock (GstDecklinkVideoSrc * self,
    GstClockTime * timestamp, GstClockTime * duration)
{
  GstClock *clock;

  g_assert (timestamp != NULL);

  if (*timestamp == GST_CLOCK_TIME_NONE)
    return;

  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  if (clock && clock != self->input->clock) {
    GstClockTime internal, external, rate_n, rate_d;
    GstClockTimeDiff external_start_time_diff;

    gst_clock_get_calibration (self->input->clock, &internal, &external,
        &rate_n, &rate_d);

    if (rate_n != rate_d && self->internal_base_time != GST_CLOCK_TIME_NONE) {
      GstClockTime internal_timestamp = *timestamp;

      // Convert to the running time corresponding to both clock times
      internal -= self->internal_base_time;
      external -= self->external_base_time;

      // Get the difference in the internal time, note
      // that the capture time is internal time.
      // Then scale this difference and offset it to
      // our external time. Now we have the running time
      // according to our external clock.
      //
      // For the duration we just scale
      if (internal > internal_timestamp) {
        guint64 diff = internal - internal_timestamp;
        diff = gst_util_uint64_scale (diff, rate_n, rate_d);
        *timestamp = external - diff;
      } else {
        guint64 diff = internal_timestamp - internal;
        diff = gst_util_uint64_scale (diff, rate_n, rate_d);
        *timestamp = external + diff;
      }

      GST_LOG_OBJECT (self,
          "Converted %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT " (external: %"
          GST_TIME_FORMAT " internal %" GST_TIME_FORMAT " rate: %lf)",
          GST_TIME_ARGS (internal_timestamp), GST_TIME_ARGS (*timestamp),
          GST_TIME_ARGS (external), GST_TIME_ARGS (internal),
          ((gdouble) rate_n) / ((gdouble) rate_d));

      if (duration) {
        GstClockTime internal_duration = *duration;

        *duration = gst_util_uint64_scale (internal_duration, rate_d, rate_n);

        GST_LOG_OBJECT (self,
            "Converted duration %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT
            " (external: %" GST_TIME_FORMAT " internal %" GST_TIME_FORMAT
            " rate: %lf)", GST_TIME_ARGS (internal_duration),
            GST_TIME_ARGS (*duration), GST_TIME_ARGS (external),
            GST_TIME_ARGS (internal), ((gdouble) rate_n) / ((gdouble) rate_d));
      }
    } else {
      GST_LOG_OBJECT (self, "No clock conversion needed, relative rate is 1.0");
    }

    // Add the diff between the external time when we
    // went to playing and the external time when the
    // pipeline went to playing. Otherwise we will
    // always start outputting from 0 instead of the
    // current running time.
    external_start_time_diff =
        gst_element_get_base_time (GST_ELEMENT_CAST (self));
    external_start_time_diff =
        self->external_base_time - external_start_time_diff;
    *timestamp += external_start_time_diff;
  } else {
    GST_LOG_OBJECT (self, "No clock conversion needed, same clocks");
  }
}

static void
gst_decklink_video_src_got_frame (GstElement * element,
    IDeckLinkVideoInputFrame * frame, GstDecklinkModeEnum mode,
    GstClockTime capture_time, GstClockTime capture_duration)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (element);

  GST_LOG_OBJECT (self, "Got video frame at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (capture_time));

  gst_decklink_video_src_convert_to_external_clock (self, &capture_time,
      &capture_duration);

  GST_LOG_OBJECT (self, "Actual timestamp %" GST_TIME_FORMAT,
      GST_TIME_ARGS (capture_time));

  g_mutex_lock (&self->lock);
  if (!self->flushing) {
    CaptureFrame *f;

    while (g_queue_get_length (&self->current_frames) >= self->buffer_size) {
      f = (CaptureFrame *) g_queue_pop_head (&self->current_frames);
      GST_WARNING_OBJECT (self, "Dropping old frame at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (f->capture_time));
      capture_frame_free (f);
    }

    f = (CaptureFrame *) g_malloc0 (sizeof (CaptureFrame));
    f->frame = frame;
    f->capture_time = capture_time;
    f->capture_duration = capture_duration;
    f->mode = mode;
    frame->AddRef ();
    g_queue_push_tail (&self->current_frames, f);
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);
}

static GstFlowReturn
gst_decklink_video_src_create (GstPushSrc * bsrc, GstBuffer ** buffer)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);
  GstFlowReturn flow_ret = GST_FLOW_OK;
  const guint8 *data;
  gsize data_size;
  VideoFrame *vf;
  CaptureFrame *f;
  GstCaps *caps;

  g_mutex_lock (&self->lock);
  while (g_queue_is_empty (&self->current_frames) && !self->flushing) {
    g_cond_wait (&self->cond, &self->lock);
  }

  f = (CaptureFrame *) g_queue_pop_head (&self->current_frames);
  g_mutex_unlock (&self->lock);

  if (self->flushing) {
    if (f)
      capture_frame_free (f);
    return GST_FLOW_WRONG_STATE;
  }

  g_mutex_lock (&self->lock);
  if (self->mode == GST_DECKLINK_MODE_AUTO && self->caps_mode != f->mode) {
    self->caps_mode = f->mode;
    g_mutex_unlock (&self->lock);
    g_mutex_lock (&self->input->lock);
    self->input->mode = gst_decklink_get_mode (f->mode);
    g_mutex_unlock (&self->input->lock);
    caps = gst_decklink_mode_get_caps (f->mode);
    gst_video_get_size_from_caps (caps, &self->buf_size);
    gst_pad_set_caps (GST_BASE_SRC_PAD (bsrc), caps);
    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_latency (GST_OBJECT_CAST (self)));
    gst_caps_unref (caps);
  } else {
    g_mutex_unlock (&self->lock);
  }

  f->frame->GetBytes ((gpointer *) & data);
  data_size = self->buf_size;

  vf = (VideoFrame *) g_malloc0 (sizeof (VideoFrame));

  *buffer = gst_buffer_new ();
  gst_buffer_set_data (buffer, (guint8*) data, data_size);
  GST_BUFFER_MALLOCDATA (buffer) = (guint8*) vf;
  GST_BUFFER_FREE_FUNC (buffer) = video_frame_free;

  vf->frame = f->frame;
  f->frame->AddRef ();
  vf->input = self->input->input;
  vf->input->AddRef ();

  GST_BUFFER_TIMESTAMP (*buffer) = f->capture_time;
  GST_BUFFER_DURATION (*buffer) = f->capture_duration;

  capture_frame_free (f);

  return flow_ret;
}

static gboolean
gst_decklink_video_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);
  gboolean ret = TRUE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if (self->input) {
        GstClockTime min, max;
        const GstDecklinkMode *mode;

        g_mutex_lock (&self->lock);
        if (self->caps_mode != GST_DECKLINK_MODE_AUTO)
          mode = gst_decklink_get_mode (self->caps_mode);
        else
          mode = gst_decklink_get_mode (self->mode);
        g_mutex_unlock (&self->lock);

        min = gst_util_uint64_scale_ceil (GST_SECOND, mode->fps_d, mode->fps_n);
        max = self->buffer_size * min;

        gst_query_set_latency (query, TRUE, min, max);
        ret = TRUE;
      } else {
        ret = FALSE;
      }

      break;
    }
    default:
      ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      break;
  }

  return ret;
}

static gboolean
gst_decklink_video_src_unlock (GstBaseSrc * bsrc)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_decklink_video_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = FALSE;
  g_queue_foreach (&self->current_frames, (GFunc) capture_frame_free, NULL);
  g_queue_clear (&self->current_frames);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_decklink_video_src_open (GstDecklinkVideoSrc * self)
{
  const GstDecklinkMode *mode;

  GST_DEBUG_OBJECT (self, "Starting");

  self->input =
      gst_decklink_acquire_nth_input (self->device_number,
      GST_ELEMENT_CAST (self), FALSE);
  if (!self->input) {
    GST_ERROR_OBJECT (self, "Failed to acquire input");
    return FALSE;
  }

  mode = gst_decklink_get_mode (self->mode);
  g_assert (mode != NULL);
  g_mutex_lock (&self->input->lock);
  self->input->mode = mode;
  self->input->got_video_frame = gst_decklink_video_src_got_frame;
  self->input->start_streams = gst_decklink_video_src_start_streams;
  self->input->clock_start_time = GST_CLOCK_TIME_NONE;
  self->input->clock_last_time = 0;
  self->input->clock_offset = 0;
  g_mutex_unlock (&self->input->lock);

  return TRUE;
}

static gboolean
gst_decklink_video_src_close (GstDecklinkVideoSrc * self)
{

  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->input) {
    g_mutex_lock (&self->input->lock);
    self->input->got_video_frame = NULL;
    self->input->mode = NULL;
    self->input->video_enabled = FALSE;
    if (self->input->start_streams)
      self->input->start_streams (self->input->videosrc);
    g_mutex_unlock (&self->input->lock);

    self->input->input->DisableVideoInput ();
    gst_decklink_release_nth_input (self->device_number,
        GST_ELEMENT_CAST (self), FALSE);
    self->input = NULL;
  }

  return TRUE;
}

static void
gst_decklink_video_src_start_streams (GstElement * element)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (element);
  HRESULT res;

  if (self->input->video_enabled && (!self->input->audiosrc
          || self->input->audio_enabled)
      && (GST_STATE (self) == GST_STATE_PLAYING
          || GST_STATE_PENDING (self) == GST_STATE_PLAYING)) {
    GST_DEBUG_OBJECT (self, "Starting streams");

    res = self->input->input->StartStreams ();
    if (res != S_OK) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          (NULL), ("Failed to start streams: 0x%08x", res));
      return;
    }

    self->input->started = TRUE;
    self->input->clock_restart = TRUE;

    // Need to unlock to get the clock time
    g_mutex_unlock (&self->input->lock);

    // Current times of internal and external clock when we go to
    // playing. We need this to convert the pipeline running time
    // to the running time of the hardware
    //
    // We can't use the normal base time for the external clock
    // because we might go to PLAYING later than the pipeline
    self->internal_base_time = gst_clock_get_internal_time (self->input->clock);
    self->external_base_time =
        gst_clock_get_internal_time (GST_ELEMENT_CLOCK (self));

    g_mutex_lock (&self->input->lock);
  } else {
    GST_DEBUG_OBJECT (self, "Not starting streams yet");
  }
}

static GstStateChangeReturn
gst_decklink_video_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_decklink_video_src_open (self)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto out;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock (&self->input->lock);
      self->input->clock_start_time = GST_CLOCK_TIME_NONE;
      self->input->clock_last_time = 0;
      self->input->clock_offset = 0;
      g_mutex_unlock (&self->input->lock);
      gst_element_post_message (element,
          gst_message_new_clock_provide (GST_OBJECT_CAST (element),
              self->input->clock, TRUE));
      self->flushing = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      GstClock *clock;

      clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
      if (clock && clock != self->input->clock) {
        gst_clock_set_master (self->input->clock, clock);
      }
      if (clock)
        gst_object_unref (clock);

      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_element_post_message (element,
          gst_message_new_clock_lost (GST_OBJECT_CAST (element),
              self->input->clock));
      gst_clock_set_master (self->input->clock, NULL);
      g_mutex_lock (&self->input->lock);
      self->input->clock_start_time = GST_CLOCK_TIME_NONE;
      self->input->clock_last_time = 0;
      self->input->clock_offset = 0;
      g_mutex_unlock (&self->input->lock);

      g_queue_foreach (&self->current_frames, (GFunc) capture_frame_free, NULL);
      g_queue_clear (&self->current_frames);
      self->caps_mode = GST_DECKLINK_MODE_AUTO;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:{
      HRESULT res;

      GST_DEBUG_OBJECT (self, "Stopping streams");
      g_mutex_lock (&self->input->lock);
      self->input->started = FALSE;
      g_mutex_unlock (&self->input->lock);

      res = self->input->input->StopStreams ();
      if (res != S_OK) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL), ("Failed to stop streams: 0x%08x", res));
        ret = GST_STATE_CHANGE_FAILURE;
      }
      self->internal_base_time = GST_CLOCK_TIME_NONE;
      self->external_base_time = GST_CLOCK_TIME_NONE;
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      g_mutex_lock (&self->input->lock);
      if (self->input->start_streams)
        self->input->start_streams (self->input->videosrc);
      g_mutex_unlock (&self->input->lock);

      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_decklink_video_src_close (self);
      break;
    default:
      break;
  }
out:

  return ret;
}

static GstClock *
gst_decklink_video_src_provide_clock (GstElement * element)
{
  GstDecklinkVideoSrc *self = GST_DECKLINK_VIDEO_SRC_CAST (element);

  if (!self->input)
    return NULL;

  return GST_CLOCK_CAST (gst_object_ref (self->input->clock));
}
