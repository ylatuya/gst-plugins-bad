/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gsthlssink.c:
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
 * SECTION:element-hlssink
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
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! keyunitsscheduler ! x264enc ! mpegtsmux ! hlssink output-directory=/var/www/hls playlist-url-prefix=http://myserver/live/hls
 * ]|
 * </refsect2>
 *
 * Last reviewed on 2011-07-04 (0.10.21)
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsthlssink.h"
#include <gst/video/video.h>
#include "gstfragmented-marshal.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    //GST_STATIC_CAPS ("video/mpegts, systemstream=true; video/webm"));
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_hls_sink_debug);
#define GST_CAT_DEFAULT gst_hls_sink_debug

enum
{
  /* signals */
  SIGNAL_EOS,
  SIGNAL_NEW_FRAGMENT,
  SIGNAL_NEW_PLAYLIST,

  /* actions */
  SIGNAL_PULL_FRAGMENT,
  SIGNAL_PULL_PLAYLIST,

  LAST_SIGNAL
};

#define DEFAULT_VERSION 6
#define DEFAULT_WINDOW_SIZE -1
#define DEFAULT_TITLE "GStreamer HTTP Live Streaming"
#define DEFAULT_OUTPUT_DIRECTORY "."
#define DEFAULT_PLAYLIST_NAME "stream"
#define DEFAULT_PLAYLIST_EXTENSION "m3u8"
#define DEFAULT_FRAGMENT_PREFIX "fragment"
#define DEFAULT_FRAGMENT_EXTENSION "ts"
#define DEFAULT_FRAGMENT_INDEX "%05d"

enum
{
  PROP_0,
  PROP_OUTPUT_DIRECTORY,
  PROP_PLAYLIST_NAME,
  PROP_PLAYLIST_VERSION,
  PROP_PLAYLIST_MAX_WINDOW,
  PROP_PLAYLIST_URL_PREFIX,
  PROP_FRAGMENT_PREFIX,
  PROP_WRITE_TO_DISK,
  PROP_DELETE_OLD_FILES,
  PROP_LAST
};

static void gst_hls_sink_dispose (GObject * object);
static void gst_hls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_hls_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_hls_sink_event (GstBaseSink * sink, GstEvent * event);
static gboolean gst_hls_sink_stop (GstBaseSink * sink);
static gboolean gst_hls_sink_unlock (GstBaseSink * sink);
static GstFlowReturn gst_hls_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static gboolean gst_hls_sink_set_caps (GstBaseSink * sink, GstCaps * caps);

static void gst_hls_sink_update_playlist_filepath (GstHlsSink * sink);
static gchar *gst_hls_sink_pull_playlist (GstHlsSink * hlssink);
static GstFragment *gst_hls_sink_pull_fragment (GstHlsSink * hlssink);

GST_BOILERPLATE (GstHlsSink, gst_hls_sink, GstBaseSink, GST_TYPE_BASE_SINK);

static guint gst_hls_sink_signals[LAST_SIGNAL] = { 0 };

static void
gst_hls_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG_CATEGORY_INIT (gst_hls_sink_debug, "hlssink", 0, "hlssink element");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
  gst_element_class_set_details_simple (gstelement_class,
      "HTTP Live Streaming Sink",
      "Sink/File",
      "Write a playlist and fragments for HTTP live streaming",
      "Andoni Morales Alastruey <ylatuya@gmail.com>");
}

