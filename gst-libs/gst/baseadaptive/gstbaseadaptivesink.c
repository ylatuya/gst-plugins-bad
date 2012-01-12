/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstsink.c:
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
 * SECTION:element-sink
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include "gstbaseadaptivesink.h"
#include "gstadaptive-marshal.h"

GST_DEBUG_CATEGORY_STATIC (gst_base_adaptive_sink_debug);
#define GST_CAT_DEFAULT gst_base_adaptive_sink_debug

enum
{
  /* signals */
  SIGNAL_EOS,
  SIGNAL_NEW_FRAGMENT,
  SIGNAL_NEW_PLAYLIST,

  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_WRITE_TO_DISK,
  PROP_OUTPUT_DIRECTORY,
  PROP_DELETE_OLD_FILES,
  PROP_MD_NAME,
  PROP_FRAGMENT_PREFIX,
  PROP_BASE_URL,
  PROP_CHUNKED,
  PROP_IS_LIVE,
  PROP_LAST
};

typedef struct
{
  GstPad *pad;
  gboolean parsed;
  GList *decoded_caps;
  gboolean new_fragment;
  guint new_fragment_index;
  GstClockTime new_fragment_ts;
  GstFragment *fragment;
  GstFragment *last_fragment;
  GCancellable *cancellable;

  GMutex *discover_lock;
  GCond *discover_cond;

  int n_streamheaders;
  GstBuffer **streamheaders;
} GstBaseAdaptivePadData;


/* GObject */
static void gst_base_adaptive_sink_dispose (GObject * object);
static void gst_base_adaptive_sink_finalize (GObject * object);
static void gst_base_adaptive_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_base_adaptive_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

/* GstElement */
static GstPad *gst_base_adaptive_sink_request_new_pad (GstElement * element,
    GstPadTemplate * template, const gchar * name);
static void gst_base_adaptive_sink_release_pad (GstElement * element,
    GstPad * pad);
static gboolean gst_base_adaptive_sink_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_base_adaptive_sink_chain (GstPad * pad,
    GstBuffer * buffer);
static gboolean gst_base_adaptive_sink_set_caps (GstPad * sink, GstCaps * caps);
static GstStateChangeReturn gst_base_adaptive_sink_change_state (GstElement *
    element, GstStateChange transition);

/*GstBaseAdaptiveSink */
typedef GError *(*WriteFunc) (GstBaseAdaptiveSink * sink,
    GFileOutputStream * output_stream, gpointer data,
    GCancellable * cancellable);
static GError *gst_base_adaptive_sink_write_fragment (GstBaseAdaptiveSink *
    sink, GstBaseAdaptivePadData * pad_data, GFileOutputStream * stream,
    GCancellable * cancellable);
static void gst_base_adaptive_sink_pad_data_free (GstBaseAdaptivePadData *
    pad_data);
static GstBaseAdaptivePadData *gst_base_adaptive_sink_pad_data_new (GstPad *
    pad);
static GstFlowReturn gst_base_adaptive_sink_close_fragment (GstBaseAdaptiveSink
    * sink, GstBaseAdaptivePadData * pad_data, GstClockTime stop_ts);
static void gst_base_adaptive_sink_stop (GstBaseAdaptiveSink * sink);
static void gst_base_adaptive_sink_unlock (GstBaseAdaptiveSink * sink);
static void gst_base_adaptive_sink_create_empty_fragment (GstBaseAdaptiveSink *
    sink, GstBaseAdaptivePadData * pad_data, GstClockTime start_ts,
    guint64 offset, guint index);
static GstFlowReturn gst_base_adaptive_sink_write_element (GstBaseAdaptiveSink *
    sink, WriteFunc write_func, gchar * filename, gpointer data);
static void gst_base_adaptive_sink_parse_stream (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstBuffer * buf);
static gboolean gst_base_adaptive_process_new_stream (GstBaseAdaptiveSink *
    sink, GstPad * pad, GstBaseAdaptivePadData * pad_data);

GST_BOILERPLATE (GstBaseAdaptiveSink, gst_base_adaptive_sink, GstElement,
    GST_TYPE_ELEMENT);

