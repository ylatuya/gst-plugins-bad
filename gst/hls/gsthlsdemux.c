/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (C) 2012, Fluendo S.A <support@fluendo.com>
 *  Author: Andoni Morales Alastruey <amorales@fluendo.com>
 *
 * Gsthlsdemux.c:
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
 * SECTION:element-hlsdemux
 *
 * HTTP Live Streaming demuxer element.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch souphttpsrc location=http://devimages.apple.com/iphone/samples/bipbop/gear4/prog_index.m3u8 ! hlsdemux ! decodebin2 ! ffmpegcolorspace ! videoscale ! autovideosink
 * ]|
 * </refsect2>
 *
 * Last reviewed on 2010-10-07
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if HAVE_LIBCRYPTO
#include <openssl/evp.h>
#endif

#include <string.h>
#include <gst/base/gsttypefindhelper.h>
#include "gsthlsdemux.h"
#include "gsthlsdemux-marshal.h"

static GstStaticPadTemplate videosrctemplate =
GST_STATIC_PAD_TEMPLATE ("video%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate audiosrctemplate =
GST_STATIC_PAD_TEMPLATE ("audio%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate subssrctemplate = GST_STATIC_PAD_TEMPLATE ("subs%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-subtitle-webvtt"));

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-hls"));

static GstStaticPadTemplate intsrctemplate =
GST_STATIC_PAD_TEMPLATE ("internal%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_hls_demux_debug);
#define GST_CAT_DEFAULT gst_hls_demux_debug

enum
{
  SIGNAL_VIDEO_CHANGED,
  SIGNAL_AUDIO_CHANGED,
  SIGNAL_TEXT_CHANGED,
  SIGNAL_STREAMS_CHANGED,
  SIGNAL_VIDEO_TAGS_CHANGED,
  SIGNAL_AUDIO_TAGS_CHANGED,
  SIGNAL_TEXT_TAGS_CHANGED,
  SIGNAL_GET_VIDEO_TAGS,
  SIGNAL_GET_AUDIO_TAGS,
  SIGNAL_GET_TEXT_TAGS,
  LAST_SIGNAL
};

enum
{
  PROP_0,

  PROP_FRAGMENTS_CACHE,
  PROP_BITRATE_LIMIT,
  PROP_CONNECTION_SPEED,
  PROP_MAX_RESOLUTION,
  PROP_ADAPTATION_ALGORITHM,
  PROP_BITRATES,
  PROP_N_VIDEO,
  PROP_CURRENT_VIDEO,
  PROP_N_AUDIO,
  PROP_CURRENT_AUDIO,
  PROP_N_TEXT,
  PROP_CURRENT_TEXT,
  PROP_LAST
};

enum
{
  GST_HLS_ADAPTATION_ALWAYS_LOWEST,
  GST_HLS_ADAPTATION_ALWAYS_HIGHEST,
  GST_HLS_ADAPTATION_BANDWIDTH_ESTIMATION,
  GST_HLS_ADAPTATION_FIXED_BITRATE,
  GST_HLS_ADAPTATION_ROTATION,
  GST_HLS_ADAPTATION_DISABLED,
  GST_HLS_ADAPTATION_CUSTOM,
};

#define GST_HLS_ADAPTATION_ALGORITHM_TYPE (gst_hls_adaptation_algorithm_get_type())
static GType
gst_hls_adaptation_algorithm_get_type (void)
{
  static GType algorithm_type = 0;

  static const GEnumValue algorithm_types[] = {
    {GST_HLS_ADAPTATION_ALWAYS_LOWEST, "Always lowest bitrate", "lowest"},
    {GST_HLS_ADAPTATION_ALWAYS_HIGHEST, "Always highest bitrate", "highest"},
    {GST_HLS_ADAPTATION_BANDWIDTH_ESTIMATION, "Based on bandwidth estimation",
        "bandwidth"},
    {GST_HLS_ADAPTATION_FIXED_BITRATE,
        "Fixed bitrate using the connection speed", "fixed"},
    {GST_HLS_ADAPTATION_ROTATION,
          "Rotates the selected stream for each fragment " "(for debug only)",
        "rotation"},
    {GST_HLS_ADAPTATION_DISABLED, "Disables adaptive switching", "disabled"},
    {0, NULL, NULL}
  };

  if (!algorithm_type) {
    algorithm_type =
        g_enum_register_static ("GstHLSAdaptionAlgorithm", algorithm_types);
  }
  return algorithm_type;
}

static const float update_interval_factor[] = { 1, 0.5 };

static guint gst_hls_demux_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_FRAGMENTS_CACHE 3
#define DEFAULT_FAILED_COUNT 3
#define DEFAULT_BITRATE_LIMIT 0.8
#define DEFAULT_CONNECTION_SPEED    0
#define DEFAULT_ADAPTATION_ALGORITHM GST_HLS_ADAPTATION_BANDWIDTH_ESTIMATION
#define DEFAULT_MAX_RESOLUTION NULL
#define GST_HLS_DEMUX_EMPTY_FRAGMENT "a34dC9Pi8gsEMce8fked"

#define GST_HLS_DEMUX_SUPPORTED_AUDIO_CAPS "audio/mpeg;audio/x-vorbis"
#define GST_HLS_DEMUX_SUPPORTED_VIDEO_CAPS "video/x-h264;video/x-vp8"
#define GST_HLS_DEMUX_SUPPORTED_AV_CAPS GST_HLS_DEMUX_SUPPORTED_AUDIO_CAPS ";" \
  GST_HLS_DEMUX_SUPPORTED_VIDEO_CAPS

#define GST_HLS_DEMUX_PADS_LOCK(x) g_mutex_lock(x->pads_lock)
#define GST_HLS_DEMUX_PADS_UNLOCK(x) g_mutex_unlock(x->pads_lock)
#define GST_HLS_DEMUX_DEMUX_SWITCH_LOCK(x) g_mutex_lock(x->demux_switch_lock)
#define GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK(x) g_mutex_unlock(x->demux_switch_lock)
#define GST_HLS_DEMUX_DEMUX_SWITCH_COND_SIGNAL(x) g_cond_signal(x->demux_switch_cond)
#define GST_HLS_DEMUX_DEMUX_SWITCH_COND_WAIT(x) \
    g_cond_wait (x->demux_switch_cond, x->demux_switch_lock);
#define GST_HLS_DEMUX_PAD_CHANGED(p) (p->active && p->pad == NULL) ||\
      (!p->active && p->pad != NULL)
#define GST_HLS_DEMUX_PAD_IS_EOS(p) (p->is_eos || !gst_pad_is_linked(p->pad))

/* GObject */
static void gst_hls_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_hls_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_hls_demux_dispose (GObject * obj);

/* GstElement */
static GstStateChangeReturn
gst_hls_demux_change_state (GstElement * element, GstStateChange transition);

/* GstHLSDemux */
static GstFlowReturn gst_hls_demux_chain (GstPad * pad, GstBuffer * buf);
static gboolean gst_hls_demux_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_hls_demux_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_hls_demux_src_query (GstPad * pad, GstQuery * query);
static void gst_hls_demux_stream_loop (GstHLSDemux * demux);
static void gst_hls_demux_updates_loop (GstHLSDemux * demux);
static void gst_hls_demux_stop (GstHLSDemux * demux);
static void gst_hls_demux_pause_tasks (GstHLSDemux * demux, gboolean caching);
static gboolean gst_hls_demux_cache_fragments (GstHLSDemux * demux);
static gboolean gst_hls_demux_schedule (GstHLSDemux * demux);
static gboolean gst_hls_demux_switch_playlist (GstHLSDemux * demux);
static gboolean gst_hls_demux_get_next_fragment (GstHLSDemux * demux,
    gboolean caching);
static gboolean gst_hls_demux_update_playlist (GstHLSDemux * demux,
    gboolean update);
static gboolean gst_hls_demux_change_playlist (GstHLSDemux * demux,
    guint32 target_bitrate);
static void gst_hls_demux_reset (GstHLSDemux * demux, gboolean dispose);
static gboolean gst_hls_demux_set_location (GstHLSDemux * demux,
    const gchar * uri);
static gchar *gst_hls_src_buf_to_utf8_playlist (GstBuffer * buf);
static void gst_hls_demux_update_adaptation_algorithm (GstHLSDemux * demux);
static void gst_hls_demux_select_stream (GstHLSDemux * demux,
    GstM3U8MediaType type);
static void gst_hls_demux_create_streams (GstHLSDemux * demux);
static GstTagList *gst_hls_demux_get_audio_tags (GstHLSDemux * demux, gint id);
static GstTagList *gst_hls_demux_get_video_tags (GstHLSDemux * demux, gint id);
static GstTagList *gst_hls_demux_get_text_tags (GstHLSDemux * demux, gint id);
static void gst_hls_demux_av_pad_added (GstElement * el, GstPad * pad,
    GstHLSDemux * demux);
static void gst_hls_demux_audio_pad_added (GstElement * el, GstPad * pad,
    GstHLSDemux * demux);
static void gst_hls_demux_switch_pads (GstHLSDemux * demux,
    GstHLSDemuxPadData * pdata, gboolean new_pads);
static void gst_hls_demux_no_more_pads_cb (GstElement * el,
    GstHLSDemux * demux);

static GstHLSDemuxPadData *
gst_hls_demux_pad_data_new (GstHLSDemux * demux, GstM3U8MediaType type)
{
  GstHLSDemuxPadData *pdata;
  gchar *pad_name;

  pdata = g_new0 (GstHLSDemuxPadData, 1);

  pdata->pad = NULL;
  pdata->pad_to_link = NULL;
  pdata->type = type;
  if (type == GST_M3U8_MEDIA_TYPE_VIDEO) {
    pdata->signal = &gst_hls_demux_signals[SIGNAL_VIDEO_CHANGED];
    pdata->desc = "video";
    pdata->pad_template = &videosrctemplate;
  } else if (type == GST_M3U8_MEDIA_TYPE_AUDIO) {
    pdata->signal = &gst_hls_demux_signals[SIGNAL_AUDIO_CHANGED];
    pdata->desc = "audio";
    pdata->pad_template = &audiosrctemplate;
  } else {
    pdata->signal = &gst_hls_demux_signals[SIGNAL_TEXT_CHANGED];
    pdata->desc = "subtitles";
    pdata->pad_template = &subssrctemplate;
  }

  pdata->current_stream = -1;
  pdata->need_segment = TRUE;
  pdata->pad_added = FALSE;
  pdata->streams = g_hash_table_new (g_direct_hash, g_direct_equal);
  pdata->queue = g_queue_new ();

  /* Internal pad used to push fragments downstream */
  pad_name = g_strdup_printf ("i_%s", pdata->desc);
  pdata->in_pad = gst_pad_new_from_static_template (&intsrctemplate, pad_name);
  gst_pad_set_element_private (pdata->in_pad, GST_ELEMENT (demux));
  g_free (pad_name);

  gst_pad_set_event_function (pdata->in_pad, gst_hls_demux_src_event);
  gst_pad_set_query_function (pdata->in_pad, gst_hls_demux_src_query);
  gst_pad_set_element_private (pdata->in_pad, demux);

  return pdata;
}

static void
gst_hls_demux_pad_data_reset (GstHLSDemuxPadData * pdata)
{
  pdata->current_stream = -1;
  pdata->need_segment = TRUE;
  pdata->pad_added = FALSE;
  pdata->is_eos = TRUE;

  while (!g_queue_is_empty (pdata->queue)) {
    GstFragment *fragment = g_queue_pop_head (pdata->queue);
    g_object_unref (fragment);
  }
  g_queue_clear (pdata->queue);

  if (pdata->streams) {
    g_hash_table_unref (pdata->streams);
  }
  pdata->streams = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
gst_hls_demux_pad_data_free (GstHLSDemuxPadData * pdata)
{
  gst_hls_demux_pad_data_reset (pdata);

  if (pdata->queue != NULL) {
    g_queue_free (pdata->queue);
    pdata->queue = NULL;
  }

  if (pdata->streams) {
    g_hash_table_unref (pdata->streams);
    pdata->streams = NULL;
  }

  if (pdata->in_pad != NULL) {
    gst_object_unref (pdata->in_pad);
    pdata->in_pad = NULL;
  }

  g_free (pdata);
}

static void
gst_hls_demux_pad_data_prepare_seek (GstHLSDemuxPadData * pdata)
{
  while (!g_queue_is_empty (pdata->queue)) {
    GstFragment *fragment = g_queue_pop_head (pdata->queue);
    g_object_unref (fragment);
  }
  g_queue_clear (pdata->queue);
  pdata->need_segment = TRUE;
}

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (gst_hls_demux_debug, "hlsdemux", 0,
      "hlsdemux element");
}

GST_BOILERPLATE_FULL (GstHLSDemux, gst_hls_demux, GstBin,
    GST_TYPE_BIN, _do_init);

static void
gst_hls_demux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &videosrctemplate);

  gst_element_class_add_static_pad_template (element_class, &audiosrctemplate);

  gst_element_class_add_static_pad_template (element_class, &subssrctemplate);

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_details_simple (element_class,
      "HLS Demuxer",
      "Demuxer/URIList",
      "HTTP Live Streaming demuxer",
      "Marc-Andre Lureau <marcandre.lureau@gmail.com>\n"
      "Andoni Morales Alastruey <ylatuya@gmail.com>");
}

static void
gst_hls_demux_dispose (GObject * obj)
{
  GstHLSDemux *demux = GST_HLS_DEMUX (obj);

  if (demux->stream_task) {
    if (GST_TASK_STATE (demux->stream_task) != GST_TASK_STOPPED) {
      GST_DEBUG_OBJECT (demux, "Leaving streaming task");
      GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
      GST_HLS_DEMUX_DEMUX_SWITCH_COND_SIGNAL (demux);
      GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
      gst_task_stop (demux->stream_task);
      g_static_rec_mutex_lock (&demux->stream_lock);
      g_static_rec_mutex_unlock (&demux->stream_lock);
      gst_task_join (demux->stream_task);
    }
    gst_object_unref (demux->stream_task);
    g_static_rec_mutex_free (&demux->stream_lock);
    g_mutex_free (demux->demux_switch_lock);
    g_cond_free (demux->demux_switch_cond);
    demux->stream_task = NULL;
  }

  if (demux->updates_task) {
    if (GST_TASK_STATE (demux->updates_task) != GST_TASK_STOPPED) {
      GST_DEBUG_OBJECT (demux, "Leaving updates task");
      demux->cancelled = TRUE;
      gst_uri_downloader_cancel (demux->downloader);
      gst_task_stop (demux->updates_task);
      g_mutex_lock (demux->updates_timed_lock);
      GST_TASK_SIGNAL (demux->updates_task);
      g_mutex_unlock (demux->updates_timed_lock);
      g_static_rec_mutex_lock (&demux->updates_lock);
      g_static_rec_mutex_unlock (&demux->updates_lock);
      gst_task_join (demux->updates_task);
    }
    gst_object_unref (demux->updates_task);
    g_mutex_free (demux->updates_timed_lock);
    g_static_rec_mutex_free (&demux->updates_lock);
    demux->updates_task = NULL;
  }

  if (demux->downloader != NULL) {
    g_object_unref (demux->downloader);
    demux->downloader = NULL;
  }

  gst_hls_demux_reset (demux, TRUE);

  if (demux->max_resolution != NULL) {
    g_free (demux->max_resolution);
    demux->max_resolution = NULL;
  }

  if (demux->pads_lock != NULL) {
    g_mutex_free (demux->pads_lock);
    demux->pads_lock = NULL;
  }

  gst_hls_demux_pad_data_free (demux->video_srcpad);
  gst_hls_demux_pad_data_free (demux->audio_srcpad);
  gst_hls_demux_pad_data_free (demux->subtt_srcpad);

  if (demux->adaptation != NULL) {
    gst_hls_adaptation_free (demux->adaptation);
    demux->adaptation = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_hls_demux_class_init (GstHLSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstHLSDemuxClass *hlsdemux_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  hlsdemux_class = (GstHLSDemuxClass *) klass;

  gobject_class->set_property = gst_hls_demux_set_property;
  gobject_class->get_property = gst_hls_demux_get_property;
  gobject_class->dispose = gst_hls_demux_dispose;

  hlsdemux_class->get_audio_tags = gst_hls_demux_get_audio_tags;
  hlsdemux_class->get_video_tags = gst_hls_demux_get_video_tags;
  hlsdemux_class->get_text_tags = gst_hls_demux_get_text_tags;

  g_object_class_install_property (gobject_class, PROP_FRAGMENTS_CACHE,
      g_param_spec_uint ("fragments-cache", "Fragments cache",
          "Number of fragments needed to be cached to start playing",
          2, G_MAXUINT, DEFAULT_FRAGMENTS_CACHE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATE_LIMIT,
      g_param_spec_float ("bitrate-limit",
          "Bitrate limit in %",
          "Limit of the available bitrate to use when switching to alternates.",
          0, 1, DEFAULT_BITRATE_LIMIT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONNECTION_SPEED,
      g_param_spec_uint ("connection-speed", "Connection Speed",
          "Network connection speed in kbps (0 = unknown)",
          0, G_MAXUINT / 1000, DEFAULT_CONNECTION_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_RESOLUTION,
      g_param_spec_string ("max-resolution", "Max resolution",
          "Maximum supported resolution in \"WxH\" format (NULL = no limit)",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATES,
      g_param_spec_string ("streams-bitrate", "Streams bitrate",
          "Space-separated list of the available streams bitrates",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstHLSDemux:n-video
   *
   * Get the total number of available video streams.
   */
  g_object_class_install_property (gobject_class, PROP_N_VIDEO,
      g_param_spec_int ("n-video", "Number Video",
          "Total number of video streams", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux:current-video
   *
   * Get or set the currently playing video stream. By default the first video
   * stream with data is played.
   */
  g_object_class_install_property (gobject_class, PROP_CURRENT_VIDEO,
      g_param_spec_int ("current-video", "Current Video",
          "Currently playing video stream (-1 = auto)",
          -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux:n-audio
   *
   * Get the total number of available audio streams.
   */
  g_object_class_install_property (gobject_class, PROP_N_AUDIO,
      g_param_spec_int ("n-audio", "Number Audio",
          "Total number of audio streams", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux:current-audio
   *
   * Get or set the currently playing video stream. By default the first audio
   * stream with data is played.
   */
  g_object_class_install_property (gobject_class, PROP_CURRENT_AUDIO,
      g_param_spec_int ("current-audio", "Current Audio",
          "Currently playing video stream (-1 = auto)",
          -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ADAPTATION_ALGORITHM,
      g_param_spec_enum ("adaptation-algorithm", "Adaptation Algorithm",
          "Algorithm used for the stream bitrate selection",
          GST_HLS_ADAPTATION_ALGORITHM_TYPE,
          DEFAULT_ADAPTATION_ALGORITHM,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux:n-text
   *
   * Get the total number of available text streams.
   */
  g_object_class_install_property (gobject_class, PROP_N_TEXT,
      g_param_spec_int ("n-text", "Number Text",
          "Total number of text streams", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux:current-text
   *
   * Get or set the currently playing text stream. By default no text stream
   * stream will be played.
   */
  g_object_class_install_property (gobject_class, PROP_CURRENT_TEXT,
      g_param_spec_int ("current-text", "Current Text",
          "Currently playing text stream (-1 = auto)",
          -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHLSDemux::video-changed
   * @hlsdemux: a #GstHLSDemux
   *
   * This signal is emitted whenever the number or order of the video
   * streams has changed. The application will most likely want to select
   * a new video stream.
   *
   * This signal is usually emitted from the context of a GStreamer streaming
   * thread. You can use gst_message_new_application() and
   * gst_element_post_message() to notify your application's main thread.
   */
  gst_hls_demux_signals[SIGNAL_VIDEO_CHANGED] =
      g_signal_new ("video-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstHLSDemuxClass, video_changed), NULL, NULL,
      gst_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstHLSDemux::audio-changed
   * @hlsdemux: a #GstHLSDemux
   *
   * This signal is emitted whenever the number or order of the audio
   * streams has changed. The application will most likely want to select
   * a new audio stream.
   *
   * This signal may be emitted from the context of a GStreamer streaming thread.
   * You can use gst_message_new_application() and gst_element_post_message()
   * to notify your application's main thread.
   */
  gst_hls_demux_signals[SIGNAL_AUDIO_CHANGED] =
      g_signal_new ("audio-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstHLSDemuxClass, audio_changed), NULL, NULL,
      gst_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstHLSDemux::text-changed
   * @hlsdemux: a #GstHLSDemux
   *
   * This signal is emitted whenever the number or order of the text
   * streams has changed. The application will most likely want to select
   * a new text stream.
   *
   * This signal may be emitted from the context of a GStreamer streaming thread.
   * You can use gst_message_new_application() and gst_element_post_message()
   * to notify your application's main thread.
   */
  gst_hls_demux_signals[SIGNAL_TEXT_CHANGED] =
      g_signal_new ("text-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstHLSDemuxClass, text_changed), NULL, NULL,
      gst_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstHLSDemux::streams-changed
   * @hlsdemux: a #GstHLSDemux
   *
   * This signal is emitted whenever the number or order of the streams
   * streams has changed. The application will most likely want to select
   * a new stream stream.
   *
   * This signal may be emitted from the context of a GStreamer streaming thread.
   * You can use gst_message_new_application() and gst_element_post_message()
   * to notify your application's main thread.
   */
  gst_hls_demux_signals[SIGNAL_STREAMS_CHANGED] =
      g_signal_new ("streams-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstHLSDemuxClass, streams_changed), NULL, NULL,
      gst_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstHLSDemux::get-video-tags
   * @hlsdemux: a #GstHLSDemux
   * @stream: a video stream number
   *
   * Action signal to retrieve the tags of a specific video stream number.
   * This information can be used to select a stream.
   *
   * Returns: a GstTagList with tags or NULL when the stream number does not
   * exist.
   */
  gst_hls_demux_signals[SIGNAL_GET_VIDEO_TAGS] =
      g_signal_new ("get-video-tags", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstHLSDemuxClass, get_video_tags), NULL, NULL,
      gst_hls_demux_marshal_BOXED__INT, GST_TYPE_TAG_LIST, 1, G_TYPE_INT);

  /**
   * GstPlayBin2::get-audio-tags
   * @hlsdemux: a #GstHLSDemux
   * @stream: an audio stream number
   *
   * Action signal to retrieve the tags of a specific audio stream number.
   * This information can be used to select a stream.
   *
   * Returns: a GstTagList with tags or NULL when the stream number does not
   * exist.
   */
  gst_hls_demux_signals[SIGNAL_GET_AUDIO_TAGS] =
      g_signal_new ("get-audio-tags", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstHLSDemuxClass, get_audio_tags), NULL, NULL,
      gst_hls_demux_marshal_BOXED__INT, GST_TYPE_TAG_LIST, 1, G_TYPE_INT);

  /**
   * GstPlayBin2::get-text-tags
   * @hlsdemux: a #GstHLSDemux
   * @stream: a text stream number
   *
   * Action signal to retrieve the tags of a specific text stream number.
   * This information can be used to select a stream.
   *
   * Returns: a GstTagList with tags or NULL when the stream number does not
   * exist.
   */
  gst_hls_demux_signals[SIGNAL_GET_TEXT_TAGS] =
      g_signal_new ("get-text-tags", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstHLSDemuxClass, get_text_tags), NULL, NULL,
      gst_hls_demux_marshal_BOXED__INT, GST_TYPE_TAG_LIST, 1, G_TYPE_INT);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_hls_demux_change_state);
}

static void
gst_hls_demux_init (GstHLSDemux * demux, GstHLSDemuxClass * klass)
{
  GstCaps *caps;

  /* sink pad */
  demux->sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_chain));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_sink_event));
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  /* Downloader */
  demux->downloader = gst_uri_downloader_new ();

  /* Properties */
  demux->fragments_cache = DEFAULT_FRAGMENTS_CACHE;
  demux->bitrate_limit = DEFAULT_BITRATE_LIMIT;
  demux->connection_speed = DEFAULT_CONNECTION_SPEED;
  demux->max_resolution = DEFAULT_MAX_RESOLUTION;
  demux->adaptation_algo = DEFAULT_ADAPTATION_ALGORITHM;

  /* Streams adaptation */
  demux->adaptation = gst_hls_adaptation_new ();
  gst_hls_adaptation_set_max_bitrate (demux->adaptation, DEFAULT_BITRATE_LIMIT);
  gst_hls_adaptation_set_connection_speed (demux->adaptation,
      DEFAULT_CONNECTION_SPEED);
  gst_hls_demux_update_adaptation_algorithm (demux);

  /* Updates task */
  g_static_rec_mutex_init (&demux->updates_lock);
  demux->updates_task =
      gst_task_create ((GstTaskFunction) gst_hls_demux_updates_loop, demux);
  gst_task_set_lock (demux->updates_task, &demux->updates_lock);
  demux->updates_timed_lock = g_mutex_new ();

  /* Streaming task */
  g_static_rec_mutex_init (&demux->stream_lock);
  demux->stream_task =
      gst_task_create ((GstTaskFunction) gst_hls_demux_stream_loop, demux);
  gst_task_set_lock (demux->stream_task, &demux->stream_lock);

  demux->pads_lock = g_mutex_new ();

  /* Create the source ghost pads */
  demux->video_srcpad = gst_hls_demux_pad_data_new (demux,
      GST_M3U8_MEDIA_TYPE_VIDEO);
  demux->audio_srcpad = gst_hls_demux_pad_data_new (demux,
      GST_M3U8_MEDIA_TYPE_AUDIO);
  demux->subtt_srcpad = gst_hls_demux_pad_data_new (demux,
      GST_M3U8_MEDIA_TYPE_SUBTITLES);

  /* Demuxer for the main stream */
  demux->avdemux = gst_element_factory_make ("decodebin2", "avdemux");
  caps = gst_caps_from_string (GST_HLS_DEMUX_SUPPORTED_AV_CAPS);
  g_object_set (demux->avdemux, "caps", caps, "max-size-time",
      5 * GST_SECOND, NULL);
  gst_caps_unref (caps);
  g_signal_connect (demux->avdemux, "pad-added",
      G_CALLBACK (gst_hls_demux_av_pad_added), demux);
  g_signal_connect (demux->avdemux, "no-more-pads",
      G_CALLBACK (gst_hls_demux_no_more_pads_cb), demux);

  /* Demuxer for the audio stream */
  demux->ademux = gst_element_factory_make ("decodebin2", "ademux");
  caps = gst_caps_from_string (GST_HLS_DEMUX_SUPPORTED_AUDIO_CAPS);
  g_object_set (demux->ademux, "caps", caps, "max-size-time",
      5 * GST_SECOND, NULL);
  g_object_set (demux->ademux, "caps", caps, NULL);
  gst_caps_unref (caps);
  g_signal_connect (demux->ademux, "pad-added",
      G_CALLBACK (gst_hls_demux_audio_pad_added), demux);
  g_signal_connect (demux->ademux, "no-more-pads",
      G_CALLBACK (gst_hls_demux_no_more_pads_cb), demux);

  /* Demuxer for the subtitles stream */
  demux->identity = gst_element_factory_make ("identity", "identity");

  demux->demux_switch_cond = g_cond_new ();
  demux->demux_switch_lock = g_mutex_new ();
}

static void
gst_hls_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHLSDemux *demux = GST_HLS_DEMUX (object);

  switch (prop_id) {
    case PROP_FRAGMENTS_CACHE:
      demux->fragments_cache = g_value_get_uint (value);
      break;
    case PROP_BITRATE_LIMIT:
      demux->bitrate_limit = g_value_get_float (value);
      gst_hls_adaptation_set_max_bitrate (demux->adaptation,
          demux->bitrate_limit);
      break;
    case PROP_CONNECTION_SPEED:
      demux->connection_speed = g_value_get_uint (value) * 1000;
      gst_hls_adaptation_set_connection_speed (demux->adaptation,
          demux->connection_speed);
      break;
    case PROP_ADAPTATION_ALGORITHM:
      demux->adaptation_algo = g_value_get_enum (value);
      gst_hls_demux_update_adaptation_algorithm (demux);
      break;
    case PROP_CURRENT_VIDEO:
      demux->video_srcpad->current_stream = g_value_get_int (value);
      gst_hls_demux_select_stream (demux, GST_M3U8_MEDIA_TYPE_VIDEO);
      break;
    case PROP_CURRENT_AUDIO:
      demux->audio_srcpad->current_stream = g_value_get_int (value);
      gst_hls_demux_select_stream (demux, GST_M3U8_MEDIA_TYPE_AUDIO);
      break;
    case PROP_CURRENT_TEXT:
      demux->subtt_srcpad->current_stream = g_value_get_int (value);
      gst_hls_demux_select_stream (demux, GST_M3U8_MEDIA_TYPE_SUBTITLES);
      break;
    case PROP_MAX_RESOLUTION:
      demux->max_resolution = g_value_dup_string (value);
      gst_m3u8_client_set_max_resolution (demux->client, demux->max_resolution);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_hls_demux_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstHLSDemux *demux = GST_HLS_DEMUX (object);

  switch (prop_id) {
    case PROP_FRAGMENTS_CACHE:
      g_value_set_uint (value, demux->fragments_cache);
      break;
    case PROP_BITRATE_LIMIT:
      g_value_set_float (value, demux->bitrate_limit);
      break;
    case PROP_CONNECTION_SPEED:
      g_value_set_uint (value, demux->connection_speed / 1000);
      break;
    case PROP_N_AUDIO:
      g_value_set_int (value, g_hash_table_size (demux->audio_srcpad->streams));
      break;
    case PROP_N_VIDEO:
      g_value_set_int (value, g_hash_table_size (demux->video_srcpad->streams));
      break;
    case PROP_N_TEXT:
      g_value_set_int (value, g_hash_table_size (demux->subtt_srcpad->streams));
      break;
    case PROP_CURRENT_VIDEO:
      g_value_set_int (value, demux->video_srcpad->current_stream);
      break;
    case PROP_CURRENT_AUDIO:
      g_value_set_int (value, demux->audio_srcpad->current_stream);
      break;
    case PROP_CURRENT_TEXT:
      g_value_set_int (value, demux->subtt_srcpad->current_stream);
      break;
    case PROP_ADAPTATION_ALGORITHM:
      g_value_set_enum (value, demux->adaptation_algo);
      break;
    case PROP_MAX_RESOLUTION:
      g_value_set_string (value, demux->max_resolution);
      break;
    case PROP_BITRATES:{
      GList *list;
      GString *string;

      if (demux->client == NULL) {
        g_value_set_string (value, "");
      } else {
        string = g_string_new ("");

        for (list = gst_m3u8_client_get_streams_bitrates (demux->client);
            list; list = list->next) {
          g_string_append_printf (string, "%d ", GPOINTER_TO_UINT (list->data));
        }
        g_value_set_string (value, string->str);
        g_string_free (string, TRUE);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_hls_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstHLSDemux *demux = GST_HLS_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GstStructure *s;
      GstMessage *msg;

      s = gst_structure_new ("stream-selector", NULL);
      msg = gst_message_new_element (GST_OBJECT (element), s);
      gst_element_post_message (GST_ELEMENT (element), msg);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_hls_demux_reset (demux, FALSE);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /* Start the streaming loop in paused only if we already received
         the main playlist. It might have been stopped if we were in PAUSED
         state and we filled our queue with enough cached fragments
       */
      if (gst_m3u8_client_get_uri (demux->client)[0] != '\0')
        gst_task_start (demux->updates_task);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      demux->cancelled = TRUE;
      gst_uri_downloader_cancel (demux->downloader);
      gst_task_stop (demux->updates_task);
      g_mutex_lock (demux->updates_timed_lock);
      GST_TASK_SIGNAL (demux->updates_task);
      g_mutex_unlock (demux->updates_timed_lock);
      g_static_rec_mutex_lock (&demux->updates_lock);
      g_static_rec_mutex_unlock (&demux->updates_lock);
      demux->cancelled = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      demux->cancelled = TRUE;
      gst_hls_demux_stop (demux);
      gst_task_join (demux->stream_task);
      gst_hls_demux_reset (demux, FALSE);
      break;
    default:
      break;
  }
  return ret;
}

static gboolean
gst_hls_demux_push_event (GstHLSDemux * demux, GstEvent * event)
{
  gboolean ret = TRUE;

  if (demux->video_srcpad->pad != NULL) {
    gst_event_ref (event);
    ret &= gst_pad_push_event (demux->video_srcpad->in_pad, event);
  }
  if (demux->audio_srcpad->pad != NULL) {
    gst_event_ref (event);
    ret &= gst_pad_push_event (demux->audio_srcpad->in_pad, event);
  }
  if (demux->subtt_srcpad->pad != NULL) {
    gst_event_ref (event);
    ret &= gst_pad_push_event (demux->subtt_srcpad->in_pad, event);
  }
  gst_event_unref (event);

  return ret;
}

static gboolean
gst_hls_demux_seek (GstHLSDemux * demux, gint64 start, gint64 stop,
    gdouble rate, GstSeekFlags flags)
{
  demux->rate = rate;
  demux->i_frames_mode = (rate > 1 || rate < -1);

  if (rate < 0)
    start = stop;

  demux->seek_start = start;
  if (!gst_m3u8_client_seek (demux->client, start)) {
    GST_WARNING_OBJECT (demux, "Could not find seeked fragment");
    return FALSE;
  }

  if (flags & GST_SEEK_FLAG_FLUSH) {
    GST_DEBUG_OBJECT (demux, "sending flush start");
    gst_hls_demux_push_event (demux, gst_event_new_flush_start ());
    GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
    demux->flushing = TRUE;
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
  }

  demux->cancelled = TRUE;
  gst_task_pause (demux->stream_task);
  gst_uri_downloader_cancel (demux->downloader);
  gst_task_stop (demux->updates_task);
  g_mutex_lock (demux->updates_timed_lock);
  GST_TASK_SIGNAL (demux->updates_task);
  g_mutex_unlock (demux->updates_timed_lock);
  g_static_rec_mutex_lock (&demux->updates_lock);
  g_static_rec_mutex_unlock (&demux->updates_lock);
  gst_task_pause (demux->stream_task);

  /* wait for streaming to finish */
  if (flags & GST_SEEK_FLAG_FLUSH) {
    GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
    GST_HLS_DEMUX_DEMUX_SWITCH_COND_SIGNAL (demux);
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
  }
  g_static_rec_mutex_lock (&demux->stream_lock);

  demux->need_cache = TRUE;
  gst_hls_demux_pad_data_prepare_seek (demux->video_srcpad);
  gst_hls_demux_pad_data_prepare_seek (demux->audio_srcpad);
  gst_hls_demux_pad_data_prepare_seek (demux->subtt_srcpad);

  gst_event_replace (&demux->newsegment, NULL);
  if (demux->rate < 0)
    demux->newsegment =
        gst_event_new_new_segment (FALSE, demux->rate, GST_FORMAT_TIME, 0,
        start, start);
  else
    demux->newsegment =
        gst_event_new_new_segment (FALSE, demux->rate, GST_FORMAT_TIME,
        start, GST_CLOCK_TIME_NONE, start);

  if (flags & GST_SEEK_FLAG_FLUSH) {
    GST_DEBUG_OBJECT (demux, "sending flush stop");
    gst_hls_demux_push_event (demux, gst_event_new_flush_stop ());
    GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
    demux->flushing = FALSE;
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
  }

  demux->cancelled = FALSE;
  demux->seek_pending = TRUE;
  demux->bitrate_switched = TRUE;
  gst_task_start (demux->stream_task);
  g_static_rec_mutex_unlock (&demux->stream_lock);
  return TRUE;

}

static gboolean
gst_hls_demux_src_event (GstPad * pad, GstEvent * event)
{
  GstHLSDemux *demux;

  demux = GST_HLS_DEMUX (gst_pad_get_element_private (pad));

  switch (event->type) {
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;

      GST_INFO_OBJECT (demux, "Received GST_EVENT_SEEK");

      if (gst_m3u8_client_is_live (demux->client)) {
        GST_WARNING_OBJECT (demux, "Received seek event for live stream");
        return FALSE;
      }

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
          &stop_type, &stop);

      if (format != GST_FORMAT_TIME)
        return FALSE;

      GST_DEBUG_OBJECT (demux, "seek event, rate: %f start: %" GST_TIME_FORMAT
          " stop: %" GST_TIME_FORMAT, rate, GST_TIME_ARGS (start),
          GST_TIME_ARGS (stop));

      if (rate != 1) {
        if (!gst_m3u8_client_supports_trick_modes (demux->client)) {
          GST_WARNING_OBJECT (demux,
              "Trick modes not supported, missing I-Frame playlist");
          return FALSE;
        }
      }

      return gst_hls_demux_seek (demux, start, stop, rate, flags);
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, event);
}

static gboolean
gst_hls_demux_sink_event (GstPad * pad, GstEvent * event)
{
  GstHLSDemux *demux;
  GstQuery *query;
  gboolean ret;
  gchar *uri;

  demux = GST_HLS_DEMUX (gst_pad_get_parent (pad));

  switch (event->type) {
    case GST_EVENT_EOS:{
      gchar *playlist = NULL;

      if (demux->playlist == NULL) {
        GST_WARNING_OBJECT (demux, "Received EOS without a playlist.");
        break;
      }

      GST_DEBUG_OBJECT (demux,
          "Got EOS on the sink pad: main playlist fetched");

      query = gst_query_new_uri ();
      ret = gst_pad_peer_query (demux->sinkpad, query);
      if (ret) {
        gst_query_parse_uri (query, &uri);
        gst_hls_demux_set_location (demux, uri);
        g_free (uri);
      }
      gst_query_unref (query);

      playlist = gst_hls_src_buf_to_utf8_playlist (demux->playlist);
      demux->playlist = NULL;
      if (playlist == NULL) {
        GST_WARNING_OBJECT (demux, "Error validating first playlist.");
      } else if (!gst_m3u8_client_parse_main_playlist (demux->client, playlist)) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the playlist */
        GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid playlist."),
            (NULL));
        gst_object_unref (demux);
        return FALSE;
      }

      gst_hls_demux_create_streams (demux);

      if (!ret && gst_m3u8_client_is_live (demux->client)) {
        GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
            ("Failed querying the playlist uri, "
                "required for live sources."), (NULL));
        gst_object_unref (demux);
        return FALSE;
      }

      gst_task_start (demux->stream_task);
      gst_event_unref (event);
      gst_object_unref (demux);
      return TRUE;
    }
    case GST_EVENT_NEWSEGMENT:
      /* Swallow newsegments, we'll push our own */
      gst_event_unref (event);
      gst_object_unref (demux);
      return TRUE;
    case GST_EVENT_QOS:{
      gdouble proportion;
      GstClockTimeDiff diff;
      GstClockTime timestamp;

      gst_event_parse_qos (event, &proportion, &diff, &timestamp);
      gst_hls_adaptation_update_qos_proportion (demux->adaptation, proportion);
      break;
    }
    default:
      break;
  }

  gst_object_unref (demux);

  return gst_pad_event_default (pad, event);
}

static gboolean
gst_hls_demux_src_query (GstPad * pad, GstQuery * query)
{
  GstHLSDemux *hlsdemux;
  gboolean ret = FALSE;

  if (query == NULL)
    return FALSE;

  hlsdemux = GST_HLS_DEMUX (gst_pad_get_element_private (pad));

  switch (query->type) {
    case GST_QUERY_DURATION:{
      GstClockTime duration = -1;
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        duration = gst_m3u8_client_get_duration (hlsdemux->client);
        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          ret = TRUE;
        }
      }
      GST_LOG_OBJECT (hlsdemux, "GST_QUERY_DURATION returns %s with duration %"
          GST_TIME_FORMAT, ret ? "TRUE" : "FALSE", GST_TIME_ARGS (duration));
      break;
    }
    case GST_QUERY_URI:
      if (hlsdemux->client) {
        /* FIXME: Do we answer with the variant playlist, with the current
         * playlist or the the uri of the least downlowaded fragment? */
        gst_query_set_uri (query, gst_m3u8_client_get_uri (hlsdemux->client));
        ret = TRUE;
      }
      break;
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gint64 stop = -1;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      GST_INFO_OBJECT (hlsdemux, "Received GST_QUERY_SEEKING with format %d",
          fmt);
      if (fmt == GST_FORMAT_TIME) {
        GstClockTime duration;

        duration = gst_m3u8_client_get_duration (hlsdemux->client);
        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0)
          stop = duration;

        gst_query_set_seeking (query, fmt,
            !gst_m3u8_client_is_live (hlsdemux->client), 0, stop);
        ret = TRUE;
        GST_INFO_OBJECT (hlsdemux, "GST_QUERY_SEEKING returning with stop : %"
            GST_TIME_FORMAT, GST_TIME_ARGS (stop));
      }
      break;
    }
    default:
      /* Don't fordward queries upstream because of the special nature of this
       * "demuxer", which relies on the upstream element only to be fed with the
       * first playlist */
      break;
  }

  return ret;
}

static GstFlowReturn
gst_hls_demux_chain (GstPad * pad, GstBuffer * buf)
{
  GstHLSDemux *demux = GST_HLS_DEMUX (gst_pad_get_parent (pad));

  if (demux->playlist == NULL)
    demux->playlist = buf;
  else
    demux->playlist = gst_buffer_join (demux->playlist, buf);

  gst_object_unref (demux);

  return GST_FLOW_OK;
}

static void
gst_hls_demux_update_adaptation_algorithm (GstHLSDemux * demux)
{
  switch (demux->adaptation_algo) {
    case GST_HLS_ADAPTATION_ALWAYS_LOWEST:
      demux->algo_func = gst_hls_adaptation_always_lowest;
      break;
    case GST_HLS_ADAPTATION_ALWAYS_HIGHEST:
      demux->algo_func = gst_hls_adaptation_always_highest;
      break;
    case GST_HLS_ADAPTATION_BANDWIDTH_ESTIMATION:
      demux->algo_func = gst_hls_adaptation_bandwidth_estimation;
      break;
    case GST_HLS_ADAPTATION_FIXED_BITRATE:
      demux->algo_func = gst_hls_adaptation_fixed_bitrate;
      break;
    case GST_HLS_ADAPTATION_ROTATION:
      demux->algo_func = gst_hls_adaptation_rotation;
      break;
    case GST_HLS_ADAPTATION_DISABLED:
      demux->algo_func = gst_hls_adaptation_disabled;
      break;
    case GST_HLS_ADAPTATION_CUSTOM:
      return;
    default:
      g_assert_not_reached ();
  }
  gst_hls_adaptation_set_algorithm_func (demux->adaptation, demux->algo_func);
}

void
gst_hls_demux_set_adaptation_algorithm_func (GstHLSDemux * demux,
    GstHLSAdaptationAlgorithmFunc func)
{
  demux->adaptation_algo = GST_HLS_ADAPTATION_CUSTOM;
  gst_hls_adaptation_set_algorithm_func (demux->adaptation, demux->algo_func);
}

static void
gst_hls_demux_pause_tasks (GstHLSDemux * demux, gboolean caching)
{
  if (GST_TASK_STATE (demux->updates_task) != GST_TASK_STOPPED) {
    demux->cancelled = TRUE;
    gst_uri_downloader_cancel (demux->downloader);
    gst_task_pause (demux->updates_task);
    if (!caching)
      g_mutex_lock (demux->updates_timed_lock);
    GST_TASK_SIGNAL (demux->updates_task);
    if (!caching)
      g_mutex_unlock (demux->updates_timed_lock);
  }

  if (GST_TASK_STATE (demux->stream_task) != GST_TASK_STOPPED) {
    demux->stop_stream_task = TRUE;
    gst_task_pause (demux->stream_task);
  }
}

static void
gst_hls_demux_stop (GstHLSDemux * demux)
{
  gst_uri_downloader_cancel (demux->downloader);

  if (GST_TASK_STATE (demux->updates_task) != GST_TASK_STOPPED) {
    demux->cancelled = TRUE;
    gst_uri_downloader_cancel (demux->downloader);
    gst_task_stop (demux->updates_task);
    g_mutex_lock (demux->updates_timed_lock);
    GST_TASK_SIGNAL (demux->updates_task);
    g_mutex_unlock (demux->updates_timed_lock);
    g_static_rec_mutex_lock (&demux->updates_lock);
    g_static_rec_mutex_unlock (&demux->updates_lock);
  }

  if (GST_TASK_STATE (demux->stream_task) != GST_TASK_STOPPED) {
    demux->stop_stream_task = TRUE;
    GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
    GST_HLS_DEMUX_DEMUX_SWITCH_COND_SIGNAL (demux);
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
    gst_task_stop (demux->stream_task);
    g_static_rec_mutex_lock (&demux->stream_lock);
    g_static_rec_mutex_unlock (&demux->stream_lock);
  }
}

static GstHLSDemuxPadData *
gst_hls_demux_get_pad (GstHLSDemux * demux, GstM3U8MediaType type)
{
  if (type == GST_M3U8_MEDIA_TYPE_VIDEO)
    return demux->video_srcpad;
  else if (type == GST_M3U8_MEDIA_TYPE_AUDIO)
    return demux->audio_srcpad;
  else if (type == GST_M3U8_MEDIA_TYPE_SUBTITLES)
    return demux->subtt_srcpad;
  else
    return NULL;
}

static void
gst_hls_demux_select_stream (GstHLSDemux * demux, GstM3U8MediaType type)
{
  GstHLSDemuxPadData *pdata;
  const gchar *alt_name;

  pdata = gst_hls_demux_get_pad (demux, type);

  /* The list of streams is ordered with the default one first */
  if (pdata->current_stream == -1)
    pdata->current_stream = 0;

  if (pdata->current_stream >= g_hash_table_size (pdata->streams)) {
    GST_WARNING_OBJECT (demux, "Invalid stream id %d, "
        "selecting the deafult option", pdata->current_stream);
    pdata->current_stream = 0;
  }

  alt_name = g_hash_table_lookup (pdata->streams,
      GINT_TO_POINTER (pdata->current_stream));
  GST_INFO_OBJECT (demux, "Switch to %s stream %s", pdata->desc, alt_name);
  gst_m3u8_client_set_alternate (demux->client, type, alt_name);

  GST_HLS_DEMUX_PADS_LOCK (demux);
  demux->stream_changed = TRUE;
  GST_HLS_DEMUX_PADS_UNLOCK (demux);

  g_signal_emit (demux, *(pdata->signal), 0);
}

static void
gst_hls_demux_add_tags (GstHLSDemux * demux, GstTagList * list, gchar * title,
    guint bitrate, gchar * lang)
{
  if (title)
    gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_TITLE, title, NULL);

  if (bitrate)
    gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
        bitrate, NULL);

  if (lang != NULL)
    gst_tag_list_add (list, GST_TAG_MERGE_APPEND, GST_TAG_LANGUAGE_CODE, lang,
        NULL);
}

static GstTagList *
gst_hls_demux_get_audio_tags (GstHLSDemux * demux, gint id)
{
  GstTagList *list;
  gchar *alt, *lang = NULL, *title = NULL;

  list = gst_tag_list_new ();

  alt =
      g_hash_table_lookup (demux->audio_srcpad->streams, GINT_TO_POINTER (id));
  if (!alt)
    return list;

  if (!gst_m3u8_client_audio_stream_info (demux->client, alt, &lang, &title))
    return list;

  gst_hls_demux_add_tags (demux, list, title, 0, lang);
  if (lang)
    g_free (lang);
  if (title)
    g_free (title);

  return list;
}

static GstTagList *
gst_hls_demux_get_video_tags (GstHLSDemux * demux, gint id)
{
  GstTagList *list;
  guint bitrate = 0;
  gchar *alt, *title = NULL;

  list = gst_tag_list_new ();

  alt =
      g_hash_table_lookup (demux->video_srcpad->streams, GINT_TO_POINTER (id));
  if (!alt)
    return list;

  if (!gst_m3u8_client_video_stream_info (demux->client, alt, &bitrate, &title))
    return list;

  gst_hls_demux_add_tags (demux, list, title, bitrate, NULL);
  if (title)
    g_free (title);

  return list;
}

static GstTagList *
gst_hls_demux_get_text_tags (GstHLSDemux * demux, gint id)
{
  GstTagList *list;
  gchar *alt, *lang = NULL, *title = NULL;

  list = gst_tag_list_new ();

  alt =
      g_hash_table_lookup (demux->subtt_srcpad->streams, GINT_TO_POINTER (id));
  if (!alt)
    return list;

  if (!gst_m3u8_client_subs_stream_info (demux->client, alt, &lang, &title))
    return list;

  gst_hls_demux_add_tags (demux, list, title, 0, lang);
  if (lang)
    g_free (lang);
  if (title)
    g_free (title);

  return list;
}

static void
gst_hls_demux_create_streams (GstHLSDemux * demux)
{
  GList *walk;
  gint index;

  for (walk = gst_m3u8_client_get_alternates (demux->client,
          GST_M3U8_MEDIA_TYPE_AUDIO); walk; walk = walk->next) {
    index = g_hash_table_size (demux->audio_srcpad->streams);
    g_hash_table_insert (demux->audio_srcpad->streams, GINT_TO_POINTER (index),
        g_strdup ((gchar *) walk->data));
  }

  for (walk = gst_m3u8_client_get_alternates (demux->client,
          GST_M3U8_MEDIA_TYPE_VIDEO); walk; walk = walk->next) {
    index = g_hash_table_size (demux->video_srcpad->streams);
    g_hash_table_insert (demux->video_srcpad->streams, GINT_TO_POINTER (index),
        g_strdup ((gchar *) walk->data));
  }

  for (walk = gst_m3u8_client_get_alternates (demux->client,
          GST_M3U8_MEDIA_TYPE_SUBTITLES); walk; walk = walk->next) {
    index = g_hash_table_size (demux->subtt_srcpad->streams);
    g_hash_table_insert (demux->subtt_srcpad->streams, GINT_TO_POINTER (index),
        g_strdup ((gchar *) walk->data));
  }

  for (walk = demux->client->main->streams; walk; walk = walk->next) {
    GstM3U8Stream *stream = GST_M3U8_STREAM (walk->data);
    gst_hls_adaptation_add_stream (demux->adaptation, stream->bandwidth);
  }

  /* trigger the streams changed */
  g_signal_emit (demux, gst_hls_demux_signals[SIGNAL_STREAMS_CHANGED], 0);
}

static void
gst_hls_demux_prepare_pads (GstHLSDemux * demux, GstHLSDemuxPadData * pdata)
{
  if (G_UNLIKELY (pdata == demux->video_srcpad && !demux->avdemux_added)) {
    /* If we have fragments in the main stream add the demuxer and link our
     * internal pad */
    GST_DEBUG_OBJECT (demux, "Adding video demuxer");
    gst_bin_add (GST_BIN (demux), demux->avdemux);
    gst_element_set_state (demux->avdemux, GST_STATE_PLAYING);
    gst_pad_link (demux->video_srcpad->in_pad,
        gst_element_get_static_pad (demux->avdemux, "sink"));
    gst_pad_activate_push (demux->video_srcpad->in_pad, TRUE);
    demux->avdemux_added = TRUE;
  }
  if (G_UNLIKELY (pdata == demux->audio_srcpad && !demux->ademux_added)) {
    /* If we have fragments in the audio stream add the audio demuxer and link
     * our internal pad */
    GST_DEBUG_OBJECT (demux, "Adding audio demuxer");
    gst_bin_add (GST_BIN (demux), demux->ademux);
    gst_element_set_state (demux->ademux, GST_STATE_PLAYING);
    gst_pad_link (demux->audio_srcpad->in_pad,
        gst_element_get_static_pad (demux->ademux, "sink"));
    gst_pad_activate_push (demux->audio_srcpad->in_pad, TRUE);
    demux->ademux_added = TRUE;
  }
  if (G_UNLIKELY (pdata == demux->subtt_srcpad && !demux->identity_added)) {
    /* If we have fragments in the subtitles stream active our internal pad and
     * update the source ghost pad */
    GST_DEBUG_OBJECT (demux, "Adding subtitles identity");
    gst_bin_add (GST_BIN (demux), demux->identity);
    gst_element_set_state (demux->identity, GST_STATE_PLAYING);
    gst_pad_link (demux->subtt_srcpad->in_pad,
        gst_element_get_static_pad (demux->identity, "sink"));
    demux->subtt_srcpad->pad_to_link =
        gst_element_get_static_pad (demux->identity, "src");
    demux->subtt_srcpad->active = TRUE;
    demux->subtt_srcpad->is_eos = FALSE;
    demux->identity_added = TRUE;
  }
}

static gboolean
gst_hls_demux_push_fragment (GstHLSDemux * demux, GstM3U8MediaType type)
{
  GstFragment *fragment;
  GstBufferList *buffer_list;
  GstBuffer *buf;
  GstFlowReturn ret;
  GstHLSDemuxPadData *pdata;
  gboolean need_segment;

  pdata = gst_hls_demux_get_pad (demux, type);

  GST_HLS_DEMUX_PADS_LOCK (demux);
  if (g_queue_is_empty (pdata->queue)) {
    GST_HLS_DEMUX_PADS_UNLOCK (demux);
    return TRUE;
  }

  fragment = g_queue_pop_head (pdata->queue);
  if (!g_strcmp0 (fragment->name, GST_HLS_DEMUX_EMPTY_FRAGMENT)) {
    g_object_unref (fragment);
    GST_HLS_DEMUX_PADS_UNLOCK (demux);
    return TRUE;
  }

  gst_hls_demux_prepare_pads (demux, pdata);
  need_segment = pdata->need_segment;
  GST_HLS_DEMUX_PADS_UNLOCK (demux);

  buffer_list = gst_fragment_get_buffer_list (fragment);
  /* Work with the first buffer of the list */
  buf = gst_buffer_list_get (buffer_list, 0, 0);

  GST_DEBUG_OBJECT (demux, "Pushing %s fragment ts:%" GST_TIME_FORMAT
      " dur:%" GST_TIME_FORMAT, pdata->desc,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf))
      );

  if (demux->newsegment == NULL) {
    demux->newsegment =
        gst_event_new_new_segment (FALSE, demux->rate, GST_FORMAT_TIME,
        fragment->start_time, GST_CLOCK_TIME_NONE, fragment->start_time);
  }

  if (need_segment) {
    gint64 start;
    GstEvent *event;

    /* The mpegts demuxer will create its own newsegment events for dummy
     * segments or forward the upstream newsegment event assuming that upstream
     * knows about the timestamps in the stream.
     * When a seek happens, for instance with a start time of 0:20:00,000
     * the demuxer will forward it. If the PTS diff is 0:30:00,000, the first
     * buffer pushed by the demuxer will have a TS of 0:50:00,000 which will
     * fill immediately the multiqueue thinking it has already 0:30:00,000 of
     * buffers and only one of the pads of the demuxer will be exposed.
     * In this case we let the demuxer create its own segment*/
    if (pdata->type != GST_M3U8_MEDIA_TYPE_VIDEO) {
      event = gst_event_ref (demux->newsegment);
      gst_event_parse_new_segment (event, NULL, NULL, NULL, &start, NULL, NULL);
      /* And send a newsegment */
      GST_DEBUG_OBJECT (demux, "Sending new-segment. segment start:%"
          GST_TIME_FORMAT, GST_TIME_ARGS (start));
      gst_pad_push_event (pdata->in_pad, event);
    }
  }
  g_object_unref (fragment);

  ret = gst_pad_push_list (pdata->in_pad, buffer_list);
  if (ret != GST_FLOW_OK) {
    if (ret == GST_FLOW_NOT_LINKED && type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
      GST_WARNING_OBJECT (demux, "stream stopped, subtitles not linked");
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED) {
      GST_ELEMENT_ERROR (demux, STREAM, FAILED, (NULL),
          ("stream stopped, reason %s", gst_flow_get_name (ret)));
      gst_hls_demux_push_event (demux, gst_event_new_eos ());
      gst_hls_demux_pause_tasks (demux, FALSE);
      return FALSE;
    } else {
      GST_WARNING_OBJECT (demux, "stream stopped, reason %s",
          gst_flow_get_name (ret));
    }
  }
  return TRUE;
}

static gboolean
gst_hls_demux_check_for_discont (GstHLSDemux * demux)
{
  GstFragment *afrag, *vfrag, *sfrag;

  vfrag = GST_FRAGMENT (g_queue_peek_head (demux->video_srcpad->queue));
  afrag = GST_FRAGMENT (g_queue_peek_head (demux->audio_srcpad->queue));
  sfrag = GST_FRAGMENT (g_queue_peek_head (demux->audio_srcpad->queue));

  return vfrag->discontinuous || afrag->discontinuous || sfrag->discontinuous;
}

static void
gst_hls_demux_push_video_fragment (GstHLSDemux * demux)
{
  gst_hls_demux_push_fragment (demux, GST_M3U8_MEDIA_TYPE_VIDEO);
}

static void
gst_hls_demux_push_audio_fragment (GstHLSDemux * demux)
{
  gst_hls_demux_push_fragment (demux, GST_M3U8_MEDIA_TYPE_AUDIO);
}

static void
gst_hls_demux_push_subtt_fragment (GstHLSDemux * demux)
{
  gst_hls_demux_push_fragment (demux, GST_M3U8_MEDIA_TYPE_SUBTITLES);
}

static gboolean
gst_hls_demux_push_eos (GstHLSDemux * demux)
{
  if (demux->avdemux_added) {
    gst_pad_push_event (demux->video_srcpad->in_pad,
        gst_event_ref (demux->demux_switch_eos));
  }
  if (demux->ademux_added) {
    gst_pad_push_event (demux->audio_srcpad->in_pad,
        gst_event_ref (demux->demux_switch_eos));
  }
  return FALSE;
}

static void
gst_hls_demux_drain_streams (GstHLSDemux * demux)
{
  GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);

  demux->demux_switch_eos = gst_event_new_eos ();
  demux->draining = TRUE;
  /* This can block for the audio stream so it needs to be pushed from a
   * different thread and the main thread can't be used either */
  g_thread_new ("eos", (GThreadFunc) gst_hls_demux_push_eos, demux);
  GST_HLS_DEMUX_DEMUX_SWITCH_COND_WAIT (demux);
  demux->draining = FALSE;

  if (demux->flushing) {
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
    return;
  }
  gst_event_replace (&demux->demux_switch_eos, NULL);
  GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);

  GST_HLS_DEMUX_PADS_LOCK (demux);
  if (demux->avdemux_added) {
    GST_DEBUG_OBJECT (demux, "Removing video demuxer");
    g_object_ref (demux->avdemux);
    gst_element_set_state (demux->avdemux, GST_STATE_READY);
    gst_bin_remove (GST_BIN (demux), demux->avdemux);
    demux->avdemux_added = FALSE;
  }
  if (demux->ademux_added) {
    GST_DEBUG_OBJECT (demux, "Removing audio demuxer");
    g_object_ref (demux->ademux);
    gst_element_set_state (demux->ademux, GST_STATE_READY);
    gst_bin_remove (GST_BIN (demux), demux->ademux);
    demux->ademux_added = FALSE;
  }
  if (demux->identity_added) {
    GST_DEBUG_OBJECT (demux, "Removing subtitles identity");
    g_object_ref (demux->identity);
    gst_element_set_state (demux->identity, GST_STATE_READY);
    gst_bin_remove (GST_BIN (demux), demux->identity);
    demux->identity_added = FALSE;
  }
  demux->avdemux_no_more_pads = FALSE;
  demux->ademux_no_more_pads = FALSE;
  demux->video_srcpad->active = FALSE;
  demux->audio_srcpad->active = FALSE;
  demux->subtt_srcpad->active = FALSE;
  GST_HLS_DEMUX_PADS_UNLOCK (demux);
}

static void
gst_hls_demux_stream_loop (GstHLSDemux * demux)
{
  GThread *video_thread, *audio_thread, *subtt_thread;
  gboolean discont = FALSE;

  /* Loop for the source pad task. The task is started when we have
   * received the main playlist from the source element. It tries first to
   * cache the first fragments and then it waits until it has more data in the
   * queue. This task is woken up when we push a new fragment to the queue or
   * when we reached the end of the playlist  */

  if (G_UNLIKELY (demux->need_cache)) {
    if (!gst_hls_demux_cache_fragments (demux))
      goto cache_error;

    /* we can start now the updates thread (only if on playing) */
    if (GST_STATE (demux) == GST_STATE_PLAYING)
      gst_task_start (demux->updates_task);
    GST_INFO_OBJECT (demux, "First fragments cached successfully");
  }

  GST_HLS_DEMUX_PADS_LOCK (demux);
  if (g_queue_is_empty (demux->video_srcpad->queue) &&
      g_queue_is_empty (demux->audio_srcpad->queue)) {
    if (demux->end_of_playlist)
      goto end_of_playlist;

    goto pause_task;
  }

  if (!demux->i_frames_mode) {
    /* Wait until we have fetched all fragments for the current sequence. This can
     * happen when the video fragment has been already downloaded and the audio
     * is being downloaded. We pause the task and will be started by the updates
     * thread when the download finished. */
    if (g_queue_get_length (demux->video_srcpad->queue) !=
        g_queue_get_length (demux->audio_srcpad->queue))
      goto pause_task;

    /* Check if there is any discontinuity in the stream */
    discont = gst_hls_demux_check_for_discont (demux);
  }
  GST_HLS_DEMUX_PADS_UNLOCK (demux);

  if (discont) {
    gst_hls_demux_drain_streams (demux);
    if (!demux->seek_pending)
      gst_event_replace (&demux->newsegment, NULL);
    demux->seek_pending = FALSE;
  }

  video_thread =
      g_thread_new ("video", (GThreadFunc) gst_hls_demux_push_video_fragment,
      demux);
  audio_thread =
      g_thread_new ("audio", (GThreadFunc) gst_hls_demux_push_audio_fragment,
      demux);
  subtt_thread =
      g_thread_new ("subs", (GThreadFunc) gst_hls_demux_push_subtt_fragment,
      demux);
  g_thread_join (video_thread);
  g_thread_join (audio_thread);
  g_thread_join (subtt_thread);

  return;

end_of_playlist:
  {
    GST_DEBUG_OBJECT (demux, "Reached end of playlist, sending EOS");
    GST_HLS_DEMUX_PADS_UNLOCK (demux);
    gst_hls_demux_push_event (demux, gst_event_new_eos ());
    gst_hls_demux_pause_tasks (demux, FALSE);
    return;
  }

cache_error:
  {
    gst_task_pause (demux->stream_task);
    if (!demux->cancelled) {
      GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
          ("Could not cache the first fragments"), (NULL));
      gst_hls_demux_pause_tasks (demux, FALSE);
    }
    return;
  }

pause_task:
  {
    gst_task_pause (demux->stream_task);
    GST_HLS_DEMUX_PADS_UNLOCK (demux);
    return;
  }
}

static void
gst_hls_demux_reset (GstHLSDemux * demux, gboolean dispose)
{
  if (demux->client) {
    gst_m3u8_client_free (demux->client);
    demux->client = NULL;
  }

  if (!dispose) {
    demux->client = gst_m3u8_client_new ("");
  }

  demux->rate = 1;
  demux->i_frames_mode = FALSE;
  demux->need_init_segment = TRUE;
  demux->need_cache = TRUE;
  demux->end_of_playlist = FALSE;
  demux->cancelled = FALSE;

  /* Bin */
  demux->need_pts_sync = TRUE;
  demux->pts_diff = 0;
  demux->start_ts = GST_CLOCK_TIME_NONE;
  demux->bitrate_switched = FALSE;
  demux->stream_changed = FALSE;
  demux->seek_pending = FALSE;
  demux->seek_start = GST_CLOCK_TIME_NONE;
  gst_event_replace (&demux->newsegment, NULL);
  gst_event_replace (&demux->demux_switch_eos, NULL);

  if (demux->avdemux_added) {
    if (!dispose)
      gst_object_ref (demux->avdemux);
    gst_bin_remove (GST_BIN (demux), demux->avdemux);
    demux->avdemux_added = FALSE;
  }
  if (demux->ademux_added) {
    if (!dispose)
      gst_object_ref (demux->ademux);
    gst_bin_remove (GST_BIN (demux), demux->ademux);
    demux->ademux_added = FALSE;
  }
  if (demux->identity_added) {
    if (!dispose)
      gst_object_ref (demux->identity);
    gst_bin_remove (GST_BIN (demux), demux->identity);
    demux->identity_added = FALSE;
  }

  gst_hls_demux_pad_data_reset (demux->video_srcpad);
  gst_hls_demux_pad_data_reset (demux->audio_srcpad);
  gst_hls_demux_pad_data_reset (demux->subtt_srcpad);

  gst_hls_adaptation_reset (demux->adaptation);
}

static gboolean
gst_hls_demux_set_location (GstHLSDemux * demux, const gchar * uri)
{
  if (demux->client)
    gst_m3u8_client_free (demux->client);
  demux->client = gst_m3u8_client_new (uri);
  gst_m3u8_client_set_max_resolution (demux->client, demux->max_resolution);
  GST_INFO_OBJECT (demux, "Changed location: %s", uri);
  return TRUE;
}

static gboolean
gst_hls_demux_resume_live_playback (GstHLSDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "Resuming playback for current live");
  gst_hls_demux_seek (demux, 0, GST_CLOCK_TIME_NONE, 1, GST_SEEK_FLAG_FLUSH);
  demux->need_pts_sync = TRUE;
  return FALSE;
}

void
gst_hls_demux_updates_loop (GstHLSDemux * demux)
{
  /* Loop for the updates. It's started when the first fragments are cached and
   * schedules the next update of the playlist (for lives sources) and the next
   * update of fragments. When a new fragment is downloaded, it compares the
   * download time with the next scheduled update to check if we can or should
   * switch to a different bitrate */

  g_mutex_lock (demux->updates_timed_lock);
  GST_DEBUG_OBJECT (demux, "Started updates task");
  while (TRUE) {
    if (demux->cancelled)
      goto quit;

    /* schedule the next update */
    gst_hls_demux_schedule (demux);

    /*  block until the next scheduled update or the signal to quit this thread */
    if (g_cond_timed_wait (GST_TASK_GET_COND (demux->updates_task),
            demux->updates_timed_lock, &demux->next_update)) {
      goto resume_playback;
    }

    if (demux->cancelled)
      goto quit;

    /* update the playlist for live sources. For VOD the client will only
     * update the playlists that were not downloaded before */
    if (!gst_hls_demux_update_playlist (demux, TRUE)) {
      if (demux->cancelled)
        goto quit;
      if (demux->client->update_failed_count < DEFAULT_FAILED_COUNT) {
        GST_WARNING_OBJECT (demux, "Could not update the playlist failed:%d",
            demux->client->update_failed_count);
      } else {
        GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
            ("Could not update the playlist"), (NULL));
        goto error;
      }
    }

    if (demux->need_cache) {
      /* With live sources, if we are paused to much time and the playlist has
       * rotated going out the DVR window, we need to restart playback from the
       * current live position. */
      g_timeout_add (0, (GSourceFunc) gst_hls_demux_resume_live_playback,
          demux);
      goto resume_playback;
    }

    if (demux->cancelled)
      goto quit;

    /* fetch the next fragment */
    if (!gst_hls_demux_get_next_fragment (demux, FALSE)) {
      if (demux->cancelled) {
        goto quit;
      } else if (!demux->end_of_playlist && !demux->cancelled) {
        if (demux->client->update_failed_count < DEFAULT_FAILED_COUNT) {
          GST_WARNING_OBJECT (demux, "Could not fetch the next fragment");
          continue;
        } else {
          GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
              ("Could not fetch the next fragment"), (NULL));
          goto error;
        }
      }
    } else {
      if (demux->cancelled)
        goto quit;

      /* try to switch to another bitrate if needed */
      gst_hls_demux_switch_playlist (demux);
    }
  }

quit:
  {
    GST_DEBUG_OBJECT (demux, "Stopped updates task");
    g_mutex_unlock (demux->updates_timed_lock);
    return;
  }

resume_playback:
  {
    GST_DEBUG_OBJECT (demux, "Stopped updates task");
    gst_hls_demux_pause_tasks (demux, TRUE);
    g_mutex_unlock (demux->updates_timed_lock);
    return;
  }

error:
  {
    GST_DEBUG_OBJECT (demux, "Stopped updates task because of error");
    gst_hls_demux_pause_tasks (demux, TRUE);
    g_mutex_unlock (demux->updates_timed_lock);
  }
}

static gboolean
gst_hls_demux_cache_fragments (GstHLSDemux * demux)
{
  gint i;
  GstM3U8Stream *stream = NULL;
  gint target_bitrate;
  guint fragments_cache;

  /* If this playlist is a variant playlist, select the first one
   * and update it */

  /* While caching we don't have to worry about being late fetching
   * so we set 100 seconds as the available time */
  target_bitrate = gst_hls_adaptation_get_target_bitrate (demux->adaptation,
      100 * GST_SECOND);

  /* If we set the connection speed, use it for the first fragments. Otherwise
   * use the default stream selected by the client */
  if (target_bitrate > 0) {
    stream = gst_m3u8_client_get_stream_for_bitrate (demux->client,
        target_bitrate);
    gst_m3u8_client_set_current (demux->client, stream);
  }

  if (!gst_hls_demux_update_playlist (demux, FALSE)) {
    /* Handle the special in which we get a 404 from the first playlist
     * and start with the failover protection. */
    if (target_bitrate == -1) {
      /* If the connexion speed is not set start with the highest bitrate */
      target_bitrate = G_MAXINT32;
    }
    if (!gst_hls_demux_change_playlist (demux, G_MAXINT32)) {
      return FALSE;
    }
  }

  if (demux->seek_start != GST_CLOCK_TIME_NONE) {
    /* We need to seek again since we might have added new playlists */
    if (!gst_m3u8_client_seek (demux->client, demux->seek_start)) {
      GST_WARNING_OBJECT (demux, "Could not find seeked fragment");
      return FALSE;
    }
    demux->seek_start = GST_CLOCK_TIME_NONE;
  }

  if (demux->newsegment == NULL) {
    gst_m3u8_client_get_current_position (demux->client, &demux->start_ts,
        NULL);
    demux->newsegment =
        gst_event_new_new_segment (FALSE, demux->rate, GST_FORMAT_TIME,
        demux->start_ts, GST_CLOCK_TIME_NONE, demux->start_ts);
  }

  if (!gst_m3u8_client_is_live (demux->client)) {
    GstClockTime duration = gst_m3u8_client_get_duration (demux->client);

    GST_DEBUG_OBJECT (demux, "Sending duration message : %" GST_TIME_FORMAT,
        GST_TIME_ARGS (duration));
    if (duration != GST_CLOCK_TIME_NONE)
      gst_element_post_message (GST_ELEMENT (demux),
          gst_message_new_duration (GST_OBJECT (demux),
              GST_FORMAT_TIME, duration));
  }

  /* Cache the first fragments */
  if (demux->i_frames_mode) {
    fragments_cache = demux->fragments_cache + 1;
  } else {
    fragments_cache = demux->fragments_cache;
  }
  for (i = 0; i < fragments_cache - 1; i++) {
    gst_element_post_message (GST_ELEMENT (demux),
        gst_message_new_buffering (GST_OBJECT (demux),
            100 * i / (fragments_cache - 1)));
    g_get_current_time (&demux->next_update);
    if (!gst_hls_demux_get_next_fragment (demux, TRUE)) {
      if (demux->end_of_playlist)
        break;
      if (!demux->cancelled)
        GST_ERROR_OBJECT (demux, "Error caching the first fragments");
      return FALSE;
    }
    /* make sure we stop caching fragments if something cancelled it */
    if (demux->cancelled)
      return FALSE;

    gst_hls_demux_switch_playlist (demux);
  }
  gst_element_post_message (GST_ELEMENT (demux),
      gst_message_new_buffering (GST_OBJECT (demux), 100));

  g_get_current_time (&demux->next_update);

  if (!demux->i_frames_mode) {
    /* We have chached the first fragments - 1 to start playing and cache the last
     * one while playing the first */
    g_time_val_add (&demux->next_update,
        -(gst_m3u8_client_get_current_fragment_duration (demux->client)
            / (gdouble) GST_SECOND * G_USEC_PER_SEC));
  }

  demux->need_cache = FALSE;
  return TRUE;
}

static gchar *
gst_hls_src_buf_to_utf8_playlist (GstBuffer * buf)
{
  gint size;
  gchar *data;
  gchar *playlist;

  data = (gchar *) GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);

  if (!g_utf8_validate (data, size, NULL))
    goto validate_error;

  /* alloc size + 1 to end with a null character */
  playlist = g_malloc0 (size + 1);
  memcpy (playlist, data, size);

  gst_buffer_unref (buf);
  return playlist;

validate_error:
  gst_buffer_unref (buf);
  return NULL;
}

static gchar *
gst_hls_demux_get_playlist_from_fragment (GstHLSDemux * demux,
    GstFragment * fragment)
{
  gchar *playlist;

  playlist =
      gst_hls_src_buf_to_utf8_playlist (gst_fragment_get_buffer (fragment));
  g_object_unref (fragment);

  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Couldn't not validate playlist encoding");
  }

  return playlist;
}

