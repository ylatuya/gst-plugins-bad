/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstbaseadaptivesink.c:
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
 * @short_description: Base class for adaptive streaming sink elements
 * @see_also: #GstBaseSink
 *
 * #GstBaseAdaptiveSink is the base class for sink elements in GStreamer that
 * needs to fragment incoming streams and provide a media representation in the
 * form of manifest or playlist for adaptive streaming formats such as DASH,
 * HLS or Smooth Streaming.
 *
 * #GstBaseAdaptiveSink will use the GstForceKeyUnit events to fragment the
 * incoming data and update the media representation.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib/gstdio.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include "gstbaseadaptivesink.h"
#include "gstadaptive-marshal.h"

GST_DEBUG_CATEGORY_STATIC (gst_base_adaptive_sink_debug);
#define GST_CAT_DEFAULT gst_base_adaptive_sink_debug

#define GST_PAD_DATA_LOCK(p) (g_mutex_lock(p->lock))
#define GST_PAD_DATA_UNLOCK(p) (g_mutex_unlock(p->lock))

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
  PROP_MAX_WINDOW,
  PROP_FRAGMENT_DURATION,
  PROP_LAST
};

typedef struct
{
  GstPad *pad;
  gchar *stream_name;
  gboolean parsed;
  GList *decoded_caps;
  gboolean new_fragment;
  guint new_fragment_index;
  GstClockTime new_fragment_ts;
  GstBuffer *fragment;
  GstBuffer *last_fragment;
  GCancellable *cancellable;
  guint count;
  GFile *init_segment;
  gboolean is_eos;

  GMutex *lock;
  GMutex *discover_lock;
  GCond *discover_cond;

  GstBuffer *streamheaders;
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
    GstPadTemplate * template, const gchar * name, const GstCaps * caps);
static void gst_base_adaptive_sink_release_pad (GstElement * element,
    GstPad * pad);
static gboolean gst_base_adaptive_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_base_adaptive_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_base_adaptive_sink_set_caps (GstBaseAdaptiveSink * sink,
    GstPad * pad, GstCaps * caps);
static GstStateChangeReturn gst_base_adaptive_sink_change_state (GstElement *
    element, GstStateChange transition);

/*GstBaseAdaptiveSink */
typedef GError *(*WriteFunc) (GstBaseAdaptiveSink * sink,
    GFileOutputStream * output_stream, gpointer data,
    GCancellable * cancellable);
static GstFlowReturn gst_base_adaptive_sink_close_fragment (GstBaseAdaptiveSink
    * sink, GstBaseAdaptivePadData * pad_data, GstClockTime stop_ts);
static gboolean gst_base_adaptive_sink_request_first_fragments
    (GstBaseAdaptiveSink * sink);
static gboolean gst_base_adaptive_process_new_stream (GstBaseAdaptiveSink *
    sink, GstPad * pad, GstBaseAdaptivePadData * pad_data);

G_DEFINE_TYPE (GstBaseAdaptiveSink, gst_base_adaptive_sink, GST_TYPE_ELEMENT);

static guint gst_base_adaptive_sink_signals[LAST_SIGNAL] = { 0 };

static GstBaseAdaptivePadData *
gst_base_adaptive_sink_pad_data_new (GstPad * pad)
{
  GstBaseAdaptivePadData *pad_data;

  pad_data = g_new0 (GstBaseAdaptivePadData, 1);
  gst_object_ref (pad);
  pad_data->pad = pad;
  pad_data->stream_name = gst_pad_get_name (pad);
  pad_data->parsed = FALSE;
  pad_data->decoded_caps = NULL;
  pad_data->new_fragment = FALSE;
  pad_data->new_fragment_index = 0;
  pad_data->count = 0;
  pad_data->new_fragment_ts = GST_CLOCK_TIME_NONE;
  pad_data->fragment = NULL;
  pad_data->cancellable = NULL;
  pad_data->streamheaders = NULL;
  pad_data->lock = g_mutex_new ();
  pad_data->discover_lock = g_mutex_new ();
  pad_data->discover_cond = g_cond_new ();
  pad_data->init_segment = FALSE;
  pad_data->is_eos = FALSE;

  return pad_data;
}

