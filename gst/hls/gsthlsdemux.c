/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
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

/* FIXME 0.11: suppress warnings for deprecated API such as GStaticRecMutex
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <gst/base/gsttypefindhelper.h>
#include <gst/glib-compat-private.h>
#include "gsthlsdemux.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-hls"));

GST_DEBUG_CATEGORY_STATIC (gst_hls_demux_debug);
#define GST_CAT_DEFAULT gst_hls_demux_debug

enum
{
  PROP_0,

  PROP_FRAGMENTS_CACHE,
  PROP_BITRATE_LIMIT,
  PROP_CONNECTION_SPEED,
  PROP_LAST
};

static const float update_interval_factor[] = { 1, 0.5, 1.5, 3 };

#define DEFAULT_FRAGMENTS_CACHE 3
#define DEFAULT_FAILED_COUNT 3
#define DEFAULT_BITRATE_LIMIT 0.8
#define DEFAULT_CONNECTION_SPEED    0

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
static void gst_hls_demux_reset (GstHLSDemux * demux, gboolean dispose);
static gboolean gst_hls_demux_set_location (GstHLSDemux * demux,
    const gchar * uri);
static gchar *gst_hls_src_buf_to_utf8_playlist (GstBuffer * buf);

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (gst_hls_demux_debug, "hlsdemux", 0,
      "hlsdemux element");
}

GST_BOILERPLATE_FULL (GstHLSDemux, gst_hls_demux, GstElement,
    GST_TYPE_ELEMENT, _do_init);

static void
gst_hls_demux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &srctemplate);

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
      gst_task_stop (demux->stream_task);
      g_static_rec_mutex_lock (&demux->stream_lock);
      g_static_rec_mutex_unlock (&demux->stream_lock);
      gst_task_join (demux->stream_task);
    }
    gst_object_unref (demux->stream_task);
    g_static_rec_mutex_free (&demux->stream_lock);
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

  g_queue_free (demux->queue);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_hls_demux_class_init (GstHLSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_hls_demux_set_property;
  gobject_class->get_property = gst_hls_demux_get_property;
  gobject_class->dispose = gst_hls_demux_dispose;

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

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_hls_demux_change_state);
}

static void
gst_hls_demux_init (GstHLSDemux * demux, GstHLSDemuxClass * klass)
{
  /* sink pad */
  demux->sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_chain));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_sink_event));
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  /* Downloader */
  demux->downloader = gst_uri_downloader_new ();

  demux->do_typefind = TRUE;

  /* Properties */
  demux->fragments_cache = DEFAULT_FRAGMENTS_CACHE;
  demux->bitrate_limit = DEFAULT_BITRATE_LIMIT;
  demux->connection_speed = DEFAULT_CONNECTION_SPEED;

  demux->queue = g_queue_new ();

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
      break;
    case PROP_CONNECTION_SPEED:
      demux->connection_speed = g_value_get_uint (value) * 1000;
      break;
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
      GList *walk;
      GstClockTime position, current_pos, target_pos;
      gint current_sequence;
      GstM3U8MediaFile *file;

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

      GST_M3U8_CLIENT_LOCK (demux->client);
      file = GST_M3U8_MEDIA_FILE (demux->client->current->files->data);
      current_sequence = file->sequence;
      current_pos = 0;
      target_pos = (GstClockTime) start;
      for (walk = demux->client->current->files; walk; walk = walk->next) {
        file = walk->data;

        current_sequence = file->sequence;
        if (current_pos <= target_pos
            && target_pos < current_pos + file->duration) {
          break;
        }
        current_pos += file->duration;
      }
      GST_M3U8_CLIENT_UNLOCK (demux->client);

      if (walk == NULL) {
        GST_WARNING_OBJECT (demux, "Could not find seeked fragment");
        return FALSE;
      }

      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_DEBUG_OBJECT (demux, "sending flush start");
        gst_pad_push_event (demux->srcpad, gst_event_new_flush_start ());
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
      g_static_rec_mutex_lock (&demux->stream_lock);

      demux->need_cache = TRUE;
      while (!g_queue_is_empty (demux->queue)) {
        GstBufferList *buf_list = g_queue_pop_head (demux->queue);
        gst_buffer_list_unref (buf_list);
      }
      g_queue_clear (demux->queue);

      GST_M3U8_CLIENT_LOCK (demux->client);
      GST_DEBUG_OBJECT (demux, "seeking to sequence %d", current_sequence);
      demux->client->sequence = current_sequence;
      gst_m3u8_client_get_current_position (demux->client, &position);
      demux->position_shift = start - position;
      demux->need_segment = TRUE;
      GST_M3U8_CLIENT_UNLOCK (demux->client);


      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_DEBUG_OBJECT (demux, "sending flush stop");
        gst_pad_push_event (demux->srcpad, gst_event_new_flush_stop ());
      }

      demux->cancelled = FALSE;
      gst_task_start (demux->stream_task);
      g_static_rec_mutex_unlock (&demux->stream_lock);

      return TRUE;
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
      } else if (!gst_m3u8_client_update (demux->client, playlist)) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the playlist */
        GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid playlist."),
            (NULL));
        gst_object_unref (demux);
        return FALSE;
      }

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
      GST_INFO_OBJECT (hlsdemux, "GST_QUERY_DURATION returns %s with duration %"
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
    gst_task_stop (demux->stream_task);
    g_static_rec_mutex_lock (&demux->stream_lock);
    g_static_rec_mutex_unlock (&demux->stream_lock);
  }
}