static gboolean
gst_hls_demux_update_playlist (GstHLSDemux * demux, gboolean update)
{
  GstFragment *download;
  gboolean updated = FALSE;
  gchar *v_playlist = NULL, *a_playlist = NULL, *s_playlist = NULL;
  const gchar *video_uri = NULL, *audio_uri = NULL;
  const gchar *subtt_uri = NULL;

  if (demux->i_frames_mode) {
    const gchar *uri = gst_m3u8_client_get_i_frame_uri (demux->client);
    if (uri != NULL) {
      download = gst_uri_downloader_fetch_uri (demux->downloader, uri);
      if (download == NULL)
        return FALSE;
      return gst_m3u8_client_update_i_frame (demux->client,
          gst_hls_demux_get_playlist_from_fragment (demux, download));
    }
    return TRUE;
  }

  gst_m3u8_client_get_current_uri (demux->client, &video_uri, &audio_uri,
      &subtt_uri);

  if (video_uri != NULL) {
    GST_DEBUG_OBJECT (demux, "Updating video playlist %s", video_uri);
    download = gst_uri_downloader_fetch_uri (demux->downloader, video_uri);
    if (download == NULL)
      return FALSE;
    v_playlist = gst_hls_demux_get_playlist_from_fragment (demux, download);
  }

  if (audio_uri != NULL) {
    GST_DEBUG_OBJECT (demux, "Updating audio playlist %s", audio_uri);
    download = gst_uri_downloader_fetch_uri (demux->downloader, audio_uri);
    if (download == NULL)
      return FALSE;
    a_playlist = gst_hls_demux_get_playlist_from_fragment (demux, download);
  }

  if (subtt_uri != NULL) {
    GST_DEBUG_OBJECT (demux, "Updating subtitles playlist %s", subtt_uri);
    download = gst_uri_downloader_fetch_uri (demux->downloader, subtt_uri);
    if (download == NULL)
      return FALSE;
    s_playlist = gst_hls_demux_get_playlist_from_fragment (demux, download);
  }
  if (!gst_m3u8_client_update (demux->client, v_playlist, a_playlist,
          s_playlist, &updated)) {
    return FALSE;
  }

  /*  If it's a live source, do not let the sequence number go beyond
   * three fragments before the end of the list */
  if (updated && update && gst_m3u8_client_is_live (demux->client)) {
    if (!gst_m3u8_client_check_sequence_validity (demux->client)) {
      demux->need_cache = TRUE;
    }
  }

  if (update)
    return updated;
  return TRUE;
}