static void
gst_base_adaptive_sink_pad_data_free (GstBaseAdaptivePadData * pad_data)
{
  if (pad_data->stream_name != NULL) {
    g_free (pad_data->stream_name);
    pad_data->stream_name = NULL;
  }

  if (pad_data->fragment != NULL) {
    g_object_unref (pad_data->fragment);
    pad_data->fragment = NULL;
  }

  if (pad_data->streamheaders != NULL) {
    gst_buffer_unref (pad_data->streamheaders);
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
    //gst_object_unref (pad_data->pad);
    pad_data->pad = NULL;
  }

  if (pad_data->lock != NULL) {
    g_mutex_free (pad_data->lock);
    pad_data->lock = NULL;
  }

  if (pad_data->discover_lock != NULL) {
    g_mutex_free (pad_data->discover_lock);
    pad_data->discover_lock = NULL;
  }

  if (pad_data->discover_cond != NULL) {
    g_cond_free (pad_data->discover_cond);
    pad_data->discover_cond = NULL;
  }

  if (pad_data->init_segment != NULL) {
    g_object_unref (pad_data->init_segment);
    pad_data->init_segment = NULL;
  }

  g_free (pad_data);
}

static void
gst_base_adaptive_sink_class_init (GstBaseAdaptiveSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_base_adaptive_sink_set_property;
  gobject_class->get_property = gst_base_adaptive_sink_get_property;

  GST_DEBUG_CATEGORY_INIT (gst_base_adaptive_sink_debug, "baseadaptivesink", 0,
      "sink element");

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
   * Whether the fragments and the playlist should be written to disk or not
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
   * Whether create a new file for each fragment or append them to same file
   *
   */
  g_object_class_install_property (gobject_class, PROP_CHUNKED,
      g_param_spec_boolean ("chunked", "Chunked",
          "Create a new file for each fragment", TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:is-live
   *
   * Whether the media is live or on-demand
   *
   */
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is live",
          "Whether the media is live or on-demand", TRUE, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:max-window
   *
   * Maximum DVR window in seconds
   *
   */
  g_object_class_install_property (gobject_class, PROP_MAX_WINDOW,
      g_param_spec_uint ("max-window", "Max window",
          "Maximum window for DVR", 0, 0, G_PARAM_READWRITE));

  /**
   * GstBaseAdaptiveSink:fragment-duration
   *
   * Fragments duration in seconds
   *
   */
  g_object_class_install_property (gobject_class, PROP_FRAGMENT_DURATION,
      g_param_spec_uint ("fragment-duration", "Fragment duration",
          "Duration of fragments in seconds", 0, G_MAXUINT32, 10,
          G_PARAM_READWRITE));
  /**
   * GstBaseAdaptiveSink::eos:
   * @sink: the sink element that emited the signal
   *
   * Signal that the end-of-stream has been reached.
   *
   * This signal is emited from the steaming thread.
   */
  gst_base_adaptive_sink_signals[SIGNAL_EOS] =
      g_signal_new ("eos", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstBaseAdaptiveSinkClass, eos),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstBaseAdaptiveSink::new-playlist:
   * @sink: the sink element that emited the signal
   * @filename: the filename of the media representation.
   * @content: the content of the media representation.
   *
   * This signal gets emitted when a media presentation has been updated.
   *
   * This signal is emited from the steaming thread.
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
   * @fragment: the #GstFragment that was created
   *
   * This signal gets emitted when a new fragment has been created.
   *
   * This signal is emited from the steaming thread.
   *
   */
  gst_base_adaptive_sink_signals[SIGNAL_NEW_FRAGMENT] =
      g_signal_new ("new-fragment", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstBaseAdaptiveSinkClass,
          new_fragment), NULL, NULL, __gst_adaptive_marshal_VOID__BOXED,
      G_TYPE_NONE, 1, GST_TYPE_BUFFER);

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
gst_base_adaptive_sink_init (GstBaseAdaptiveSink * sink)
{

  sink->pad_datas = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gst_base_adaptive_sink_pad_data_free);
  sink->write_to_disk = TRUE;
  sink->delete_old_files = TRUE;
  sink->chunked = TRUE;
  sink->is_live = FALSE;
  sink->needs_init_segment = FALSE;
  sink->md_name = NULL;
  sink->base_url = NULL;
  sink->output_directory = NULL;
  sink->fragment_prefix = NULL;
  sink->fragment_tpl = "%s-%s-%05d.frag";
  sink->fragment_duration = 10 * GST_SECOND;
  sink->prepend_headers = TRUE;
  sink->max_window = 0;
  /* This should be left to 1 until muxer supports properly queing force key
   * unit events */
  sink->min_cache = 1;

  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_FLAG_SINK);
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

  if (sink->md_name != NULL) {
    g_free (sink->md_name);
    sink->md_name = NULL;
  }

  if (sink->output_directory != NULL) {
    g_free (sink->output_directory);
    sink->output_directory = NULL;
  }

  if (sink->fragment_prefix != NULL) {
    g_free (sink->fragment_prefix);
    sink->fragment_prefix = NULL;
  }

  if (sink->pad_datas != NULL) {
    g_hash_table_destroy (sink->pad_datas);
    sink->pad_datas = NULL;
  }

  G_OBJECT_CLASS (gst_base_adaptive_sink_parent_class)->finalize (object);
}