static void
gst_hls_sink_class_init (GstHlsSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = gst_hls_sink_set_property;
  gobject_class->get_property = gst_hls_sink_get_property;

  /**
   * GstHlsSink:output-directory
   *
   * Directory where the playlist and fragments will be saved in disk
   *
   */
  g_object_class_install_property (gobject_class, PROP_OUTPUT_DIRECTORY,
      g_param_spec_string ("output-directory", "Output Directory",
          "Directory where the playlist and fragments will be saved", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstHlsSink:playlist-name
   *
   * Name of the playlist file, without the extension
   *
   */
  g_object_class_install_property (gobject_class, PROP_PLAYLIST_NAME,
      g_param_spec_string ("playlist-name", "Playlist Name",
          "Name of the playlist without the file extension", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstHlsSink:playlist-version
   *
   * Compatibility version of the playlist file
   *
   */
  g_object_class_install_property (gobject_class, PROP_PLAYLIST_VERSION,
      g_param_spec_uint ("playlist-version", "Playlist Version",
          "Compatibility version of the playlist file", 1, G_MAXUINT,
          DEFAULT_VERSION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstHlsSink:playlist-max-window
   *
   * Maximum number of fragments available in the playlist (-1 for unlimited)
   *
   */
  g_object_class_install_property (gobject_class, PROP_PLAYLIST_MAX_WINDOW,
      g_param_spec_int ("playlist-max-window", "Playlist Max Window",
          "Maximum number of fragments available in the playlist", -1, G_MAXINT,
          DEFAULT_WINDOW_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstHlsSink:playlist-url-prefix
   *
   * URL prefix used for the playlist fragments
   *
   */
  g_object_class_install_property (gobject_class, PROP_PLAYLIST_URL_PREFIX,
      g_param_spec_string ("playlist-url-prefix", "Playlist URL Prefix",
          "URL prefix used for the playlist fragments", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstHlsSink:fragment-prefix
   *
   * Prefix used to name fragments written to disk
   *
   */
  g_object_class_install_property (gobject_class, PROP_FRAGMENT_PREFIX,
      g_param_spec_string ("fragment-prefix", "Fragment Prefix",
          "Prefix used to name the fragments written to disk.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstHlsSink:write-to-disk
   *
   * Wheter the fragments and the playlist should be written to disk or not
   *
   */
  g_object_class_install_property (gobject_class, PROP_WRITE_TO_DISK,
      g_param_spec_boolean ("write-to-disk", "Write To Disk",
          "Wheter fragments and playlist should be written to disk or not",
          TRUE, G_PARAM_READWRITE));

  /**
   * GstHlsSink:delete-old-files
   *
   * Delete files for fragments that are no longer available in the playlist
   *
   */
  g_object_class_install_property (gobject_class, PROP_DELETE_OLD_FILES,
      g_param_spec_boolean ("delete-old-files", "Delete Old Files",
          "Delete files for fragments that are no longer available in the "
          "playlist", TRUE, G_PARAM_READWRITE));

  /**
   * GstHlsSink::eos:
   * @hlssink: the hlssink element that emited the signal
   *
   * Signal that the end-of-stream has been reached. This signal is emited from
   * the steaming thread.
   */
  gst_hls_sink_signals[SIGNAL_EOS] =
      g_signal_new ("eos", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstHlsSinkClass, eos),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstAppSink::new-playlist:
   * @glssink: the hlssink element that emited the signal
   *
   * Signal that the playlist has been modified and a new version is available
   *
   * This signal is emited from the steaming thread.
   *
   * The new playlist can be retrieved with the "pull-playlist" action
   * signal or gst_hls_sink_pull_buffer() either from this signal callback
   * or from any other thread.
   *
   */
  gst_hls_sink_signals[SIGNAL_NEW_PLAYLIST] =
      g_signal_new ("new-playlist", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstHlsSinkClass, new_playlist), NULL,
      NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstAppSink::new-fragment:
   * @hlssink: the hlssink element that emited the signal
   *
   * Signal that a new fragment is available.
   *
   * This signal is emited from the steaming thread.
   *
   * The new fragment can be retrieved with the "pull-fragment" action
   * signal.
   *
   */
  gst_hls_sink_signals[SIGNAL_NEW_FRAGMENT] =
      g_signal_new ("new-fragment", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstHlsSinkClass, new_fragment),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstAppSink::pull-fragment:
   * @hlssink: the hlssink element to emit this signal on
   *
   * This function returns the last rendered fragmentthe hlsink is in the PLAYING
   * state. Only the last rendered fragment is available, thus the application
   * must try to pull it as soon as it's available.
   *
   * If an EOS event was received before any fragment, this function returns
   * %NULL. Use gst_hls_sink_is_eos () to check for the EOS condition.
   *
   * Returns: a #GstBufferList or NULL when the hlssink is stopped or EOS.
   */
  gst_hls_sink_signals[SIGNAL_PULL_FRAGMENT] =
      g_signal_new ("pull-fragment", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (GstHlsSinkClass,
          pull_fragment), NULL, NULL, __gst_fragmented_marshal_OBJECT__VOID,
      GST_TYPE_FRAGMENT, 0, G_TYPE_NONE);

  /**
   * GstHlsSink::pull-playlist:
   * @hlssink: the hlssink element to emit this signal on
   *
   * This function returns the current playlist or %NULL if no one was
   * rendered yet. The playlist is not available until the first the element
   * has rendered at least 3 fragments, like defined in the draft.
   *
   * Returns: a #gchar or NULL when the hlssink is EOS.
   */
  gst_hls_sink_signals[SIGNAL_PULL_PLAYLIST] =
      g_signal_new ("pull-playlist", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (GstHlsSinkClass,
          pull_playlist), NULL, NULL, __gst_fragmented_marshal_STRING__VOID,
      G_TYPE_STRING, 0, G_TYPE_NONE);

  gobject_class->dispose = gst_hls_sink_dispose;

  gstbasesink_class->get_times = NULL;
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_hls_sink_stop);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_hls_sink_unlock);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_hls_sink_render);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_hls_sink_set_caps);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_hls_sink_event);

  klass->pull_playlist = gst_hls_sink_pull_playlist;
  klass->pull_fragment = gst_hls_sink_pull_fragment;
}

static void
gst_hls_sink_init (GstHlsSink * hlssink, GstHlsSinkClass * g_class)
{
  hlssink->playlist =
      gst_m3u8_playlist_new (DEFAULT_VERSION, DEFAULT_WINDOW_SIZE, TRUE);
  hlssink->fragment = NULL;
  hlssink->last_fragment = NULL;
  hlssink->fragment_cancellable = NULL;
  hlssink->new_fragment = FALSE;
  hlssink->write_to_disk = TRUE;
  hlssink->delete_old_files = TRUE;
  hlssink->output_directory = g_strdup (DEFAULT_OUTPUT_DIRECTORY);
  hlssink->playlist_name = g_strdup (DEFAULT_PLAYLIST_NAME);
  hlssink->playlist_url_prefix = g_strdup ("");
  hlssink->fragment_prefix = g_strdup (DEFAULT_FRAGMENT_PREFIX);
  hlssink->title = g_strdup (DEFAULT_TITLE);

  gst_hls_sink_update_playlist_filepath (hlssink);

  gst_base_sink_set_sync (GST_BASE_SINK (hlssink), FALSE);
}

static void
gst_hls_sink_dispose (GObject * object)
{
  gint i;
  GstHlsSink *hlssink = GST_HLS_SINK (object);

  if (hlssink->playlist != NULL) {
    gst_m3u8_playlist_free (hlssink->playlist);
    hlssink->playlist = NULL;
  }

  if (hlssink->last_fragment != NULL) {
    g_object_unref (hlssink->last_fragment);
    hlssink->last_fragment = NULL;
  }

  if (hlssink->fragment != NULL) {
    g_object_unref (hlssink->fragment);
    hlssink->fragment = NULL;
  }

  if (hlssink->streamheaders != NULL) {
    for (i = 0; i < hlssink->n_streamheaders; i++) {
      if (hlssink->streamheaders[i] != NULL)
        gst_buffer_unref (hlssink->streamheaders[i]);
    }
    g_free (hlssink->streamheaders);
    hlssink->streamheaders = NULL;
  }

  g_free (hlssink->output_directory);
  g_free (hlssink->playlist_name);
  g_free (hlssink->playlist_url_prefix);
  g_free (hlssink->playlist_filepath);
  g_free (hlssink->fragment_prefix);
  g_free (hlssink->title);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_hls_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstHlsSink *sink = GST_HLS_SINK (object);

  switch (prop_id) {
    case PROP_OUTPUT_DIRECTORY:
      g_free (sink->output_directory);
      sink->output_directory = g_value_dup_string (value);
      gst_hls_sink_update_playlist_filepath (sink);
      break;
    case PROP_FRAGMENT_PREFIX:
      g_free (sink->fragment_prefix);
      sink->fragment_prefix = g_value_dup_string (value);
      break;
    case PROP_PLAYLIST_NAME:
      g_free (sink->playlist_name);
      sink->playlist_name = g_value_dup_string (value);
      gst_hls_sink_update_playlist_filepath (sink);
      break;
    case PROP_PLAYLIST_URL_PREFIX:
      g_free (sink->playlist_url_prefix);
      sink->playlist_url_prefix = g_value_dup_string (value);
      break;
    case PROP_PLAYLIST_VERSION:
      sink->playlist->version = g_value_get_uint (value);
      break;
    case PROP_PLAYLIST_MAX_WINDOW:
      sink->playlist->window_size = g_value_get_int (value);
      break;
    case PROP_WRITE_TO_DISK:
      sink->write_to_disk = g_value_get_boolean (value);
      break;
    case PROP_DELETE_OLD_FILES:
      sink->delete_old_files = g_value_get_boolean (value);
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
  GstHlsSink *sink = GST_HLS_SINK (object);

  switch (prop_id) {
    case PROP_OUTPUT_DIRECTORY:
      g_value_set_string (value, sink->output_directory);
      break;
    case PROP_FRAGMENT_PREFIX:
      g_value_set_string (value, sink->fragment_prefix);
      break;
    case PROP_PLAYLIST_NAME:
      g_value_set_string (value, sink->playlist_name);
      break;
    case PROP_PLAYLIST_URL_PREFIX:
      g_value_set_string (value, sink->playlist_url_prefix);
      break;
    case PROP_PLAYLIST_VERSION:
      g_value_set_uint (value, sink->playlist->version);
      break;
    case PROP_PLAYLIST_MAX_WINDOW:
      g_value_set_int (value, sink->playlist->window_size);
      break;
    case PROP_WRITE_TO_DISK:
      g_value_set_boolean (value, sink->write_to_disk);
      break;
    case PROP_DELETE_OLD_FILES:
      g_value_set_boolean (value, sink->delete_old_files);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_hls_sink_unlock (GstBaseSink * sink)
{
  GstHlsSink *hlssink;

  hlssink = GST_HLS_SINK (sink);
  if (hlssink->fragment_cancellable == NULL)
    return TRUE;

  g_cancellable_cancel (hlssink->fragment_cancellable);
  g_object_unref (hlssink->fragment_cancellable);
  hlssink->fragment_cancellable = NULL;
  return TRUE;
}

static gboolean
gst_hls_sink_stop (GstBaseSink * sink)
{
  GstHlsSink *hlssink;

  hlssink = GST_HLS_SINK (sink);

  if (hlssink->playlist != NULL) {
    gst_m3u8_playlist_clear (hlssink->playlist);
  }

  if (hlssink->fragment != NULL) {
    g_object_unref (hlssink->fragment);
    hlssink->fragment = NULL;
  }

  if (hlssink->last_fragment != NULL) {
    g_object_unref (hlssink->last_fragment);
    hlssink->last_fragment = NULL;
  }

  hlssink->new_fragment = FALSE;

  return TRUE;
}

static gchar *
gst_hls_sink_pull_playlist (GstHlsSink * hlssink)
{
  gchar *playlist;

  g_return_val_if_fail (hlssink != NULL, NULL);

  GST_OBJECT_LOCK (hlssink);
  playlist = gst_m3u8_playlist_render (hlssink->playlist);
  GST_OBJECT_UNLOCK (hlssink);

  return playlist;
}

static GstFragment *
gst_hls_sink_pull_fragment (GstHlsSink * hlssink)
{
  GstFragment *fragment;

  g_return_val_if_fail (hlssink != NULL, NULL);

  GST_OBJECT_LOCK (hlssink);
  g_object_ref (hlssink->last_fragment);
  fragment = hlssink->last_fragment;
  GST_OBJECT_UNLOCK (hlssink);

  return fragment;
}

static void
gst_hls_sink_update_playlist_filepath (GstHlsSink * hlssink)
{
  gchar *basename;

  if (hlssink->playlist_filepath != NULL)
    g_free (hlssink->playlist_filepath);

  basename = g_strdup_printf ("%s.%s", hlssink->playlist_name,
      DEFAULT_PLAYLIST_EXTENSION);
  hlssink->playlist_filepath = g_build_filename (hlssink->output_directory,
      basename, NULL);
  g_free (basename);
}

static GstFlowReturn
gst_hls_sink_write_element (GstHlsSink * hlssink, gchar * filename,
    gboolean is_fragment)
{
  GFile *file;
  GFileOutputStream *stream;
  GError *error = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBufferList *list = NULL;
  GstBufferListIterator *it = NULL;

  /* Create the new file */
  file = g_file_new_for_path (filename);
  hlssink->fragment_cancellable = g_cancellable_new ();
  stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
      hlssink->fragment_cancellable, &error);
  if (error) {
    if (error->code == G_IO_ERROR_CANCELLED)
      goto done;
    goto open_error;
  }

  if (is_fragment) {
    GstBuffer *buffer;

    /* Iterate over all the fragment's buffers and write them */
    list = gst_fragment_get_buffer_list (hlssink->fragment);
    it = gst_buffer_list_iterate (list);
    while (gst_buffer_list_iterator_next_group (it)) {
      while ((buffer = gst_buffer_list_iterator_next (it)) != NULL) {
        g_output_stream_write ((GOutputStream *) stream,
            GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer),
            hlssink->fragment_cancellable, &error);
        if (error)
          goto write_error;
      }
    }
  } else {
    gchar *playlist_str;

    playlist_str = gst_m3u8_playlist_render (hlssink->playlist);
    g_output_stream_write ((GOutputStream *) stream, playlist_str,
        g_utf8_strlen (playlist_str, -1), hlssink->fragment_cancellable,
        &error);
    g_free (playlist_str);
  }

  /* Flush and close file */
  g_output_stream_flush ((GOutputStream *) stream,
      hlssink->fragment_cancellable, &error);
  if (error)
    goto close_error;

  g_output_stream_close ((GOutputStream *) stream,
      hlssink->fragment_cancellable, &error);
  if (error)
    goto close_error;

  ret = GST_FLOW_OK;
  goto done;

  /* ERRORS */
open_error:
  {
    GST_ELEMENT_ERROR (hlssink, RESOURCE, OPEN_WRITE,
        ("Error openning file \"%s\".", filename), ("%s", g_strerror (errno)));
    ret = GST_FLOW_ERROR;
    goto done;
  }

write_error:
  {
    if (error->code == G_IO_ERROR_CANCELLED) {
      ret = GST_FLOW_WRONG_STATE;
      goto done;
    }

    switch (error->code) {
      case G_IO_ERROR_NO_SPACE:{
        GST_ELEMENT_ERROR (hlssink, RESOURCE, NO_SPACE_LEFT, (NULL), (NULL));
        ret = GST_FLOW_ERROR;
        break;
      }
      default:{
        GST_ELEMENT_ERROR (hlssink, RESOURCE, WRITE,
            ("Error while writing to file \"%s\".", filename),
            ("%s", g_strerror (errno)));
        ret = GST_FLOW_ERROR;
        ret = GST_FLOW_ERROR;
      }
    }
    goto done;
  }

close_error:
  {
    if (error->code == G_IO_ERROR_CANCELLED) {
      ret = GST_FLOW_WRONG_STATE;
      goto done;
    }

    GST_ELEMENT_ERROR (hlssink, RESOURCE, CLOSE,
        ("Error closing file \"%s\".", filename), ("%s", g_strerror (errno)));
    ret = GST_FLOW_ERROR;
    goto done;
  }

done:
  {
    if (error)
      g_error_free (error);
    if (stream)
      g_object_unref (stream);
    if (it)
      gst_buffer_list_iterator_free (it);
    if (list)
      gst_buffer_list_unref (list);
    g_object_unref (file);
    g_object_unref (hlssink->fragment_cancellable);
    hlssink->fragment_cancellable = NULL;

    return ret;
  }
}

static void
gst_hls_sink_update_fragment (GstHlsSink * hlssink)
{
  /* Replace the last fragment with the new one */
  GST_OBJECT_LOCK (hlssink);
  if (hlssink->last_fragment != NULL) {
    g_object_unref (hlssink->last_fragment);
  }
  hlssink->last_fragment = hlssink->fragment;
  hlssink->fragment = NULL;
  GST_OBJECT_UNLOCK (hlssink);
}

static void
gst_hls_sink_create_empty_fragment (GstHlsSink * hlssink,
    GstClockTime start_ts, guint index)
{
  /* Create a new empty fragment */
  hlssink->fragment = gst_fragment_new ();
  hlssink->fragment->index = index;
  hlssink->fragment->start_ts = start_ts;
  g_free (hlssink->fragment->name);
  hlssink->fragment->name = g_strdup_printf ("%s-%05d.%s",
      hlssink->fragment_prefix, hlssink->fragment->index,
      DEFAULT_FRAGMENT_EXTENSION);
  if (hlssink->streamheaders != NULL)
    gst_fragment_set_headers (hlssink->fragment, hlssink->streamheaders,
        hlssink->n_streamheaders);
}

static GstFlowReturn
gst_hls_sink_close_fragment (GstHlsSink * hlssink, GstClockTime stop_ts)
{
  GstClockTime duration;
  gchar *fragment_filename;
  gchar *fragment_url;
  GList *old_files;
  GstFlowReturn ret = GST_FLOW_OK;

  hlssink->fragment->completed = TRUE;
  hlssink->fragment->stop_ts = stop_ts;
  duration = hlssink->fragment->stop_ts - hlssink->fragment->start_ts;

  /* Build fragment file path and url */
  fragment_filename =
      g_build_filename (hlssink->output_directory, hlssink->fragment->name,
      NULL);
  fragment_url =
      g_build_path (G_DIR_SEPARATOR_S, hlssink->playlist_url_prefix,
      hlssink->fragment->name, NULL);

  GST_INFO_OBJECT (hlssink, "Created new fragment name:%s duration: %"
      GST_TIME_FORMAT, fragment_filename, GST_TIME_ARGS (duration));

  /* Write fragment to disk */
  if (hlssink->write_to_disk) {
    GST_DEBUG_OBJECT (hlssink, "Writting fragment to disk");
    ret = gst_hls_sink_write_element (hlssink, fragment_filename, TRUE);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  /* Add the new entry to the playlist after the fragment was succeesfully
   * written to disk */
  GST_OBJECT_LOCK (hlssink);
  old_files = gst_m3u8_playlist_add_entry (hlssink->playlist, fragment_url,
      g_file_new_for_path (fragment_filename), hlssink->title, duration,
      hlssink->fragment->index, hlssink->fragment->discontinuous);
  GST_OBJECT_UNLOCK (hlssink);
  GST_INFO_OBJECT (hlssink, "Playlist updated with %d entries",
      gst_m3u8_playlist_n_entries (hlssink->playlist));

  /* Delete old files */
  if (hlssink->delete_old_files) {
    g_list_foreach (old_files, (GFunc) g_file_delete, NULL);
  }
  g_list_foreach (old_files, (GFunc) g_object_unref, NULL);
  g_list_free (old_files);

  /* Write playlist to disk */
  if (hlssink->write_to_disk) {
    GST_DEBUG_OBJECT (hlssink, "Writting playlist to disk");
    ret =
        gst_hls_sink_write_element (hlssink, hlssink->playlist_filepath, FALSE);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  gst_hls_sink_update_fragment (hlssink);

  g_signal_emit (hlssink, gst_hls_sink_signals[SIGNAL_NEW_PLAYLIST], 0);
  g_signal_emit (hlssink, gst_hls_sink_signals[SIGNAL_NEW_FRAGMENT], 0);

done:
  {
    g_free (fragment_filename);
    g_free (fragment_url);
    return ret;
  }
}

static GstFlowReturn
gst_hls_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstHlsSink *hlssink;
  GstFlowReturn ret;
  guint new_fragment_index;
  GstClockTime new_fragment_ts;

  hlssink = GST_HLS_SINK (sink);

  /* Check if a new fragment needs to be created and save the old one updating
   * the playlist too */
  if (hlssink->new_fragment) {
    GST_OBJECT_LOCK (hlssink);
    hlssink->new_fragment = FALSE;
    new_fragment_ts = hlssink->new_fragment_ts;
    new_fragment_index = hlssink->new_fragment_index;
    GST_OBJECT_UNLOCK (hlssink);

    if (hlssink->fragment != NULL) {
      ret = gst_hls_sink_close_fragment (hlssink, new_fragment_ts);
      if (ret != GST_FLOW_OK)
        return ret;
    }
    gst_hls_sink_create_empty_fragment (hlssink, new_fragment_ts,
        new_fragment_index);
  }

  /* Drop this buffer if we haven't started a fragment yet */
  if (hlssink->fragment == NULL) {
    GST_DEBUG_OBJECT (hlssink,
        "Dropping buffer because no fragment was started yet");
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (hlssink, "Added new buffer to fragment with timestamp: %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  if (G_UNLIKELY (hlssink->fragment->start_ts == GST_CLOCK_TIME_NONE))
    hlssink->fragment->start_ts = GST_BUFFER_TIMESTAMP (buffer);

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS)) {
    GST_DEBUG_OBJECT (hlssink, "Dropping IN_CAPS buffer");
  } else {
    gst_fragment_add_buffer (hlssink->fragment, buffer);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_hls_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstHlsSink *hlssink;
  GstStructure *structure;

  hlssink = GST_HLS_SINK (sink);

  structure = gst_caps_get_structure (caps, 0);
  if (structure) {
    const GValue *value;

    value = gst_structure_get_value (structure, "streamheader");

    if (GST_VALUE_HOLDS_ARRAY (value)) {
      int i;

      if (hlssink->streamheaders) {
        for (i = 0; i < hlssink->n_streamheaders; i++) {
          gst_buffer_unref (hlssink->streamheaders[i]);
        }
        g_free (hlssink->streamheaders);
      }

      hlssink->n_streamheaders = gst_value_array_get_size (value);
      hlssink->streamheaders =
          g_malloc (sizeof (GstBuffer *) * hlssink->n_streamheaders);

      for (i = 0; i < hlssink->n_streamheaders; i++) {
        hlssink->streamheaders[i] =
            gst_buffer_ref (gst_value_get_buffer (gst_value_array_get_value
                (value, i)));
      }
    }
  }

  return TRUE;
}

static gboolean
gst_hls_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstHlsSink *hlssink;
  GstClockTime timestamp, stream_time, running_time;
  gboolean all_headers;
  guint count;

  hlssink = GST_HLS_SINK (sink);

  if (gst_video_event_parse_downstream_force_key_unit (event, &timestamp,
          &stream_time, &running_time, &all_headers, &count)) {
    GST_DEBUG_OBJECT (hlssink, "Received GstForceKeyUnit event");
    GST_OBJECT_LOCK (hlssink);
    hlssink->new_fragment_index = count;
    hlssink->new_fragment_ts = timestamp;
    hlssink->new_fragment = TRUE;
    GST_OBJECT_UNLOCK (hlssink);
  }

  else if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GST_OBJECT_LOCK (hlssink);
    hlssink->playlist->end_list = TRUE;
    GST_OBJECT_UNLOCK (hlssink);

    if (hlssink->fragment != NULL) {
      gst_hls_sink_close_fragment (hlssink, GST_CLOCK_TIME_NONE);
      g_signal_emit (hlssink, gst_hls_sink_signals[SIGNAL_NEW_PLAYLIST], 0);
      g_signal_emit (hlssink, gst_hls_sink_signals[SIGNAL_NEW_FRAGMENT], 0);
    }
    g_signal_emit (hlssink, gst_hls_sink_signals[SIGNAL_EOS], 0);
  }

  return TRUE;
}