static gboolean
gst_hls_demux_change_playlist (GstHLSDemux * demux, guint32 target_bitrate)
{
  GstM3U8Stream *previous_stream, *current_stream;
  gint old_bandwidth, new_bandwidth;

  if (target_bitrate == -1)
    return TRUE;

  current_stream = gst_m3u8_client_get_stream_for_bitrate (demux->client,
      target_bitrate);
  previous_stream = demux->client->selected_stream;

retry_failover_protection:
  old_bandwidth = GST_M3U8_STREAM (previous_stream)->bandwidth;
  new_bandwidth = GST_M3U8_STREAM (current_stream)->bandwidth;

  /* Don't do anything else if the playlist is the same */
  if (new_bandwidth == old_bandwidth) {
    return TRUE;
  }

  gst_m3u8_client_set_current (demux->client, current_stream);

  GST_INFO_OBJECT (demux, "Client was on %dbps, target is %dbps, switching"
      " to bitrate %dbps", old_bandwidth, target_bitrate, new_bandwidth);

  if (gst_hls_demux_update_playlist (demux, FALSE)) {
    GstStructure *s;
    const gchar *v_uri, *a_uri, *s_uri;

    gst_m3u8_client_get_current_uri (demux->client, &v_uri, &a_uri, &s_uri);
    s = gst_structure_new ("playlist",
        "uri", G_TYPE_STRING, v_uri,
        "uri-alt", G_TYPE_STRING, a_uri,
        "uri-subtitles", G_TYPE_STRING, s_uri,
        "bitrate", G_TYPE_INT, new_bandwidth, NULL);
    gst_element_post_message (GST_ELEMENT_CAST (demux),
        gst_message_new_element (GST_OBJECT_CAST (demux), s));
  } else {
    GstM3U8Stream *failover = NULL;

    GST_INFO_OBJECT (demux, "Unable to update playlist. Switching back");

    failover = gst_m3u8_client_get_previous_stream (demux->client);
    if (failover && new_bandwidth == GST_M3U8_STREAM (failover)->bandwidth) {
      current_stream = failover;
      goto retry_failover_protection;
    }

    gst_m3u8_client_set_current (demux->client, current_stream);
    /*  Try a lower bitrate (or stop if we just tried the lowest) */
    if (new_bandwidth ==
        GST_M3U8_STREAM (g_list_first (demux->client->main->streams)->
            data)->bandwidth)
      return FALSE;
    else
      return gst_hls_demux_change_playlist (demux, new_bandwidth - 1);
  }

  return TRUE;
}