static void
gst_base_adaptive_sink_dispose (GObject * object)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  if (sink->streams_manager != NULL) {
    g_object_unref (sink->streams_manager);
    sink->streams_manager = NULL;
  }

  if (sink->supported_caps != NULL) {
    gst_caps_unref (sink->supported_caps);
    sink->supported_caps = NULL;
  }

  G_OBJECT_CLASS (gst_base_adaptive_sink_parent_class)->dispose (object);
}

void
gst_base_adaptive_sink_set_output_directory (GstBaseAdaptiveSink * sink,
    const gchar * output_dir)
{
  g_free (sink->output_directory);
  sink->output_directory = g_strdup (output_dir);
}

void
gst_base_adaptive_sink_set_base_url (GstBaseAdaptiveSink * sink,
    const gchar * base_url)
{
  g_free (sink->base_url);
  sink->base_url = g_strdup (base_url);
}

void
gst_base_adaptive_sink_set_md_name (GstBaseAdaptiveSink * sink,
    const gchar * name)
{
  g_free (sink->md_name);
  sink->md_name = g_strdup (name);
}

void
gst_base_adaptive_sink_set_fragment_prefix (GstBaseAdaptiveSink * sink,
    const gchar * fragment_prefix)
{
  g_free (sink->fragment_prefix);
  sink->fragment_prefix = g_strdup (fragment_prefix);
}

static void
gst_base_adaptive_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseAdaptiveSink *sink = GST_BASE_ADAPTIVE_SINK (object);

  switch (prop_id) {
    case PROP_OUTPUT_DIRECTORY:
      gst_base_adaptive_sink_set_output_directory (sink,
          g_value_get_string (value));
      break;
    case PROP_BASE_URL:
      gst_base_adaptive_sink_set_base_url (sink, g_value_get_string (value));
      break;
    case PROP_MD_NAME:
      gst_base_adaptive_sink_set_md_name (sink, g_value_get_string (value));
      break;
    case PROP_FRAGMENT_PREFIX:
      gst_base_adaptive_sink_set_fragment_prefix (sink,
          g_value_get_string (value));
      break;
    case PROP_WRITE_TO_DISK:
      sink->write_to_disk = g_value_get_boolean (value);
      break;
    case PROP_DELETE_OLD_FILES:
      sink->delete_old_files = g_value_get_boolean (value);
      break;
    case PROP_CHUNKED:
      sink->chunked = g_value_get_boolean (value);
      break;
    case PROP_IS_LIVE:
      sink->is_live = g_value_get_boolean (value);
      break;
    case PROP_MAX_WINDOW:
      sink->max_window = g_value_get_uint (value);
      break;
    case PROP_FRAGMENT_DURATION:
      sink->fragment_duration = g_value_get_uint (value) * GST_SECOND;
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
    case PROP_MAX_WINDOW:
      g_value_set_uint (value, sink->max_window);
      break;
    case PROP_FRAGMENT_DURATION:
      g_value_set_uint (value, sink->fragment_duration / GST_SECOND);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
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
      continue;

    g_cancellable_cancel (pad_data->cancellable);
    g_object_unref (pad_data->cancellable);
    pad_data->cancellable = NULL;
  }
}

static void
gst_base_adaptive_sink_stop (GstBaseAdaptiveSink * sink)
{
  GHashTableIter iter;
  GstPad *pad;
  GstBaseAdaptivePadData *pad_data;

  /* Clear all pad data */
  g_hash_table_iter_init (&iter, sink->pad_datas);
  while (g_hash_table_iter_next (&iter, (void *) &pad, (void *) &pad_data)) {
    GST_PAD_DATA_LOCK (pad_data);
    if (pad_data->fragment != NULL) {
      gst_base_adaptive_sink_close_fragment (sink, pad_data,
          GST_CLOCK_TIME_NONE);
    }
    g_hash_table_remove (sink->pad_datas, pad_data);
    GST_PAD_DATA_UNLOCK (pad_data);
  }

  g_object_unref (sink->streams_manager);
  sink->streams_manager = NULL;
}