static void
switch_pads (GstHLSDemux * demux, GstCaps * newcaps)
{
  GstPad *oldpad = demux->srcpad;

  GST_DEBUG ("Switching pads (oldpad:%p) with caps: %" GST_PTR_FORMAT, oldpad,
      newcaps);

  /* FIXME: This is a workaround for a bug in playsink.
   * If we're switching from an audio-only or video-only fragment
   * to an audio-video segment, the new sink doesn't know about
   * the current running time and audio/video will go out of sync.
   *
   * This should be fixed in playsink by distributing the
   * current running time to newly created sinks and is
   * fixed in 0.11 with the new segments.
   */
  if (demux->srcpad)
    gst_pad_push_event (demux->srcpad, gst_event_new_flush_stop ());

  /* First create and activate new pad */
  demux->srcpad = gst_pad_new_from_static_template (&srctemplate, NULL);
  gst_pad_set_event_function (demux->srcpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_src_event));
  gst_pad_set_query_function (demux->srcpad,
      GST_DEBUG_FUNCPTR (gst_hls_demux_src_query));
  gst_pad_set_element_private (demux->srcpad, demux);
  gst_pad_set_active (demux->srcpad, TRUE);
  gst_pad_set_caps (demux->srcpad, newcaps);
  gst_element_add_pad (GST_ELEMENT (demux), demux->srcpad);

  gst_element_no_more_pads (GST_ELEMENT (demux));

  if (oldpad) {
    /* Push out EOS */
    gst_pad_push_event (oldpad, gst_event_new_eos ());
    gst_pad_set_active (oldpad, FALSE);
    gst_element_remove_pad (GST_ELEMENT (demux), oldpad);
  }
}

static void
gst_hls_demux_stream_loop (GstHLSDemux * demux)
{
  GstFragment *fragment;
  GstBufferList *buffer_list;
  GstBuffer *buf;
  GstFlowReturn ret;

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

  if (g_queue_is_empty (demux->queue)) {
    if (demux->end_of_playlist)
      goto end_of_playlist;

    goto pause_task;
  }

  fragment = g_queue_pop_head (demux->queue);
  buffer_list = gst_fragment_get_buffer_list (fragment);
  /* Work with the first buffer of the list */
  buf = gst_buffer_list_get (buffer_list, 0, 0);

  /* Figure out if we need to create/switch pads */
  if (G_UNLIKELY (!demux->srcpad
          || !gst_caps_is_equal_fixed (GST_BUFFER_CAPS (buf),
              GST_PAD_CAPS (demux->srcpad))
          || demux->need_segment)) {
    switch_pads (demux, GST_BUFFER_CAPS (buf));
    demux->need_segment = TRUE;
  }
  g_object_unref (fragment);

  if (demux->need_segment) {
    GstClockTime start = GST_BUFFER_TIMESTAMP (buf);

    start += demux->position_shift;
    /* And send a newsegment */
    GST_DEBUG_OBJECT (demux, "Sending new-segment. segment start:%"
        GST_TIME_FORMAT, GST_TIME_ARGS (start));
    gst_pad_push_event (demux->srcpad,
        gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
            start, GST_CLOCK_TIME_NONE, start));
    demux->need_segment = FALSE;
    demux->position_shift = 0;
  }

  ret = gst_pad_push_list (demux->srcpad, buffer_list);
  if (ret != GST_FLOW_OK)
    goto error_pushing;

  return;