static gboolean
gst_hls_demux_schedule (GstHLSDemux * demux)
{
  gfloat update_factor;
  gint count;
  GstClockTime next_ts;

  /* As defined in §6.3.4. Reloading the Playlist file:
   * "If the client reloads a Playlist file and finds that it has not
   * changed then it MUST wait for a period of time before retrying.  The
   * minimum delay is a multiple of the target duration.  This multiple is
   * 0.5 for the first attempt, 1.5 for the second, and 3.0 thereafter."
   */
  count = demux->client->update_failed_count;
  if (count == 0)
    update_factor = update_interval_factor[0];
  else
    update_factor = update_interval_factor[1];

  /* schedule the next update using the target duration field of the
   * playlist */
  next_ts = gst_m3u8_client_get_current_fragment_duration (demux->client);

  /* With I-Frame playlists the scheduling is done based on the number of queued
   * frames, we schedule at fixed interval which is 2x faster than what we
   * should have in theory */
  if (demux->i_frames_mode) {
    if (demux->rate > 1) {
      next_ts /= demux->rate * 2;
    } else if (demux->rate < -1) {
      next_ts /= -demux->rate * 2;
    }
  }

  g_time_val_add (&demux->next_update,
      next_ts / (gdouble) GST_SECOND * G_USEC_PER_SEC * update_factor);
  GST_DEBUG_OBJECT (demux, "Next update scheduled at %s",
      g_time_val_to_iso8601 (&demux->next_update));

  return TRUE;
}