static void
gst_base_adaptive_sink_start (GstBaseAdaptiveSink * sink)
{
  GstBaseAdaptiveSinkClass *b_class = GST_BASE_ADAPTIVE_SINK_GET_CLASS (sink);

  if (b_class->create_streams_manager != NULL)
    sink->streams_manager = b_class->create_streams_manager (sink);
  else
    sink->streams_manager = gst_streams_manager_new ();
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
      gst_base_adaptive_sink_start (sink);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (sink->fragment_duration) {
        gst_base_adaptive_sink_request_first_fragments (sink);
      }
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_base_adaptive_sink_parent_class)->change_state
      (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
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

  return ret;
}

static GstPad *
gst_base_adaptive_sink_request_new_pad (GstElement * element,
    GstPadTemplate * template, const gchar * name, const GstCaps * caps)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;
  GstPad *pad;

  sink = GST_BASE_ADAPTIVE_SINK (element);

  pad = gst_pad_new_from_template (template, name);
  gst_pad_set_event_function (pad, gst_base_adaptive_sink_event);
  gst_pad_set_chain_function (pad, gst_base_adaptive_sink_chain);

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

  sink = GST_BASE_ADAPTIVE_SINK (element);
  g_hash_table_remove (sink->pad_datas, pad);
}

static gboolean
gst_base_adaptive_sink_send_force_key_unit_event (GstBaseAdaptiveSink * sink,
    GstPad * pad, GstClockTime ts, guint count)
{
  GstEvent *event;

  event = gst_video_event_new_upstream_force_key_unit (ts,
      sink->prepend_headers, count);

  if (!gst_pad_push_event (pad, event)) {
    GST_WARNING_OBJECT (sink, "Failed to push upstream force key unit event");
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_base_adaptive_sink_request_new_fragment (GstBaseAdaptiveSink * sink,
    GstPad * pad)
{
  GstBaseAdaptivePadData *pad_data;
  GstClockTime ts;
  gboolean ret;

  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  if (pad_data == NULL)
    return FALSE;

  ts = sink->fragment_duration * pad_data->count;
  GST_DEBUG_OBJECT (sink,
      "Requesting new fragment upstream for pad %s:%s and ts:%" GST_TIME_FORMAT,
      GST_DEBUG_PAD_NAME (pad), GST_TIME_ARGS (ts));
  ret =
      gst_base_adaptive_sink_send_force_key_unit_event (sink, pad, ts,
      pad_data->count);
  pad_data->count++;
  return ret;
}

static gboolean
gst_base_adaptive_sink_request_first_fragments (GstBaseAdaptiveSink * sink)
{
  GList *tmp;
  gint i;

  tmp = g_list_first (GST_ELEMENT_PADS (GST_ELEMENT (sink)));

  do {
    for (i = 0; i < sink->min_cache; i++) {
      gst_base_adaptive_sink_request_new_fragment (sink, (GstPad *) tmp->data);
    }
    tmp = g_list_next (tmp);
  } while (tmp != NULL);

  return TRUE;
}

static void
gst_base_adaptive_sink_create_empty_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstClockTime start_ts,
    guint64 offset, guint index)
{
  GstFragmentMeta *meta;
  gchar *frag_name, *frag_filename;

  /* Create a new empty fragment */
  pad_data->fragment = gst_fragment_new ();

  /* Fill the fragment's metadata */
  meta = gst_buffer_get_fragment_meta (pad_data->fragment);
  meta->index = index;
  GST_BUFFER_PTS (pad_data->fragment) = start_ts;
  GST_BUFFER_OFFSET (pad_data->fragment) = offset;

  /* If fragments are not chunked in files, reuse the same file (with index=0) */
  if (!sink->chunked)
    index = 0;

  frag_name = g_strdup_printf (sink->fragment_tpl, sink->fragment_prefix,
      pad_data->stream_name, index);
  frag_filename = g_build_path (G_DIR_SEPARATOR_S, pad_data->stream_name,
      frag_name, NULL);
  gst_fragment_set_name (pad_data->fragment, frag_filename);

  GST_DEBUG_OBJECT (sink, "Created empty fragment %s", frag_filename);
  g_free (frag_name);
}

static GstFlowReturn
gst_base_adaptive_sink_chain (GstPad * pad, GstObject * element,
    GstBuffer * buffer)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;
  GstFlowReturn ret = GST_FLOW_OK;

  sink = GST_BASE_ADAPTIVE_SINK (element);
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  GST_LOG_OBJECT (sink, "New buffer in chain function");

  /* Check if a new fragment needs to be created and save the old one updating
   * the playlist too */
  GST_PAD_DATA_LOCK (pad_data);
  if (pad_data->new_fragment) {
    guint64 offset;
    guint index;
    GstClockTime ts;

    pad_data->new_fragment = FALSE;
    ts = pad_data->new_fragment_ts;
    index = pad_data->new_fragment_index;
    offset = 0;

    if (pad_data->fragment != NULL) {
      /* Parse headers for this stream and add it to the media manager */
      if (G_UNLIKELY (!pad_data->parsed)) {
        if (!gst_base_adaptive_process_new_stream (sink, pad, pad_data)) {
          ret = GST_FLOW_ERROR;
          goto quit;
        }
      }

      offset = GST_BUFFER_OFFSET (pad_data->fragment) +
          gst_buffer_get_size (pad_data->fragment);

      ret = gst_base_adaptive_sink_close_fragment (sink, pad_data, ts);
      if (ret != GST_FLOW_OK)
        goto quit;
    }
    if (sink->fragment_duration)
      gst_base_adaptive_sink_request_new_fragment (sink, pad);
    gst_base_adaptive_sink_create_empty_fragment (sink, pad_data, ts,
        offset, index);
  }

  /* Drop this buffer if we haven't started a fragment yet */
  if (pad_data->fragment == NULL) {
    GST_DEBUG_OBJECT (sink,
        "Dropping buffer because no fragment was started yet");
    goto quit;
  }

  GST_LOG_OBJECT (sink, "Added new buffer to fragment with timestamp: %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (buffer)));
  if (G_UNLIKELY (GST_BUFFER_PTS (pad_data->fragment) == GST_CLOCK_TIME_NONE))
    GST_BUFFER_PTS (pad_data->fragment) = GST_BUFFER_PTS (buffer);
  GST_BUFFER_DURATION (pad_data->fragment) =
      GST_BUFFER_PTS (buffer) - GST_BUFFER_PTS (pad_data->fragment);
  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DURATION (buffer)))
    GST_BUFFER_DURATION (pad_data->fragment) += GST_BUFFER_DURATION (buffer);

  gst_fragment_add_buffer (pad_data->fragment, buffer);