end_of_playlist:
  {
    GST_DEBUG_OBJECT (demux, "Reached end of playlist, sending EOS");
    gst_pad_push_event (demux->srcpad, gst_event_new_eos ());
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

error_pushing:
  {
    if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED) {
      GST_ELEMENT_ERROR (demux, STREAM, FAILED, (NULL),
          ("stream stopped, reason %s", gst_flow_get_name (ret)));
      gst_pad_push_event (demux->srcpad, gst_event_new_eos ());
    } else {
      GST_DEBUG_OBJECT (demux, "stream stopped, reason %s",
          gst_flow_get_name (ret));
    }
    gst_hls_demux_pause_tasks (demux, FALSE);
    return;
  }

pause_task:
  {
    gst_task_pause (demux->stream_task);
    return;
  }
}

static void
gst_hls_demux_reset (GstHLSDemux * demux, gboolean dispose)
{
  demux->need_cache = TRUE;
  demux->end_of_playlist = FALSE;
  demux->cancelled = FALSE;
  demux->do_typefind = TRUE;

  if (demux->input_caps) {
    gst_caps_unref (demux->input_caps);
    demux->input_caps = NULL;
  }

  if (demux->playlist) {
    gst_buffer_unref (demux->playlist);
    demux->playlist = NULL;
  }

  if (demux->client) {
    gst_m3u8_client_free (demux->client);
    demux->client = NULL;
  }

  if (!dispose) {
    demux->client = gst_m3u8_client_new ("");
  }

  while (!g_queue_is_empty (demux->queue)) {
    GstFragment *fragment = g_queue_pop_head (demux->queue);
    g_object_unref (fragment);
  }
  g_queue_clear (demux->queue);

  demux->position_shift = 0;
  demux->need_segment = TRUE;
}

static gboolean
gst_hls_demux_set_location (GstHLSDemux * demux, const gchar * uri)
{
  if (demux->client)
    gst_m3u8_client_free (demux->client);
  demux->client = gst_m3u8_client_new (uri);
  GST_INFO_OBJECT (demux, "Changed location: %s", uri);
  return TRUE;
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
      goto quit;
    }

    if (demux->cancelled)
      goto quit;

    /* update the playlist for live sources */
    if (gst_m3u8_client_is_live (demux->client)) {
      if (!gst_hls_demux_update_playlist (demux, TRUE)) {
        if (demux->cancelled)
          goto quit;
        demux->client->update_failed_count++;
        if (demux->client->update_failed_count < DEFAULT_FAILED_COUNT) {
          GST_WARNING_OBJECT (demux, "Could not update the playlist");
          continue;
        } else {
          GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
              ("Could not update the playlist"), (NULL));
          goto error;
        }
      }
    }

    /* if it's a live source and the playlist couldn't be updated, there aren't
     * more fragments in the playlist, so we just wait for the next schedulled
     * update */
    if (gst_m3u8_client_is_live (demux->client) &&
        demux->client->update_failed_count > 0) {
      GST_WARNING_OBJECT (demux,
          "The playlist hasn't been updated, failed count is %d",
          demux->client->update_failed_count);
      continue;
    }

    if (demux->cancelled)
      goto quit;

    /* fetch the next fragment */
    if (g_queue_is_empty (demux->queue)) {
      if (!gst_hls_demux_get_next_fragment (demux, FALSE)) {
        if (demux->cancelled) {
          goto quit;
        } else if (!demux->end_of_playlist && !demux->cancelled) {
          demux->client->update_failed_count++;
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
        demux->client->update_failed_count = 0;

        if (demux->cancelled)
          goto quit;

        /* try to switch to another bitrate if needed */
        gst_hls_demux_switch_playlist (demux);
      }
    }
  }