static gboolean
gst_hls_demux_switch_playlist (GstHLSDemux * demux)
{
  gint old_bitrate, target_bitrate;
  GTimeVal now;
  gint64 time_avail;
  gboolean ret;

  old_bitrate = demux->client->selected_stream->bandwidth;
  g_get_current_time (&now);
  time_avail = GST_TIMEVAL_TO_TIME (demux->next_update) -
      GST_TIMEVAL_TO_TIME (now);
  target_bitrate = gst_hls_adaptation_get_target_bitrate (demux->adaptation,
      time_avail);

  ret = gst_hls_demux_change_playlist (demux, target_bitrate);
  if (old_bitrate != demux->client->selected_stream->bandwidth) {
    demux->bitrate_switched = TRUE;
    /* trigger the streams changed to update the new bitrate for the selected
     * stream */
    g_signal_emit (demux, gst_hls_demux_signals[SIGNAL_STREAMS_CHANGED], 0);
  }
  return ret;
}

#if HAVE_LIBCRYPTO
static gboolean
gst_hls_demux_decrypt_aes_128 (GstHLSDemux * demux, GstFragment * fragment)
{
  GstBuffer *in = NULL, *out = NULL, *key_buf = NULL;
  EVP_CIPHER_CTX *cryptCtx = NULL;
  GstBufferList *list = NULL;
  GstBufferListIterator *it = NULL;
  GstFragment *key_frag;
  guchar iv[16];
  const gchar *pos = 0;
  gint dec_length = 0, res = 0, i = 0;
  gboolean ret = FALSE;

  out = gst_buffer_new_and_alloc (gst_fragment_get_total_size (fragment));
  GST_BUFFER_SIZE (out) = 0;

  if (fragment->key_url == NULL) {
    GST_ERROR_OBJECT (demux, "The key URL is missing for this fragment");
    goto exit;
  }

  key_frag =
      gst_uri_downloader_fetch_uri (demux->downloader, fragment->key_url);
  if (key_frag == NULL) {
    GST_ERROR_OBJECT (demux, "Could not fetch key from %s", fragment->key_url);
    goto exit;
  }
  key_buf = gst_fragment_get_buffer (key_frag);
  g_object_unref (key_frag);

  /* Parse the IV hexadecimal string */
  if (g_utf8_strlen (fragment->iv, -1) != 2 + 32) {
    GST_ERROR_OBJECT (demux, "The initial vector is not correctly "
        "formatted %s (length:%d)", fragment->iv, g_utf8_strlen (fragment->iv,
            -1));
    goto exit;
  }
  pos = fragment->iv + 2;
  for (i = 0; i < 16; i++) {
    sscanf (pos, "%2hhx", &iv[i]);
    pos += 2;
  }

  /* Initialize the AES-128 CBC decryption cypher with the fragment's key
   * and its initialization vector*/
  cryptCtx = EVP_CIPHER_CTX_new ();
  EVP_CIPHER_CTX_init (cryptCtx);
  res = EVP_DecryptInit_ex (cryptCtx, EVP_aes_128_cbc (), NULL,
      GST_BUFFER_DATA (key_buf), iv);
  gst_buffer_unref (key_buf);
  if (res != 1) {
    GST_ERROR_OBJECT (demux, "Error in EVP_DecryptInit_ex");
    goto exit;
  }

  /* Feed the decryption cipher with the input buffer */
  list = gst_fragment_get_buffer_list (fragment);
  it = gst_buffer_list_iterate (list);
  while (gst_buffer_list_iterator_next_group (it)) {
    while ((in = gst_buffer_list_iterator_next (it)) != NULL) {
      res = EVP_DecryptUpdate (cryptCtx,
          GST_BUFFER_DATA (out) + GST_BUFFER_SIZE (out),
          &dec_length, GST_BUFFER_DATA (in), GST_BUFFER_SIZE (in));
      GST_BUFFER_SIZE (out) += dec_length;
      if (res != 1) {
        GST_ERROR_OBJECT (demux, "Error in EVP_DecryptUpdate");
        goto exit;
      }
    }
  }

  /* Finalize the last block, this shouldn't be needed as the fragment should be
   * padded properly */
  res = EVP_DecryptFinal_ex (cryptCtx,
      GST_BUFFER_DATA (out) + GST_BUFFER_SIZE (out), &dec_length);
  GST_BUFFER_SIZE (out) += dec_length;
  if (res != 1) {
    GST_ERROR_OBJECT (demux, "Error in EVP_DecryptFinal_ex");
    goto exit;
  }

  /* Replace the buffers in the fragment with the decrypted one */
  gst_fragment_clear (fragment);
  gst_fragment_add_buffer (fragment, out);
  fragment->completed = TRUE;
  out = NULL;
  ret = TRUE;

exit:
  if (out != NULL)
    gst_buffer_unref (out);
  if (it != NULL)
    gst_buffer_list_iterator_free (it);
  if (list != NULL)
    gst_buffer_list_unref (list);
  if (cryptCtx != NULL)
    EVP_CIPHER_CTX_cleanup (cryptCtx);
  return ret;
}
#endif