static guint gst_base_adaptive_sink_signals[LAST_SIGNAL] = { 0 };

static void
gst_base_adaptive_sink_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_base_adaptive_sink_debug, "baseadaptivesink", 0,
      "sink element");
}

static void
gst_base_adaptive_sink_class_init (GstBaseAdaptiveSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_base_adaptive_sink_set_property;
  gobject_class->get_property = gst_base_adaptive_sink_get_property;

  /**
   * GstBaseAdaptiveSink:output-directory
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
   * GstBaseAdaptiveSink:media-description-filename
   *
   * Name of the Media Description file, without the extension
   *
   */
  g_object_class_install_property (gobject_class, PROP_MD_NAME,
      g_param_spec_string ("media-description-filename",
          "Media Description filename",
          "Name of the media description file without the file extension", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstBaseAdaptiveSink:base-url
   *
   * Base URL use for the media description links
   *
   */
  g_object_class_install_property (gobject_class, PROP_BASE_URL,
      g_param_spec_string ("base-url", "Base URL",
          "Base URL used for the media description links", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstBaseAdaptiveSink:fragment-prefix
   *
   * Prefix used to name fragments written to disk
   *
   */
  g_object_class_install_property (gobject_class, PROP_FRAGMENT_PREFIX,
      g_param_spec_string ("fragment-prefix", "Fragment Prefix",
          "Prefix used to name the fragments written to disk.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseAdaptiveSink:write-to-disk
   *
   * Wheter the fragments and the playlist should be written to disk or not
   *
   */
  g_object_class_install_property (gobject_class, PROP_WRITE_TO_DISK,
      g_param_spec_boolean ("write-to-disk", "Write To Disk",
          "Wheter fragments and playlist should be written to disk or not",
          TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:delete-old-files
   *
   * Delete files for fragments that are no longer available in the playlist
   *
   */
  g_object_class_install_property (gobject_class, PROP_DELETE_OLD_FILES,
      g_param_spec_boolean ("delete-old-files", "Delete Old Files",
          "Delete files for fragments that are no longer available in the "
          "playlist", TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:chunked
   *
   * Wheter create a new file for each fragment or append them to same file
   *
   */
  g_object_class_install_property (gobject_class, PROP_DELETE_OLD_FILES,
      g_param_spec_boolean ("chunked", "Chunked",
          "Create a new file for each fragment", TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:is-live
   *
   * Wheter the media is live or on-demand
   *
   */
  g_object_class_install_property (gobject_class, PROP_DELETE_OLD_FILES,
      g_param_spec_boolean ("is-live", "Is live",
          "Whether the media is live or on-demand", TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink::eos:
   * @sink: the sink element that emited the signal
   *
   * Signal that the end-of-stream has been reached. This signal is emited from
   * the steaming thread.
   */
  gst_base_adaptive_sink_signals[SIGNAL_EOS] =
      g_signal_new ("eos", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstBaseAdaptiveSinkClass, eos),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstBaseAdaptiveSink::new-playlist:
   * @glssink: the sink element that emited the signal
   *
   * Signal that the playlist has been modified and a new version is available
   *
   * This signal is emited from the steaming thread.
   *
   * The new playlist can be retrieved with the "pull-playlist" action
   * signal or gst_base_adaptive_sink_pull_buffer() either from this signal callback
   * or from any other thread.
   *
   */
  gst_base_adaptive_sink_signals[SIGNAL_NEW_PLAYLIST] =
      g_signal_new ("new-playlist", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstBaseAdaptiveSinkClass,
          new_playlist), NULL, NULL, __gst_adaptive_marshal_VOID__STRING_STRING,
      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * GstAppSink::new-fragment:
   * @sink: the sink element that emited the signal
   *
   * Signal that a new fragment is available.
   *
   * This signal is emited from the steaming thread.
   *
   * The new fragment can be retrieved with the "pull-fragment" action
   * signal.
   *
   */
  gst_base_adaptive_sink_signals[SIGNAL_NEW_FRAGMENT] =
      g_signal_new ("new-fragment", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstBaseAdaptiveSinkClass,
          new_fragment), NULL, NULL, __gst_adaptive_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, GST_TYPE_FRAGMENT);


  gobject_class->dispose = gst_base_adaptive_sink_dispose;
  gobject_class->finalize = gst_base_adaptive_sink_finalize;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_base_adaptive_sink_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_base_adaptive_sink_release_pad);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_base_adaptive_sink_change_state);
}

static void
gst_base_adaptive_sink_init (GstBaseAdaptiveSink * sink,
    GstBaseAdaptiveSinkClass * g_class)
{
  sink->pad_datas = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gst_base_adaptive_sink_pad_data_free);
  sink->write_to_disk = TRUE;
  sink->delete_old_files = TRUE;
  sink->chunked = TRUE;
  sink->output_directory = g_strdup (".");
  sink->base_url = g_strdup ("");
  if (g_class->create_media_manager != NULL)
    sink->media_manager = g_class->create_media_manager (sink);
  else
    sink->media_manager = gst_base_media_manager_new ();

  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_IS_SINK);

}

static void
gst_base_adaptive_sink_finalize (GObject * object)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  if (sink->output_directory != NULL) {
    g_free (sink->output_directory);
    sink->output_directory = NULL;
  }

  if (sink->base_url != NULL) {
    g_free (sink->base_url);
    sink->base_url = NULL;
  }

  if (sink->pad_datas != NULL) {
    g_hash_table_destroy (sink->pad_datas);
    sink->pad_datas = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_adaptive_sink_dispose (GObject * object)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  if (sink->media_manager != NULL) {
    g_object_unref (sink->media_manager);
    sink->media_manager = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_base_adaptive_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  switch (prop_id) {
    case PROP_OUTPUT_DIRECTORY:
      g_free (sink->output_directory);
      sink->output_directory = g_value_dup_string (value);
      break;
    case PROP_BASE_URL:
      g_free (sink->base_url);
      sink->base_url = g_value_dup_string (value);
      gst_base_media_manager_set_base_url (sink->media_manager,
          g_value_dup_string (value));
      break;
    case PROP_MD_NAME:
      g_free (sink->md_name);
      sink->md_name = g_value_dup_string (value);
      gst_base_media_manager_set_title (sink->media_manager,
          g_value_dup_string (value));
      break;
    case PROP_FRAGMENT_PREFIX:
      g_free (sink->media_manager->fragment_prefix);
      sink->media_manager->fragment_prefix = g_value_dup_string (value);
      gst_base_media_manager_set_fragment_prefix (sink->media_manager,
          g_value_dup_string (value));
      break;
    case PROP_WRITE_TO_DISK:
      sink->write_to_disk = g_value_get_boolean (value);
      break;
    case PROP_DELETE_OLD_FILES:
      sink->delete_old_files = g_value_get_boolean (value);
      break;
    case PROP_CHUNKED:
      sink->chunked = g_value_get_boolean (value);
      sink->media_manager->chunked = g_value_get_boolean (value);
      break;
    case PROP_IS_LIVE:
      sink->is_live = g_value_get_boolean (value);
      sink->media_manager->is_live = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_adaptive_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  switch (prop_id) {
    case PROP_OUTPUT_DIRECTORY:
      g_value_set_string (value, sink->output_directory);
      break;
    case PROP_BASE_URL:
      g_value_set_string (value, sink->base_url);
      break;
    case PROP_MD_NAME:
      g_value_set_string (value, sink->md_name);
      break;
    case PROP_FRAGMENT_PREFIX:
      g_value_set_string (value, sink->fragment_prefix);
      break;
    case PROP_WRITE_TO_DISK:
      g_value_set_boolean (value, sink->write_to_disk);
      break;
    case PROP_DELETE_OLD_FILES:
      g_value_set_boolean (value, sink->delete_old_files);
      break;
    case PROP_CHUNKED:
      g_value_set_boolean (value, sink->chunked);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, sink->is_live);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_base_adaptive_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_base_adaptive_sink_unlock (sink);
      gst_base_adaptive_sink_stop (sink);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}

static void
gst_base_adaptive_sink_unlock (GstBaseAdaptiveSink * sink)
{
  GHashTableIter iter;
  GstPad *pad;
  GstBaseAdaptivePadData *pad_data;

  g_hash_table_iter_init (&iter, sink->pad_datas);
  while (g_hash_table_iter_next (&iter, (void *) &pad, (void *) &pad_data)) {
    if (pad_data->cancellable == NULL)
      return;

    g_cancellable_cancel (pad_data->cancellable);
    g_object_unref (pad_data->cancellable);
    pad_data->cancellable = NULL;
  }
}

static void
gst_base_adaptive_sink_stop (GstBaseAdaptiveSink * sink)
{
  if (sink->media_manager != NULL) {
    g_object_unref (sink->media_manager);
    sink->media_manager = NULL;
  }

  /* Clear all pad data */
  g_hash_table_remove_all (sink->pad_datas);
}

static GstPad *
gst_base_adaptive_sink_request_new_pad (GstElement * element,
    GstPadTemplate * template, const gchar * name)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;
  GstPad *pad;

  sink = GST_BASE_ADAPTIVE_SINK (element);

  pad = gst_pad_new_from_template (template, name);
  gst_pad_set_event_function (pad, gst_base_adaptive_sink_event);
  gst_pad_set_chain_function (pad, gst_base_adaptive_sink_chain);
  gst_pad_set_setcaps_function (pad, gst_base_adaptive_sink_set_caps);
  gst_pad_set_element_private (pad, sink);

  pad_data = gst_base_adaptive_sink_pad_data_new (pad);
  g_hash_table_insert (sink->pad_datas, pad, pad_data);

  gst_pad_set_active (pad, TRUE);
  gst_element_add_pad (element, pad);

  GST_INFO_OBJECT (sink, "Added new stream for pad  %s:%s",
      GST_DEBUG_PAD_NAME (pad));
  return pad;
}

static void
gst_base_adaptive_sink_release_pad (GstElement * element, GstPad * pad)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;

  sink = GST_BASE_ADAPTIVE_SINK (element);
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  if (pad_data != NULL)
    gst_base_adaptive_sink_pad_data_free (pad_data);
}

static GstFlowReturn
gst_base_adaptive_sink_chain (GstPad * pad, GstBuffer * buffer)
{
  GstBaseAdaptiveSink *sink;
  GstFlowReturn ret;
  GstBaseAdaptivePadData *pad_data;

  sink = GST_BASE_ADAPTIVE_SINK (gst_pad_get_element_private (pad));
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  /* Drop all IN_CAPS buffers */
  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS)) {
    GST_DEBUG_OBJECT (pad_data, "Dropping IN_CAPS buffer");
    return GST_FLOW_OK;
  }

  /* Check if a new fragment needs to be created and save the old one updating
   * the playlist too */
  if (pad_data->new_fragment) {
    guint64 offset;
    guint index;
    GstClockTime ts;

    /* Parse headers for this stream and add it to the media manager */
    if (G_UNLIKELY (!pad_data->parsed)) {
      if (!gst_base_adaptive_process_new_stream (sink, pad, pad_data)) {
        GST_ELEMENT_ERROR (sink, STREAM, FORMAT,
            ("Error adding stream for pad %s:%s", GST_DEBUG_PAD_NAME (pad)),
            NULL);
        return GST_FLOW_ERROR;
      }
    }

    pad_data->new_fragment = FALSE;
    ts = pad_data->new_fragment_ts;
    index = pad_data->new_fragment_index;
    offset = 0;

    if (pad_data->fragment != NULL) {
      offset = pad_data->fragment->offset + pad_data->fragment->size;
      ret = gst_base_adaptive_sink_close_fragment (sink, pad_data, ts);
      if (ret != GST_FLOW_OK)
        return ret;
    }
    gst_base_adaptive_sink_create_empty_fragment (sink, pad_data, ts, offset,
        index);
  }

  /* Drop this buffer if we haven't started a fragment yet */
  if (pad_data->fragment == NULL) {
    GST_DEBUG_OBJECT (sink,
        "Dropping buffer because no fragment was started yet");
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (sink, "Added new buffer to fragment with timestamp: %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  if (G_UNLIKELY (pad_data->fragment->start_ts == GST_CLOCK_TIME_NONE))
    pad_data->fragment->start_ts = GST_BUFFER_TIMESTAMP (buffer);

  gst_fragment_add_buffer (pad_data->fragment, buffer);

  return GST_FLOW_OK;
}

static gboolean
gst_base_adaptive_sink_set_stream_headers (GstBaseAdaptiveSink * sink,
    GstPad * pad, GstBaseAdaptivePadData * pad_data)
{
  GstFragment *fragment;
  gchar *init_segment_name;
  GstFlowReturn fret;
  gboolean ret = TRUE;

  if (pad_data->n_streamheaders == 0)
    return TRUE;

  /* Create a new fragment with the stream headers */
  fragment = gst_fragment_new ();
  gst_fragment_set_headers (fragment, pad_data->streamheaders,
      pad_data->n_streamheaders);
  fragment->completed = TRUE;

  /* Add initialization segment */
  gst_base_media_manager_add_headers (sink->media_manager, pad, fragment);
  init_segment_name = fragment->name;

  if (sink->write_to_disk && init_segment_name != NULL) {
    gchar *filename;

    GST_DEBUG_OBJECT (sink, "Writting headers fragment to disk");
    filename = g_build_filename (sink->output_directory, init_segment_name,
        NULL);
    pad_data->fragment = fragment;

    fret = gst_base_adaptive_sink_write_element (sink,
        (WriteFunc) gst_base_adaptive_sink_write_fragment, filename, pad_data);
    g_free (filename);

    if (fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (sink, "Could not write headers fragment to disk");
      ret = FALSE;
    }
    pad_data->fragment = NULL;
  }

  g_object_unref (fragment);
  return ret;
}

static void
gst_base_adaptive_sink_get_headers_from_caps (GstBaseAdaptiveSink * sink,
    GstCaps * caps, GstBaseAdaptivePadData * pad_data)
{
  GstStructure *structure;
  const GValue *value;
  gint i;

  structure = gst_caps_get_structure (caps, 0);
  if (!structure)
    return;

  value = gst_structure_get_value (structure, "streamheader");
  if (!GST_VALUE_HOLDS_ARRAY (value)) {
    return;
  }

  /* Clear old streamheaders */
  if (pad_data->streamheaders) {
    for (i = 0; i < pad_data->n_streamheaders; i++) {
      gst_buffer_unref (pad_data->streamheaders[i]);
    }
    g_free (pad_data->streamheaders);
  }

  /* Save new streamheaders */
  pad_data->n_streamheaders = gst_value_array_get_size (value);
  pad_data->streamheaders =
      g_malloc (sizeof (GstBuffer *) * pad_data->n_streamheaders);

  for (i = 0; i < pad_data->n_streamheaders; i++) {
    pad_data->streamheaders[i] =
        gst_buffer_ref (gst_value_get_buffer (gst_value_array_get_value
            (value, i)));
  }
}

static gboolean
gst_base_adaptive_sink_set_caps (GstPad * pad, GstCaps * caps)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;

  sink = GST_BASE_ADAPTIVE_SINK (gst_pad_get_element_private (pad));
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  gst_base_adaptive_sink_get_headers_from_caps (sink, caps, pad_data);

  return TRUE;
}

static gboolean
gst_base_adaptive_process_new_stream (GstBaseAdaptiveSink * sink, GstPad * pad,
    GstBaseAdaptivePadData * pad_data)
{
  GstBuffer *buf = NULL;
  gint i;

  /* Parse the headers to get the muxed streams, needed for fragmented MP4 */
  if (pad_data->n_streamheaders != 0) {
    for (i = 0; i < pad_data->n_streamheaders; i++) {
      if (buf == NULL)
        buf = pad_data->streamheaders[i];
      else
        buf = gst_buffer_merge (buf, pad_data->streamheaders[i]);
    }
    gst_base_adaptive_sink_parse_stream (sink, pad_data, buf);
    gst_buffer_unref (buf);
  }
  pad_data->parsed = TRUE;

  if (!gst_base_media_manager_add_stream (sink->media_manager, pad,
          pad_data->decoded_caps)) {
    GST_ERROR_OBJECT (sink, "Could not add stream for pad %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    return FALSE;
  }

  /* Save the headers to disk and update it in the media manager */
  gst_base_adaptive_sink_set_stream_headers (sink, pad, pad_data);

  return TRUE;
}

static gboolean
gst_base_adaptive_sink_event (GstPad * pad, GstEvent * event)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;
  GstClockTime timestamp, stream_time, running_time;
  gboolean all_headers;
  guint count;

  sink = GST_BASE_ADAPTIVE_SINK (gst_pad_get_element_private (pad));
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  if (gst_video_event_parse_downstream_force_key_unit (event, &timestamp,
          &stream_time, &running_time, &all_headers, &count)) {
    GST_DEBUG_OBJECT (sink, "Received GstForceKeyUnit event");
    GST_OBJECT_LOCK (sink);
    pad_data->new_fragment_index = count;
    pad_data->new_fragment_ts = timestamp;
    pad_data->new_fragment = TRUE;
    GST_OBJECT_UNLOCK (sink);
  }

  else if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GstMessage *message;

    GST_OBJECT_LOCK (sink);
    /* FIXME: */
    /*gst_base_playlist_finish(sink->playlist); */
    GST_OBJECT_UNLOCK (sink);

    if (pad_data->fragment != NULL) {
      gst_base_adaptive_sink_close_fragment (sink, pad_data,
          GST_CLOCK_TIME_NONE);
    }
    g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_EOS], 0);
    /* ok, now we can post the message */
    GST_DEBUG_OBJECT (sink, "Posting EOS");

    message = gst_message_new_eos (GST_OBJECT_CAST (sink));
    gst_element_post_message (GST_ELEMENT_CAST (sink), message);
  }

  return TRUE;
}

static GError *
gst_base_adaptive_sink_write_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GFileOutputStream * stream,
    GCancellable * cancellable)
{
  GstBufferList *list = NULL;
  GstBufferListIterator *it = NULL;
  GstBuffer *buffer;
  GError *error = NULL;

  pad_data->cancellable = cancellable;

  /* Iterate over all the fragment's buffers and write them */
  list = gst_fragment_get_buffer_list (pad_data->fragment);
  if (list == NULL)
    goto done;
  it = gst_buffer_list_iterate (list);
  while (gst_buffer_list_iterator_next_group (it)) {
    while ((buffer = gst_buffer_list_iterator_next (it)) != NULL) {
      g_output_stream_write ((GOutputStream *) stream,
          GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer),
          pad_data->cancellable, &error);
      if (error)
        goto done;
    }
  }

done:{
    if (it)
      gst_buffer_list_iterator_free (it);
    if (list)
      gst_buffer_list_unref (list);
    pad_data->cancellable = NULL;
    return error;
  }
}

static GError *
gst_base_adaptive_sink_write_playlist (GstBaseAdaptiveSink * sink,
    gchar * playlist_str, GFileOutputStream * stream,
    GCancellable * cancellable)
{
  GError *error = NULL;

  g_output_stream_write ((GOutputStream *) stream, playlist_str,
      g_utf8_strlen (playlist_str, -1), NULL, &error);
  return error;
}

static GstFlowReturn
gst_base_adaptive_sink_write_element (GstBaseAdaptiveSink * sink,
    WriteFunc write_func, gchar * filename, gpointer data)
{
  GFile *file;
  GFileOutputStream *stream;
  GError *error = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GCancellable *cancellable;

  /* Create the new file */
  file = g_file_new_for_path (filename);
  cancellable = g_cancellable_new ();
  stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
      cancellable, &error);
  if (error) {
    if (error->code == G_IO_ERROR_CANCELLED)
      goto done;
    goto open_error;
  }

  if (write_func (sink, data, stream, cancellable) != NULL)
    goto write_error;

  /* Flush and close file */
  g_output_stream_flush ((GOutputStream *) stream, cancellable, &error);
  if (error)
    goto close_error;

  g_output_stream_close ((GOutputStream *) stream, cancellable, &error);
  if (error)
    goto close_error;

  ret = GST_FLOW_OK;
  goto done;

  /* ERRORS */
open_error:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
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
        GST_ELEMENT_ERROR (sink, RESOURCE, NO_SPACE_LEFT, (NULL), (NULL));
        ret = GST_FLOW_ERROR;
        break;
      }
      default:{
        GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
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

    GST_ELEMENT_ERROR (sink, RESOURCE, CLOSE,
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
    g_object_unref (file);
    g_object_unref (cancellable);

    return ret;
  }
}

static void
gst_base_adaptive_sink_create_empty_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstClockTime start_ts, guint64 offset,
    guint index)
{
  /* Create a new empty fragment */
  pad_data->fragment = gst_fragment_new ();
  pad_data->fragment->index = index;
  pad_data->fragment->start_ts = start_ts;
  pad_data->fragment->offset = offset;
  if (pad_data->streamheaders != NULL && sink->append_headers)
    gst_fragment_set_headers (pad_data->fragment, pad_data->streamheaders,
        pad_data->n_streamheaders);
}

static GstFlowReturn
gst_base_adaptive_sink_close_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstClockTime stop_ts)
{
  GstClockTime duration;
  GList *old_files;
  GstFlowReturn ret = GST_FLOW_OK;
  gchar *fragment_filename = NULL;

  pad_data->fragment->completed = TRUE;
  pad_data->fragment->stop_ts = stop_ts;
  duration = pad_data->fragment->stop_ts - pad_data->fragment->start_ts;

  /* Add the new entry to the playlist after the fragment was succeesfully
   * written to disk */
  gst_base_media_manager_add_fragment (sink->media_manager,
      pad_data->pad, pad_data->fragment, &old_files);

  /* Delete old files */
  if (sink->delete_old_files && old_files != NULL) {
    g_list_foreach (old_files, (GFunc) g_file_delete, NULL);
  }
  g_list_foreach (old_files, (GFunc) g_object_unref, NULL);
  g_list_free (old_files);

  /* Write playlist to disk */
  if (sink->write_to_disk) {
    GstMediaManagerFile *mg_file;

    GST_DEBUG_OBJECT (sink, "Writting playlist to disk");
    mg_file = gst_base_media_manager_render (sink->media_manager,
        pad_data->pad);

    if (mg_file != NULL) {
      gchar *filename;

      filename = g_build_filename (sink->output_directory, mg_file->filename,
          NULL);
      ret = gst_base_adaptive_sink_write_element (sink,
          (WriteFunc) gst_base_adaptive_sink_write_playlist,
          filename, mg_file->content);

      g_free (filename);
      if (ret != GST_FLOW_OK) {
        gst_base_media_manager_file_free (mg_file);
        goto done;
      }

      g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_NEW_PLAYLIST],
          0, mg_file->filename, mg_file->content);
      gst_base_media_manager_file_free (mg_file);
    }
  }

  /* Build fragment file path and url */
  fragment_filename =
      g_build_filename (sink->output_directory, pad_data->fragment->name, NULL);

  GST_INFO_OBJECT (sink, "Created new fragment name:%s duration: %"
      GST_TIME_FORMAT, fragment_filename, GST_TIME_ARGS (duration));

  /* Write fragment to disk */
  if (sink->write_to_disk) {
    GST_DEBUG_OBJECT (sink, "Writting fragment to disk");
    ret = gst_base_adaptive_sink_write_element (sink,
        (WriteFunc) gst_base_adaptive_sink_write_fragment,
        fragment_filename, pad_data);
    if (ret != GST_FLOW_OK)
      goto done;
  }
  //g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_NEW_FRAGMENT], 0);