quit:
  {
    GST_PAD_DATA_UNLOCK (pad_data);
    return ret;
  }
}

static void
gst_base_adaptive_sink_get_headers_from_caps (GstBaseAdaptiveSink * sink,
    GstCaps * caps, GstBaseAdaptivePadData * pad_data)
{
  GstStructure *structure;
  const GValue *value;
  gint i = 0;

  structure = gst_caps_get_structure (caps, 0);
  if (!structure)
    return;

  value = gst_structure_get_value (structure, "streamheader");
  if (!GST_VALUE_HOLDS_ARRAY (value)) {
    return;
  }

  /* Clear old streamheaders */
  if (pad_data->streamheaders) {
    gst_buffer_unref (pad_data->streamheaders);
    pad_data->streamheaders = NULL;
  }

  for (i = 0; i < gst_value_array_get_size (value); i++) {
    const GValue *val = gst_value_array_get_value (value, i);
    GstBuffer *buf;

    if (!val)
      continue;

    buf = gst_value_get_buffer (val);
    if (!GST_IS_BUFFER (buf))
      continue;

    gst_buffer_ref (buf);
    if (pad_data->streamheaders == NULL) {
      pad_data->streamheaders = buf;
    } else {
      pad_data->streamheaders =
          gst_buffer_append (pad_data->streamheaders, buf);
    }
  }
}

static gboolean
gst_base_adaptive_sink_set_caps (GstBaseAdaptiveSink * sink, GstPad * pad,
    GstCaps * caps)
{
  GstBaseAdaptivePadData *pad_data;

  pad_data = g_hash_table_lookup (sink->pad_datas, pad);
  gst_base_adaptive_sink_get_headers_from_caps (sink, caps, pad_data);
  return TRUE;
}