static gboolean
gst_hls_demux_decrypt_fragment (GstHLSDemux * demux, GstFragment * fragment)
{
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (demux, "Decrypting fragment");
  switch (fragment->enc_method) {
    case GST_FRAGMENT_ENCODING_METHOD_NONE:
      ret = TRUE;
      break;
#if HAVE_LIBCRYPTO
    case GST_FRAGMENT_ENCODING_METHOD_AES_128:
      ret = gst_hls_demux_decrypt_aes_128 (demux, fragment);
      break;
#endif
    default:
      GST_ERROR_OBJECT (demux, "Encoding method %d not supported",
          fragment->enc_method);
      ret = FALSE;
      break;
  }

  return ret;
}

static gboolean
gst_hls_demux_fetch_fragment (GstHLSDemux * demux, GstFragment * fragment,
    GstM3U8MediaType type)
{
  GstBufferList *buffer_list;
  GstBuffer *buf;
  GstHLSDemuxPadData *pdata;

  pdata = gst_hls_demux_get_pad (demux, type);

  if (fragment == NULL) {
    /* Create an empty fragment to keep the queues in sync */
    fragment = gst_fragment_new ();
    g_free (fragment->name);
    fragment->name = g_strdup (GST_HLS_DEMUX_EMPTY_FRAGMENT);
    GST_HLS_DEMUX_PADS_LOCK (demux);
    g_queue_push_tail (pdata->queue, fragment);
    GST_HLS_DEMUX_PADS_UNLOCK (demux);
    return TRUE;
  }

  GST_INFO_OBJECT (demux, "Fetching next fragment %s %d@%d", fragment->name,
      fragment->offset, fragment->length);

  fragment = gst_uri_downloader_fetch_fragment (demux->downloader, fragment);

  if (fragment == NULL) {
    if (!demux->cancelled)
      GST_ERROR_OBJECT (demux, "Could not download fragment");
    return FALSE;
  }

  if (type != GST_M3U8_MEDIA_TYPE_SUBTITLES)
    gst_hls_adaptation_add_fragment (demux->adaptation,
        gst_fragment_get_total_size (fragment),
        fragment->download_stop_time - fragment->download_start_time);

  buffer_list = gst_fragment_get_buffer_list (fragment);
  buf = gst_buffer_list_get (buffer_list, 0, 0);
  GST_BUFFER_DURATION (buf) = fragment->stop_time - fragment->start_time;
  GST_BUFFER_TIMESTAMP (buf) = fragment->start_time;
  GST_BUFFER_OFFSET (buf) = 0;
  GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);

  if (fragment->discontinuous) {
    GST_DEBUG_OBJECT (demux, "Marking fragment as discontinuous");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  }

  gst_hls_demux_decrypt_fragment (demux, fragment);

  GST_HLS_DEMUX_PADS_LOCK (demux);
  g_queue_push_tail (pdata->queue, fragment);
  GST_HLS_DEMUX_PADS_UNLOCK (demux);
  gst_buffer_list_unref (buffer_list);

  return TRUE;
}