done:
  {
    if (fragment_filename != NULL)
      g_free (fragment_filename);
    g_object_unref (pad_data->fragment);
    pad_data->fragment = NULL;
    return ret;
  }
}


static GstBaseAdaptivePadData *
gst_base_adaptive_sink_pad_data_new (GstPad * pad)
{
  GstBaseAdaptivePadData *pad_data;

  pad_data = g_new0 (GstBaseAdaptivePadData, 1);
  gst_object_ref (pad);
  pad_data->pad = pad;
  pad_data->parsed = FALSE;
  pad_data->decoded_caps = NULL;
  pad_data->new_fragment = FALSE;
  pad_data->new_fragment_index = 0;
  pad_data->new_fragment_ts = GST_CLOCK_TIME_NONE;
  pad_data->fragment = NULL;
  pad_data->cancellable = NULL;
  pad_data->n_streamheaders = 0;
  pad_data->streamheaders = NULL;
  pad_data->discover_lock = g_mutex_new ();
  pad_data->discover_cond = g_cond_new ();

  return pad_data;
}


static void
gst_base_adaptive_sink_pad_data_free (GstBaseAdaptivePadData * pad_data)
{
  gint i;

  if (pad_data->fragment != NULL) {
    g_object_unref (pad_data->fragment);
    pad_data->fragment = NULL;
  }

  if (pad_data->streamheaders != NULL) {
    for (i = 0; i < pad_data->n_streamheaders; i++) {
      if (pad_data->streamheaders[i] != NULL)
        gst_buffer_unref (pad_data->streamheaders[i]);
    }
    g_free (pad_data->streamheaders);
    pad_data->streamheaders = NULL;
  }

  if (pad_data->cancellable != NULL) {
    g_object_unref (pad_data->cancellable);
    pad_data->cancellable = NULL;
  }

  if (pad_data->decoded_caps != NULL) {
    g_list_foreach (pad_data->decoded_caps, (GFunc) gst_caps_unref, NULL);
    g_list_free (pad_data->decoded_caps);
    pad_data->decoded_caps = NULL;
  }

  if (pad_data->pad != NULL) {
    gst_object_unref (pad_data->pad);
    pad_data->pad = NULL;
  }

  if (pad_data->discover_lock != NULL) {
    g_mutex_free (pad_data->discover_lock);
    pad_data->discover_lock = NULL;
  }

  if (pad_data->discover_cond != NULL) {
    g_cond_free (pad_data->discover_cond);
    pad_data->discover_cond = NULL;
  }

  g_free (pad_data);
}