static GstFlowReturn
gst_base_adaptive_sink_write_element (GstBaseAdaptiveSink * sink,
    WriteFunc write_func, gchar * filename, gboolean append, gpointer data)
{
  GFile *file;
  GFileOutputStream *stream;
  GError *error = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GCancellable *cancellable;

  /* Create the new file */
  file = g_file_new_for_path (filename);
  cancellable = g_cancellable_new ();
  if (append) {
    stream = g_file_append_to (file, G_FILE_CREATE_REPLACE_DESTINATION,
        cancellable, &error);
  } else {
    stream = g_file_replace (file, NULL, FALSE,
        G_FILE_CREATE_REPLACE_DESTINATION, cancellable, &error);
  }
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
      ret = GST_FLOW_ERROR;
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
      ret = GST_FLOW_ERROR;
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

static GError *
gst_base_adaptive_sink_write_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GFileOutputStream * stream,
    GCancellable * cancellable)
{
  GstMapInfo map;
  GError *error = NULL;

  pad_data->cancellable = cancellable;

  gst_buffer_map (pad_data->fragment, &map, GST_MAP_READ);
  g_output_stream_write ((GOutputStream *) stream,
      map.data, map.size, pad_data->cancellable, &error);
  gst_buffer_unmap (pad_data->fragment, &map);

  pad_data->cancellable = NULL;
  return error;
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

static gboolean
gst_base_adaptive_sink_set_stream_headers (GstBaseAdaptiveSink * sink,
    GstPad * pad, GstBaseAdaptivePadData * pad_data)
{
  GstBuffer *fragment;
  GstFragmentMeta *meta;
  gboolean ret = TRUE;

  if (pad_data->streamheaders != NULL)
    return TRUE;

  /* Create a new fragment with the stream headers */
  fragment = gst_fragment_new ();
  gst_fragment_set_headers (fragment, pad_data->streamheaders);
  meta = gst_buffer_get_fragment_meta (fragment);
  meta->completed = FALSE;

  /* Add initialization segment */
  gst_streams_manager_add_headers (sink->streams_manager, pad, fragment);

  if (sink->needs_init_segment) {
    GstFlowReturn fret;
    gchar *filename;

    filename = g_build_filename (sink->output_directory,
        pad_data->stream_name, NULL);

    pad_data->init_segment = g_file_new_for_path (filename);

    GST_DEBUG_OBJECT (sink, "Writting initialization segment");
    pad_data->fragment = fragment;

    fret = gst_base_adaptive_sink_write_element (sink,
        (WriteFunc) gst_base_adaptive_sink_write_fragment,
        filename, FALSE, pad_data);
    g_free (filename);

    if (fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (sink, "Could not write headers fragment");
      ret = FALSE;
    }
    pad_data->fragment = NULL;
  }

  gst_buffer_unref (fragment);
  return ret;
}

static void
on_new_decoded_pad_cb (GstElement * bin, GstPad * pad,
    GstBaseAdaptivePadData * pad_data)
{
  GstCaps *caps;

  caps = gst_pad_query_caps (pad, NULL);
  pad_data->decoded_caps = g_list_prepend (pad_data->decoded_caps, caps);
}

static void
on_drained_cb (GstElement * bin, GstBaseAdaptivePadData * pad_data)
{
  g_cond_signal (pad_data->discover_cond);
}

static gboolean
gst_base_adaptive_sink_parse_stream (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data)
{
  GstElement *pipeline, *appsrc, *decodebin;
  GTimeVal timeout;
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (sink, "Demuxing first fragment");

  pipeline = gst_pipeline_new ("pipeline");
  appsrc = gst_element_factory_make ("appsrc", "appsrc");
  decodebin = gst_element_factory_make ("decodebin", "decodebin");

  gst_bin_add_many (GST_BIN (pipeline), appsrc, decodebin, NULL);
  gst_element_link (appsrc, decodebin);

  g_signal_connect (decodebin, "drained", G_CALLBACK (on_drained_cb), pad_data);
  g_signal_connect (decodebin, "pad_added",
      G_CALLBACK (on_new_decoded_pad_cb), pad_data);

  if (sink->supported_caps != NULL)
    g_object_set (G_OBJECT (decodebin), "caps", sink->supported_caps, NULL);

  g_get_current_time (&timeout);
  g_time_val_add (&timeout, 5 * 1000 * 1000);

  /* Push buffer and wait for the "drained" signal */
  g_mutex_lock (pad_data->discover_lock);

  pad_data->decoded_caps = NULL;

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Push the headers and the first fragment */
  if (pad_data->streamheaders && !sink->chunked) {
    gst_buffer_ref (pad_data->streamheaders);
    gst_app_src_push_buffer (GST_APP_SRC (appsrc), pad_data->streamheaders);
  }
  gst_buffer_ref (pad_data->fragment);
  gst_app_src_push_buffer (GST_APP_SRC (appsrc), pad_data->fragment);
  gst_app_src_end_of_stream (GST_APP_SRC (appsrc));

  if (!g_cond_timed_wait (pad_data->discover_cond, pad_data->discover_lock,
          &timeout)) {
    GST_WARNING_OBJECT (sink, "Timed out decoding the fragment");
    ret = FALSE;
  }
  g_mutex_unlock (pad_data->discover_lock);

  /* Dispose the pipeline */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_element_get_state (pipeline, NULL, NULL, 0);
  g_object_unref (pipeline);
  return ret;
}

static gboolean
gst_base_adaptive_sink_update_playlist (GstBaseAdaptiveSink * sink,
    GstMediaRepFile * rep_file)
{
  gchar *filename;
  gboolean ret = TRUE;

  if (rep_file == NULL)
    return TRUE;

  filename = g_file_get_path (rep_file->file);
  GST_DEBUG_OBJECT (sink, "Updating playlist: %s", filename);
  g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_NEW_PLAYLIST],
      0, filename, rep_file->content);

  if (!sink->write_to_disk)
    goto done;

  ret = gst_base_adaptive_sink_write_element (sink,
      (WriteFunc) gst_base_adaptive_sink_write_playlist,
      filename, FALSE, rep_file->content);
  if (ret != GST_FLOW_OK)
    ret = FALSE;