static gboolean
gst_hls_demux_get_next_fragment (GstHLSDemux * demux, gboolean caching)
{
  gboolean ret = TRUE;
  GstFragment *v_fragment = NULL, *a_fragment = NULL;
  GstFragment *s_fragment = NULL, *i_segment = NULL;

  /* Trick Modes */
  if (demux->i_frames_mode) {
    GList *i_frames = NULL;

    /* Don't let the I-Frames queue go higher than 20 */
    if (g_queue_get_length (demux->video_srcpad->queue) > 10) {
      return TRUE;
    }

    if (demux->rate > 1) {
      ret = gst_m3u8_client_get_next_i_frames (demux->client, &i_segment,
          &i_frames);
    } else {
      ret = gst_m3u8_client_get_prev_i_frames (demux->client, &i_segment,
          &i_frames);
    }

    if (!ret || g_list_length (i_frames) == 0) {
      GST_INFO_OBJECT (demux, "This playlist doesn't contain more fragments");
      demux->end_of_playlist = TRUE;
      gst_task_start (demux->stream_task);
      return FALSE;
    }

    if (demux->need_init_segment) {
      /* This fragment has incorrect timestamp information, which is used to
       * set the new segment start time */
      i_segment->start_time = GST_FRAGMENT (i_frames->data)->start_time;

      if (!gst_hls_demux_fetch_fragment (demux, i_segment,
              GST_M3U8_MEDIA_TYPE_VIDEO))
        goto exit;
      demux->need_init_segment = FALSE;
    }
    GST_LOG_OBJECT (demux, "Adding init_segment");
    for (; i_frames; i_frames = i_frames->next) {
      GstFragment *frag = GST_FRAGMENT (i_frames->data);
      frag->discontinuous = TRUE;
      if (!gst_hls_demux_fetch_fragment (demux, frag,
              GST_M3U8_MEDIA_TYPE_VIDEO))
        goto exit;
      GST_LOG_OBJECT (demux, "Adding I-Frame");
      /* FIXME: only download the first one until we find a way of way of
       * determining the correct number of I-Frame based on the rate */
      break;
    }
    goto start_streaming;
  }

  if (!gst_m3u8_client_get_next_fragment (demux->client, &v_fragment,
          &a_fragment, &s_fragment)) {
    GST_INFO_OBJECT (demux, "This playlist doesn't contain more fragments");
    if (!gst_m3u8_client_is_live (demux->client)) {
      demux->end_of_playlist = TRUE;
    }
    gst_task_start (demux->stream_task);
    return FALSE;
  }

  /* Fetch video fragment */
  if (!gst_hls_demux_fetch_fragment (demux, v_fragment,
          GST_M3U8_MEDIA_TYPE_VIDEO))
    goto error;
  /* Fetch audio fragment */
  if (!gst_hls_demux_fetch_fragment (demux, a_fragment,
          GST_M3U8_MEDIA_TYPE_AUDIO))
    goto error;
  /* Fetch subtitles fragment */
  if (!gst_hls_demux_fetch_fragment (demux, s_fragment,
          GST_M3U8_MEDIA_TYPE_SUBTITLES))
    goto error;

  if (demux->bitrate_switched || demux->stream_changed) {
    if (v_fragment != NULL) {
      v_fragment->discontinuous = TRUE;
    }
    if (a_fragment != NULL) {
      a_fragment->discontinuous = TRUE;
    }
    if (s_fragment != NULL) {
      s_fragment->discontinuous = TRUE;
    }
    demux->bitrate_switched = FALSE;
    demux->stream_changed = FALSE;
  }

start_streaming:
  if (!caching) {
    GST_TASK_SIGNAL (demux->updates_task);
    gst_task_start (demux->stream_task);
  }

exit:
  return ret;

error:
  {
    if (!demux->cancelled) {
      GST_ERROR_OBJECT (demux, "Error fetching fragment");
      gst_hls_demux_stop (demux);
    }
    ret = FALSE;
    goto exit;
  }

}