quit:
  {
    GST_DEBUG_OBJECT (demux, "Stopped updates task");
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

  /* If this playlist is a variant playlist, select the first one
   * and update it */
  if (gst_m3u8_client_has_variant_playlist (demux->client)) {
    GstM3U8 *child = NULL;

    if (demux->connection_speed == 0) {

      GST_M3U8_CLIENT_LOCK (demux->client);
      child = demux->client->main->current_variant->data;
      GST_M3U8_CLIENT_UNLOCK (demux->client);
    } else {
      GList *tmp = gst_m3u8_client_get_playlist_for_bitrate (demux->client,
          demux->connection_speed);

      child = GST_M3U8 (tmp->data);
    }

    gst_m3u8_client_set_current (demux->client, child);
    if (!gst_hls_demux_update_playlist (demux, FALSE)) {
      GST_ERROR_OBJECT (demux, "Could not fetch the child playlist %s",
          child->uri);
      return FALSE;
    }
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
  for (i = 0; i < demux->fragments_cache; i++) {
    gst_element_post_message (GST_ELEMENT (demux),
        gst_message_new_buffering (GST_OBJECT (demux),
            100 * i / demux->fragments_cache));
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

static gboolean
gst_hls_demux_update_playlist (GstHLSDemux * demux, gboolean update)
{
  GstFragment *download;
  GstBufferListIterator *it;
  GstBuffer *buf;
  gchar *playlist;
  gboolean updated = FALSE;

  const gchar *uri = gst_m3u8_client_get_current_uri (demux->client);

  download = gst_uri_downloader_fetch_uri (demux->downloader, uri);

  if (download == NULL)
    return FALSE;

  /* Merge all the buffers in the list to build a unique buffer with the
   * playlist */
  it = gst_buffer_list_iterate (gst_fragment_get_buffer_list (download));
  gst_buffer_list_iterator_next_group (it);
  buf = gst_buffer_list_iterator_merge_group (it);

  playlist = gst_hls_src_buf_to_utf8_playlist (buf);
  gst_buffer_list_iterator_free (it);
  g_object_unref (download);

  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Couldn't not validate playlist encoding");
    return FALSE;
  }

  updated = gst_m3u8_client_update (demux->client, playlist);

  /*  If it's a live source, do not let the sequence number go beyond
   * three fragments before the end of the list */
  if (updated && update == FALSE && demux->client->current &&
      gst_m3u8_client_is_live (demux->client)) {
    guint last_sequence;

    GST_M3U8_CLIENT_LOCK (demux->client);
    last_sequence =
        GST_M3U8_MEDIA_FILE (g_list_last (demux->client->current->
            files)->data)->sequence;

    if (demux->client->sequence >= last_sequence - 3) {
      GST_DEBUG_OBJECT (demux, "Sequence is beyond playlist. Moving back to %d",
          last_sequence - 3);
      demux->need_segment = TRUE;
      demux->client->sequence = last_sequence - 3;
    }
    GST_M3U8_CLIENT_UNLOCK (demux->client);
  }

  return updated;
}

static gboolean
gst_hls_demux_change_playlist (GstHLSDemux * demux, guint max_bitrate)
{
  GList *previous_variant, *current_variant;
  gint old_bandwidth, new_bandwidth;

  /* If user specifies a connection speed never use a playlist with a bandwidth
   * superior than it */
  if (demux->connection_speed != 0 && max_bitrate > demux->connection_speed)
    max_bitrate = demux->connection_speed;

  previous_variant = demux->client->main->current_variant;
  current_variant = gst_m3u8_client_get_playlist_for_bitrate (demux->client,
      max_bitrate);

retry_failover_protection:
  old_bandwidth = GST_M3U8 (previous_variant->data)->bandwidth;
  new_bandwidth = GST_M3U8 (current_variant->data)->bandwidth;

  /* Don't do anything else if the playlist is the same */
  if (new_bandwidth == old_bandwidth) {
    return TRUE;
  }

  demux->client->main->current_variant = current_variant;
  GST_M3U8_CLIENT_UNLOCK (demux->client);

  gst_m3u8_client_set_current (demux->client, current_variant->data);

  GST_INFO_OBJECT (demux, "Client was on %dbps, max allowed is %dbps, switching"
      " to bitrate %dbps", old_bandwidth, max_bitrate, new_bandwidth);

  if (gst_hls_demux_update_playlist (demux, FALSE)) {
    GstStructure *s;

    s = gst_structure_new ("playlist",
        "uri", G_TYPE_STRING, gst_m3u8_client_get_current_uri (demux->client),
        "bitrate", G_TYPE_INT, new_bandwidth, NULL);
    gst_element_post_message (GST_ELEMENT_CAST (demux),
        gst_message_new_element (GST_OBJECT_CAST (demux), s));
  } else {
    GList *failover = NULL;

    GST_INFO_OBJECT (demux, "Unable to update playlist. Switching back");
    GST_M3U8_CLIENT_LOCK (demux->client);

    failover = g_list_previous (current_variant);
    if (failover && new_bandwidth == GST_M3U8 (failover->data)->bandwidth) {
      current_variant = failover;
      goto retry_failover_protection;
    }

    demux->client->main->current_variant = previous_variant;
    GST_M3U8_CLIENT_UNLOCK (demux->client);
    gst_m3u8_client_set_current (demux->client, previous_variant->data);
    /*  Try a lower bitrate (or stop if we just tried the lowest) */
    if (new_bandwidth ==
        GST_M3U8 (g_list_first (demux->client->main->lists)->data)->bandwidth)
      return FALSE;
    else
      return gst_hls_demux_change_playlist (demux, new_bandwidth - 1);
  }

  /* Force typefinding since we might have changed media type */
  demux->do_typefind = TRUE;

  return TRUE;
}

static gboolean
gst_hls_demux_schedule (GstHLSDemux * demux)
{
  gfloat update_factor;
  gint count;

  /* As defined in §6.3.4. Reloading the Playlist file:
   * "If the client reloads a Playlist file and finds that it has not
   * changed then it MUST wait for a period of time before retrying.  The
   * minimum delay is a multiple of the target duration.  This multiple is
   * 0.5 for the first attempt, 1.5 for the second, and 3.0 thereafter."
   */
  count = demux->client->update_failed_count;
  if (count < 3)
    update_factor = update_interval_factor[count];
  else
    update_factor = update_interval_factor[3];

  /* schedule the next update using the target duration field of the
   * playlist */
  g_time_val_add (&demux->next_update,
      gst_m3u8_client_get_target_duration (demux->client)
      / GST_SECOND * G_USEC_PER_SEC * update_factor);
  GST_DEBUG_OBJECT (demux, "Next update scheduled at %s",
      g_time_val_to_iso8601 (&demux->next_update));

  return TRUE;
}

static gboolean
gst_hls_demux_switch_playlist (GstHLSDemux * demux)
{
  GTimeVal now;
  GstClockTime diff;
  gsize size;
  gint bitrate;
  GstFragment *fragment = g_queue_peek_tail (demux->queue);

  GST_M3U8_CLIENT_LOCK (demux->client);
  if (!demux->client->main->lists) {
    GST_M3U8_CLIENT_UNLOCK (demux->client);
    return TRUE;
  }
  GST_M3U8_CLIENT_UNLOCK (demux->client);

  /* compare the time when the fragment was downloaded with the time when it was
   * scheduled */
  g_get_current_time (&now);
  diff = (GST_TIMEVAL_TO_TIME (now) - GST_TIMEVAL_TO_TIME (demux->next_update));
  size = gst_fragment_get_total_size (fragment);
  bitrate = (size * 8) / ((double) diff / GST_SECOND);

  GST_DEBUG ("Downloaded %" G_GSIZE_FORMAT " bytes in %" GST_TIME_FORMAT
      ". Bitrate is : %d", size, GST_TIME_ARGS (diff), bitrate);

  return gst_hls_demux_change_playlist (demux, bitrate * demux->bitrate_limit);
}

static gboolean
gst_hls_demux_get_next_fragment (GstHLSDemux * demux, gboolean caching)
{
  GstFragment *download;
  const gchar *next_fragment_uri;
  GstClockTime duration, timestamp;
  gint64 offset, length;
  GstBufferList *buffer_list;
  GstBuffer *buf;
  gboolean discont;

  if (!gst_m3u8_client_get_next_fragment (demux->client, &discont,
          &next_fragment_uri, &duration, &timestamp, &offset, &length)) {
    GST_INFO_OBJECT (demux, "This playlist doesn't contain more fragments");
    demux->end_of_playlist = TRUE;
    gst_task_start (demux->stream_task);
    return FALSE;
  }

  GST_INFO_OBJECT (demux, "Fetching next fragment %s", next_fragment_uri);

  download = gst_uri_downloader_fetch_uri_range (demux->downloader,
      next_fragment_uri, offset, length);

  if (download == NULL)
    goto error;

  buffer_list = gst_fragment_get_buffer_list (download);
  buf = gst_buffer_list_get (buffer_list, 0, 0);
  GST_BUFFER_DURATION (buf) = duration;
  GST_BUFFER_TIMESTAMP (buf) = timestamp;

  /* We actually need to do this every time we switch bitrate */
  if (G_UNLIKELY (demux->do_typefind)) {
    GstCaps *caps = gst_type_find_helper_for_buffer (NULL, buf, NULL);

    if (!demux->input_caps || !gst_caps_is_equal (caps, demux->input_caps)) {
      gst_caps_replace (&demux->input_caps, caps);
      /* gst_pad_set_caps (demux->srcpad, demux->input_caps); */
      GST_INFO_OBJECT (demux, "Input source caps: %" GST_PTR_FORMAT,
          demux->input_caps);
      demux->do_typefind = FALSE;
    }
    gst_caps_unref (caps);
  }
  gst_buffer_set_caps (buf, demux->input_caps);

  if (discont) {
    GST_DEBUG_OBJECT (demux, "Marking fragment as discontinuous");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  }

  g_queue_push_tail (demux->queue, download);
  gst_buffer_list_unref (buffer_list);
  if (!caching) {
    GST_TASK_SIGNAL (demux->updates_task);
    gst_task_start (demux->stream_task);
  }
  return TRUE;

error:
  {
    gst_hls_demux_stop (demux);
    return FALSE;
  }
}