done:
  gst_media_rep_file_free (rep_file);
  g_free (filename);
  return ret;
}

static gboolean
gst_base_adaptive_process_new_stream (GstBaseAdaptiveSink * sink, GstPad * pad,
    GstBaseAdaptivePadData * pad_data)
{
  GstMediaRepFile *rep_file;
  guint avg_bitrate;

  GST_DEBUG_OBJECT (sink, "Parsing first fragment for pad %s:%s",
      GST_DEBUG_PAD_NAME (pad));

  /* Parse/demux the first fragment to find out the substreams for this pad */
  if (!gst_base_adaptive_sink_parse_stream (sink, pad_data)) {
    GST_ERROR_OBJECT (sink, "The stream is not in any support format");
    GST_PAD_DATA_UNLOCK (pad_data);
    return FALSE;
  }

  pad_data->parsed = TRUE;

  GST_INFO_OBJECT (sink, "Adding new stream for pad %s:%s",
      GST_DEBUG_PAD_NAME (pad));

  /* Calculate the average bitrate for this stream */
  avg_bitrate = gst_buffer_get_size (pad_data->fragment);
  avg_bitrate /= sink->fragment_duration / GST_SECOND;

  /* Notify the streams manager about this new stream */
  if (!gst_streams_manager_add_stream (sink->streams_manager, pad, avg_bitrate,
          pad_data->decoded_caps, &rep_file)) {
    GST_ERROR_OBJECT (sink, "Could not add stream for pad %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    return FALSE;
  }

  /* Create a new folder with the stream name in the output directory, where
   * fragments are saved */
  if (sink->write_to_disk) {
    gchar *path;

    path = g_build_filename (sink->output_directory, pad_data->stream_name,
        NULL);

    if (g_file_test (path, G_FILE_TEST_EXISTS)) {
      g_remove (path);
    }
    if (g_mkdir_with_parents (path, 0755)) {
      GST_ERROR_OBJECT (sink, "Could not create output directory %s", path);
      g_free (path);
      return FALSE;
    };
    g_free (path);
  }

  /* Adding a new stream might have changed any of the representation files */
  gst_base_adaptive_sink_update_playlist (sink, rep_file);

  gst_base_adaptive_sink_set_stream_headers (sink, pad, pad_data);
  return TRUE;
}

static gboolean
gst_base_adaptive_sink_all_pads_eos (GstBaseAdaptiveSink * sink)
{
  GList *pad_datas;
  gboolean eos = TRUE;

  pad_datas = g_hash_table_get_values (sink->pad_datas);
  while (pad_datas) {
    GstBaseAdaptivePadData *p = (GstBaseAdaptivePadData *) pad_datas->data;
    if (!p->is_eos) {
      eos = FALSE;
      break;
    }
    pad_datas = g_list_next (pad_datas);
  }

  return eos;
}

static gboolean
gst_base_adaptive_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstBaseAdaptiveSink *sink;
  GstBaseAdaptivePadData *pad_data;
  GstClockTime timestamp, stream_time, running_time;
  gboolean all_headers;
  guint count;

  sink = GST_BASE_ADAPTIVE_SINK (parent);
  pad_data = g_hash_table_lookup (sink->pad_datas, pad);

  if (gst_video_event_parse_downstream_force_key_unit (event, &timestamp,
          &stream_time, &running_time, &all_headers, &count)) {
    GST_DEBUG_OBJECT (sink, "Received GstForceKeyUnit event");
    GST_PAD_DATA_LOCK (pad_data);
    pad_data->new_fragment_index = count;
    pad_data->new_fragment_ts = timestamp;
    pad_data->new_fragment = TRUE;
    GST_PAD_DATA_UNLOCK (pad_data);
  }

  else if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GstMediaRepFile *rep_file;
    GstMessage *message;
    gboolean all_eos;

    gst_streams_manager_eos (sink->streams_manager, pad, &rep_file);
    gst_base_adaptive_sink_update_playlist (sink, rep_file);
    GST_PAD_DATA_LOCK (pad_data);
    pad_data->is_eos = TRUE;
    all_eos = gst_base_adaptive_sink_all_pads_eos (sink);
    GST_PAD_DATA_UNLOCK (pad_data);
    if (all_eos) {
      g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_EOS], 0);
      GST_DEBUG_OBJECT (sink, "Posting EOS");
      message = gst_message_new_eos (GST_OBJECT_CAST (sink));
      gst_element_post_message (GST_ELEMENT_CAST (sink), message);
    }
  }

  else if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
    GstCaps *caps;

    gst_event_parse_caps (event, &caps);
    gst_base_adaptive_sink_set_caps (sink, pad, caps);
  }

  return TRUE;
}