/**************************************
 *        Bin handling                *
 **************************************/

static gboolean
gst_hls_demux_proxy_newsegment (GstPad * pad, GstEvent * event,
    GstHLSDemux * demux, GstHLSDemuxPadData * pdata)
{
  gboolean ret = FALSE;

  if (event->type == GST_EVENT_NEWSEGMENT) {
    /* For live streams where seeking is not supported forward the upstreams
     * events */
    if (gst_m3u8_client_is_live (demux->client)) {
      if (pdata->need_segment) {
        ret = TRUE;
        pdata->need_segment = FALSE;
      }
      goto exit;
    }
    if (pdata->need_segment) {
      /* Override new segments created by other elements */
      if (event != demux->newsegment) {
        gst_pad_push_event (pdata->pad, gst_event_ref (demux->newsegment));
      } else {
        gst_pad_push_event (pdata->pad, gst_event_ref (event));
      }
      pdata->need_segment = FALSE;
    }
    goto exit;
  }
  ret = TRUE;

  if (event->type == GST_EVENT_EOS) {
    gboolean signal_drained;

    GST_HLS_DEMUX_DEMUX_SWITCH_LOCK (demux);
    if (event == demux->demux_switch_eos) {
      GST_LOG_OBJECT (demux, "Got EOS on %s pad", pdata->desc);
      pdata->is_eos = TRUE;
      ret = FALSE;
    }
    signal_drained = GST_HLS_DEMUX_PAD_IS_EOS (demux->video_srcpad) &&
        GST_HLS_DEMUX_PAD_IS_EOS (demux->audio_srcpad);
    if (signal_drained && demux->draining) {
      GST_LOG_OBJECT (demux, "Signaling drained");
      GST_HLS_DEMUX_DEMUX_SWITCH_COND_SIGNAL (demux);
    }
    GST_HLS_DEMUX_DEMUX_SWITCH_UNLOCK (demux);
  }

exit:
  return ret;
}

static gboolean
gst_hls_demux_proxy_newsegment_video (GstPad * pad, GstEvent * event,
    GstHLSDemux * demux)
{
  return gst_hls_demux_proxy_newsegment (pad, event, demux,
      demux->video_srcpad);
}

static gboolean
gst_hls_demux_proxy_newsegment_audio (GstPad * pad, GstEvent * event,
    GstHLSDemux * demux)
{
  return gst_hls_demux_proxy_newsegment (pad, event, demux,
      demux->audio_srcpad);
}

static gboolean
gst_hls_demux_proxy_newsegment_subtt (GstPad * pad, GstEvent * event,
    GstHLSDemux * demux)
{
  return gst_hls_demux_proxy_newsegment (pad, event, demux,
      demux->subtt_srcpad);
}

static gboolean
gst_hls_demux_retimestamp (GstPad * pad, GstBuffer * buffer,
    GstHLSDemux * demux, const gchar * type)
{
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    if (G_UNLIKELY (demux->need_pts_sync) && demux->avdemux_added) {
      demux->pts_diff = GST_BUFFER_TIMESTAMP (buffer) - demux->start_ts;
      demux->need_pts_sync = FALSE;
      GST_DEBUG_OBJECT (demux, "PTS diff is %" GST_TIME_FORMAT,
          GST_TIME_ARGS (demux->pts_diff));
    }
    GST_BUFFER_TIMESTAMP (buffer) -= demux->pts_diff;
  }

  GST_LOG_OBJECT (demux, "Pushing %s buffer with ts %" GST_TIME_FORMAT,
      type, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  return TRUE;
}

static gboolean
gst_hls_demux_audio_retimestamp (GstPad * pad, GstBuffer * buffer,
    GstHLSDemux * demux)
{
  gboolean ret;

  ret = gst_hls_demux_retimestamp (pad, buffer, demux, "audio");

  return ret;
}

static gboolean
gst_hls_demux_video_retimestamp (GstPad * pad, GstBuffer * buffer,
    GstHLSDemux * demux)
{
  return gst_hls_demux_retimestamp (pad, buffer, demux, "video");
}

static void
gst_hls_demux_pad_blocked (GstElement * el, GstPad * pad)
{
  /* We don't need to do anything here, but we want to block async and a
   * callback is needed instead of NULL */
}

static void
gst_hls_demux_drain_pad (GstHLSDemux * demux, GstHLSDemuxPadData * pdata,
    GstPad * pad)
{
  if (pad == NULL)
    return;

  /* Push out EOS */
  GST_DEBUG_OBJECT (demux, "Draining %s pad", pdata->desc);
  gst_pad_push_event (pad, gst_event_new_eos ());
  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT (demux), pad);
  pdata->pad_added = FALSE;
}

static void
gst_hls_demux_switch_pads (GstHLSDemux * demux, GstHLSDemuxPadData * pdata,
    gboolean new_pads)
{
  GstPad *pad;
  GstPad *oldpad = NULL;

  GST_DEBUG ("Switching %s pads", pdata->desc);

  pad = pdata->pad_to_link;
  if (new_pads || !pdata->active) {
    oldpad = pdata->pad;
  }

  if (pdata->active) {
    if (new_pads) {
      pdata->pad = gst_ghost_pad_new_no_target_from_template (NULL,
          gst_static_pad_template_get (pdata->pad_template));
    }
    if (pdata->type == GST_M3U8_MEDIA_TYPE_VIDEO) {
      gst_pad_add_event_probe (pad,
          G_CALLBACK (gst_hls_demux_proxy_newsegment_video), demux);
    } else if (pdata->type == GST_M3U8_MEDIA_TYPE_AUDIO) {
      gst_pad_add_event_probe (pad,
          G_CALLBACK (gst_hls_demux_proxy_newsegment_audio), demux);
    } else if (pdata->type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
      gst_pad_add_event_probe (pad,
          G_CALLBACK (gst_hls_demux_proxy_newsegment_subtt), demux);
    }
    gst_pad_set_active (pdata->pad, TRUE);
    gst_ghost_pad_set_target (GST_GHOST_PAD (pdata->pad), pad);
    gst_pad_set_blocked_async (pad, FALSE,
        (GstPadBlockCallback) gst_hls_demux_pad_blocked, NULL);
    if (new_pads) {
      gst_element_add_pad (GST_ELEMENT (demux), pdata->pad);
    }
    pdata->pad_added = TRUE;
  }

  if (oldpad) {
    gst_hls_demux_drain_pad (demux, pdata, oldpad);
  }
}

static inline void
_prepare_new_pad (GstHLSDemuxPadData * pdata, GstPad * pad)
{
  pdata->is_eos = FALSE;
  pdata->active = TRUE;
  pdata->pad_to_link = pad;
  gst_pad_set_blocked_async (pad, TRUE,
      (GstPadBlockCallback) gst_hls_demux_pad_blocked, NULL);
}

static void
gst_hls_demux_no_more_pads_cb (GstElement * el, GstHLSDemux * demux)
{
  gboolean new_pads = FALSE;

  if (el == demux->avdemux) {
    demux->avdemux_no_more_pads = TRUE;
  } else if (el == demux->ademux) {
    demux->ademux_no_more_pads = TRUE;
  }

  if ((demux->avdemux_added && !demux->avdemux_no_more_pads) ||
      (demux->ademux_added && !demux->ademux_no_more_pads)) {
    GST_DEBUG_OBJECT (demux, "Waiting for the no-more-pads signal in the "
        "other demuxer");
    return;
  }

  /* Check if the topology of the stream changed and video/audio/subtt
   * pads are going to be added or removed with respect of the last state */
  if (GST_HLS_DEMUX_PAD_CHANGED (demux->video_srcpad) ||
      GST_HLS_DEMUX_PAD_CHANGED (demux->audio_srcpad) ||
      GST_HLS_DEMUX_PAD_CHANGED (demux->subtt_srcpad)) {
    new_pads = TRUE;
    GST_DEBUG_OBJECT (demux, "Stream topology changed, creating a new decoding"
        " chain");
  } else {
    GST_DEBUG_OBJECT (demux, "Keeping old pads");
  }

  gst_hls_demux_switch_pads (demux, demux->video_srcpad, new_pads);
  gst_hls_demux_switch_pads (demux, demux->audio_srcpad, new_pads);
  gst_hls_demux_switch_pads (demux, demux->subtt_srcpad, new_pads);
  if (!demux->video_srcpad->active) {
    demux->video_srcpad->pad = NULL;
  }
  if (!demux->audio_srcpad->active) {
    demux->audio_srcpad->pad = NULL;
  }
  if (!demux->subtt_srcpad->active) {
    demux->subtt_srcpad->pad = NULL;
  }

  gst_element_no_more_pads (GST_ELEMENT (demux));
}

static void
gst_hls_demux_audio_pad_added (GstElement * el, GstPad * pad,
    GstHLSDemux * demux)
{
  GST_HLS_DEMUX_PADS_LOCK (demux);
  _prepare_new_pad (demux->audio_srcpad, pad);
  GST_HLS_DEMUX_PADS_UNLOCK (demux);
}

static void
gst_hls_demux_av_pad_added (GstElement * el, GstPad * pad, GstHLSDemux * demux)
{
  GstCaps *caps;
  GstStructure *s;
  const gchar *mime;

  caps = gst_pad_get_caps_reffed (pad);
  s = gst_caps_get_structure (caps, 0);
  mime = gst_structure_get_name (s);

  GST_HLS_DEMUX_PADS_LOCK (demux);

  if (g_str_has_prefix (mime, "audio")) {
    if (demux->ademux_added) {
      GST_DEBUG_OBJECT (demux, "Skipping unused audio pad");
      goto exit;
    }
    /* The main stream has an audio track muxed */
    GST_DEBUG_OBJECT (demux, "New audio stream with caps %" GST_PTR_FORMAT,
        caps);
    _prepare_new_pad (demux->audio_srcpad, pad);
    if (!gst_m3u8_client_is_live (demux->client)) {
      gst_pad_add_buffer_probe (pad,
          G_CALLBACK (gst_hls_demux_audio_retimestamp), demux);
    }
  } else if (g_str_has_prefix (mime, "video")) {
    /* The main stream has a video track muxed */
    GST_DEBUG_OBJECT (demux, "New video stream with caps %" GST_PTR_FORMAT,
        caps);
    _prepare_new_pad (demux->video_srcpad, pad);
    if (!gst_m3u8_client_is_live (demux->client)) {
      gst_pad_add_buffer_probe (pad,
          G_CALLBACK (gst_hls_demux_video_retimestamp), demux);
    }
  }

exit:
  GST_HLS_DEMUX_PADS_UNLOCK (demux);
  gst_caps_unref (caps);
}