static void
on_new_decoded_pad_cb (GstElement * bin, GstPad * pad, gboolean is_last,
    GstBaseAdaptivePadData * pad_data)
{
  GstCaps *caps;

  caps = gst_pad_get_caps_reffed (pad);
  pad_data->decoded_caps = g_list_append (pad_data->decoded_caps,
      gst_caps_copy (caps));
  gst_caps_unref (caps);
}

static void
on_drained_cb (GstElement * bin, GstBaseAdaptivePadData * pad_data)
{
  g_cond_signal (pad_data->discover_cond);
}

static void
gst_base_adaptive_sink_parse_stream (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstBuffer * buf)
{
  GstElement *pipeline;
  GstElement *appsrc;
  GstElement *decodebin;
  GTimeVal timeout;

  pipeline = gst_pipeline_new ("pipeline");
  appsrc = gst_element_factory_make ("appsrc", "appsrc");
  decodebin = gst_element_factory_make ("decodebin2", "decodebin2");

  gst_bin_add_many (GST_BIN (pipeline), appsrc, decodebin, NULL);
  gst_element_link (appsrc, decodebin);

  g_signal_connect (decodebin, "drained", G_CALLBACK (on_drained_cb), pad_data);
  g_signal_connect (decodebin, "new-decoded-pad",
      G_CALLBACK (on_new_decoded_pad_cb), pad_data);

  g_object_set (G_OBJECT (appsrc), "is-live", TRUE, "num-buffers", 1, NULL);
  g_object_set (G_OBJECT (decodebin), "caps",
      gst_caps_from_string ("video/x-h264"), NULL);

  g_get_current_time (&timeout);
  g_time_val_add (&timeout, 5 * 1000 * 1000);

  /* Push buffer and wait for the "drained" signal */
  g_mutex_lock (pad_data->discover_lock);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gst_buffer_ref (buf);
  gst_app_src_push_buffer (GST_APP_SRC (appsrc), buf);
  if (!g_cond_timed_wait (pad_data->discover_cond, pad_data->discover_lock,
          &timeout)) {
    GST_WARNING ("Timed out decoding the fragment");
  }
  g_mutex_unlock (pad_data->discover_lock);

  /* Dispose the pipeline */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_element_get_state (pipeline, NULL, NULL, 0);
  g_object_unref (pipeline);
}