static GstFlowReturn
gst_base_adaptive_sink_close_fragment (GstBaseAdaptiveSink * sink,
    GstBaseAdaptivePadData * pad_data, GstClockTime stop_ts)
{
  GstClockTime duration;
  GstFragmentMeta *meta;
  GList *old_files;
  GFile *file;
  GstMediaRepFile *rep_file;
  GstFlowReturn ret = GST_FLOW_OK;
  gchar *fragment_filename = NULL;

  meta = gst_buffer_get_fragment_meta (pad_data->fragment);
  meta->completed = TRUE;
  if (GST_CLOCK_TIME_IS_VALID (stop_ts))
    GST_BUFFER_DURATION (pad_data->fragment) =
        stop_ts - GST_BUFFER_PTS (pad_data->fragment);
  duration = GST_BUFFER_DURATION (pad_data->fragment);

  /* Build fragment file */
  fragment_filename =
      g_build_filename (sink->output_directory, meta->name, NULL);
  file = g_file_new_for_path (fragment_filename);
  gst_fragment_set_file (pad_data->fragment, file);

  GST_INFO_OBJECT (sink, "Creating new fragment %s with duration: %"
      GST_TIME_FORMAT, fragment_filename, GST_TIME_ARGS (duration));

  /* Write fragment to disk */
  if (sink->write_to_disk) {
    GST_DEBUG_OBJECT (sink, "Writting fragment to disk %s", fragment_filename);
    ret = gst_base_adaptive_sink_write_element (sink,
        (WriteFunc) gst_base_adaptive_sink_write_fragment,
        fragment_filename, !sink->chunked, pad_data);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  /* Add the new entry to the playlist after the fragment was succeesfully
   * written to disk */
  gst_streams_manager_add_fragment (sink->streams_manager,
      pad_data->pad, pad_data->fragment, &rep_file, &old_files);

  /* Delete old files */
  if (sink->delete_old_files && old_files != NULL) {
    g_list_foreach (old_files, (GFunc) g_file_delete, NULL);
  }
  g_list_foreach (old_files, (GFunc) g_object_unref, NULL);
  g_list_free (old_files);

  /* Write playlist to disk */
  gst_base_adaptive_sink_update_playlist (sink, rep_file);

  g_signal_emit (sink, gst_base_adaptive_sink_signals[SIGNAL_NEW_FRAGMENT], 0);

done:
  {
    if (fragment_filename != NULL)
      g_free (fragment_filename);
    if (pad_data->fragment != NULL) {
      gst_buffer_unref (pad_data->fragment);
    }
    pad_data->fragment = NULL;
    return ret;
  }
}
