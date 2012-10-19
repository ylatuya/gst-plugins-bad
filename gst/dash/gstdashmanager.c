/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstbasemediamanager.c:
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

#include <gst/baseadaptive/gstfragment.h>

#include "gstdashmanager.h"
#include "mdp/gstmediapresentation.h"

#define ISOFF_HEADERS_EXTENSION ".m4v"
#define ISOFF_FRAGMENT_EXTENSION ".m4s"
#define MPEGTS_FRAGMENT_EXTENSION ".mpegts"
#define MDP_EXTENSION ".mdp"

static void gst_dash_manager_finalize (GObject * gobject);

static gboolean gst_dash_manager_add_stream (GstStreamsManager * b_manager,
    GstPad * pad, GstDiscovererVideoInfo * video_info,
    GstDiscovererAudioInfo * audio_info,
    GstDiscovererSubtitleInfo * subtitle_info, GstMediaRepFile ** rep_file);
static gboolean gst_dash_manager_remove_stream (GstStreamsManager * manager,
    GstPad * pad, GstMediaRepFile ** rep_file);
static gchar *gst_dash_manager_fragment_name (GstStreamsManager * manager,
    GstPad * pad, guint index);
static gchar *gst_dash_manager_headers_name (GstStreamsManager * manager,
    GstPad * pad);
static gboolean gst_dash_manager_add_fragment (GstStreamsManager * manager,
    GstPad * pad, GstBuffer * fragment, GstMediaRepFile ** rep_file,
    GList ** removed_fragments);
static gboolean gst_dash_manager_add_headers (GstStreamsManager * manager,
    GstPad * pad, GstBuffer * fragment);
static gboolean gst_dash_manager_render (GstStreamsManager * manager,
    GstPad * pad, GstMediaRepFile ** rep_file);
static void gst_dash_manager_clear (GstStreamsManager * manager);

G_DEFINE_TYPE (GstDashManager, gst_dash_manager, GST_TYPE_STREAMS_MANAGER);

static void
gst_dash_manager_class_init (GstDashManagerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstStreamsManagerClass *manager_class = GST_STREAMS_MANAGER_CLASS (klass);

  manager_class->add_stream = gst_dash_manager_add_stream;
  manager_class->remove_stream = gst_dash_manager_remove_stream;
  manager_class->fragment_name = gst_dash_manager_fragment_name;
  manager_class->headers_name = gst_dash_manager_headers_name;
  manager_class->add_fragment = gst_dash_manager_add_fragment;
  manager_class->add_headers = gst_dash_manager_add_headers;
  manager_class->render = gst_dash_manager_render;
  manager_class->clear = gst_dash_manager_clear;

  gobject_class->finalize = gst_dash_manager_finalize;
}

static void
gst_dash_manager_init (GstDashManager * manager)
{
  GstStreamsManager *base_manager;

  base_manager = GST_STREAMS_MANAGER (manager);
  gst_streams_manager_set_base_url (base_manager, g_strdup ("./"));
  gst_streams_manager_set_title (base_manager, g_strdup ("dash"));
  gst_streams_manager_set_fragment_prefix (base_manager, g_strdup ("segment"));
  base_manager->chunked = TRUE;
  base_manager->is_live = FALSE;
  base_manager->finished = FALSE;
  base_manager->window_size = GST_CLOCK_TIME_NONE;
  manager->mdp = NULL;
}

static void
gst_dash_manager_finalize (GObject * gobject)
{
  GstDashManager *manager = GST_DASH_MANAGER (gobject);

  if (manager->mdp != NULL) {
    gst_media_presentation_free (manager->mdp);
    manager->mdp = NULL;
  }

  G_OBJECT_CLASS (gst_dash_manager_parent_class)->finalize (gobject);
}

GstDashManager *
gst_dash_manager_new (gboolean is_live, gboolean chunked,
    guint64 min_buffer_time)
{
  GstDashManager *manager;


  manager = GST_DASH_MANAGER (g_object_new (GST_TYPE_DASH_MANAGER, NULL));
  manager->mdp = gst_media_presentation_new (is_live ?
      MEDIA_PRESENTATION_TYPE_LIVE : MEDIA_PRESENTATION_TYPE_ONDEMAND,
      !chunked, min_buffer_time);
  manager->min_buffer_time = min_buffer_time;
  return manager;
}

static gboolean
gst_dash_manager_add_stream (GstStreamsManager * b_manager, GstPad * pad,
    GstDiscovererVideoInfo * video_info,
    GstDiscovererAudioInfo * audio_info,
    GstDiscovererSubtitleInfo * subtitle_info, GstMediaRepFile ** rep_file)
{
  GstDashManager *manager;
  G_GNUC_UNUSED GstCaps *caps;
  GstCaps *pad_caps;
  guint streams_count = 0;
  const gchar *mime_type, *lang = NULL;
  StreamType type;
  G_GNUC_UNUSED gint fps_n, fps_d;
  gint width = 0, height = 0, par_n = 0, par_d = 0, samplerate = 0;
  guint bitrate = 0;
  gdouble framerate = 0;
  gchar *pad_name;
  gboolean ret;

  manager = GST_DASH_MANAGER (b_manager);

  if (video_info != NULL) {
    streams_count++;
  }
  if (audio_info != NULL) {
    streams_count++;
  }
  if (subtitle_info != NULL) {
    streams_count++;
  }
  if (streams_count != 1) {
    GST_ERROR ("The stream contains %d muxed streams instead of 1",
        streams_count);
    return FALSE;
  }

  if (video_info != NULL) {
    height = gst_discoverer_video_info_get_height (video_info);
    width = gst_discoverer_video_info_get_width (video_info);
    fps_d = gst_discoverer_video_info_get_framerate_denom (video_info);
    fps_n = gst_discoverer_video_info_get_framerate_num (video_info);
    par_d = gst_discoverer_video_info_get_par_denom (video_info);
    par_n = gst_discoverer_video_info_get_par_num (video_info);
    bitrate = gst_discoverer_video_info_get_bitrate (video_info);
    caps = gst_discoverer_stream_info_get_caps (
        (GstDiscovererStreamInfo *) video_info);
    type = STREAM_TYPE_VIDEO;
  }

  if (audio_info != NULL) {
    samplerate = gst_discoverer_audio_info_get_sample_rate (audio_info);
    lang = gst_discoverer_audio_info_get_language (audio_info);
    bitrate = gst_discoverer_audio_info_get_bitrate (audio_info);
    caps = gst_discoverer_stream_info_get_caps (
        (GstDiscovererStreamInfo *) audio_info);
    type = STREAM_TYPE_AUDIO;
  }

  if (subtitle_info != NULL) {
    lang = gst_discoverer_subtitle_info_get_language (subtitle_info);
    caps = gst_discoverer_stream_info_get_caps (
        (GstDiscovererStreamInfo *) subtitle_info);
    type = STREAM_TYPE_SUBTITLE;
  }

  /* FIXME: Add audio channels */
  pad_name = gst_pad_get_name (pad);
  pad_caps = gst_pad_get_current_caps (pad);
  mime_type = gst_structure_get_name (gst_caps_get_structure (pad_caps, 0));
  ret = gst_media_presentation_add_stream (manager->mdp, type,
      pad_name, mime_type, width, height, par_n, par_d, framerate,
      NULL, samplerate, bitrate, lang, b_manager->fragment_duration);
  g_free (pad_name);
  gst_caps_unref (pad_caps);
  return ret;
}

static gboolean
gst_dash_manager_remove_stream (GstStreamsManager * b_manager, GstPad * pad,
    GstMediaRepFile ** file)
{
  /* FIXME: Not implemented */
  return FALSE;
}

static const gchar *
gst_dash_manager_get_extension (GstStreamsManager * manager, GstPad * pad)
{
  GstCaps *pad_caps;
  const gchar *mime_type;

  pad_caps = gst_pad_get_current_caps (pad);
  mime_type = gst_structure_get_name (gst_caps_get_structure (pad_caps, 0));
  gst_caps_unref (pad_caps);
  if (!g_strcmp0 (mime_type, "video/quicktime"))
    return ISOFF_FRAGMENT_EXTENSION;
  else if (!g_strcmp0 (mime_type, "video/mpegts"))
    return MPEGTS_FRAGMENT_EXTENSION;
  else
    g_assert_not_reached ();
  return NULL;
}

static gchar *
gst_dash_manager_fragment_name (GstStreamsManager * manager,
    GstPad * pad, guint index)
{
  gchar *name, *frag_filename, *pad_name;
  const gchar *extension;

  pad_name = gst_pad_get_name (pad);
  extension = gst_dash_manager_get_extension (manager, pad);
  if (manager->chunked) {
    name = g_strdup_printf ("%s_%s_%d%s", manager->title,
        manager->fragment_prefix, index, extension);
  } else {
    name = g_strdup_printf ("%s_%s%s", manager->title,
        manager->fragment_prefix, extension);
  }
  frag_filename = g_build_path (G_DIR_SEPARATOR_S, pad_name, name, NULL);
  g_free (name);
  g_free (pad_name);

  return frag_filename;
}

static gchar *
gst_dash_manager_headers_name (GstStreamsManager * manager, GstPad * pad)
{
  gchar *name, *frag_filename, *pad_name;
  const gchar *extension;

  pad_name = gst_pad_get_name (pad);
  extension = gst_dash_manager_get_extension (manager, pad);
  if (manager->chunked) {
    name = g_strdup_printf ("%s%s", manager->title, extension);
  } else {
    name = g_strdup_printf ("%s_%s%s", manager->title,
        manager->fragment_prefix, extension);
  }
  frag_filename = g_build_path (G_DIR_SEPARATOR_S, pad_name, name, NULL);
  g_free (name);
  g_free (pad_name);

  return frag_filename;
}

static gboolean
gst_dash_manager_add_headers (GstStreamsManager * b_manager,
    GstPad * pad, GstBuffer * fragment)
{
  GstDashManager *manager;
  GstFragmentMeta *meta;
  gchar *pad_name;
  gboolean ret;

  manager = GST_DASH_MANAGER (b_manager);

  pad_name = gst_pad_get_name (pad);
  meta = gst_buffer_get_fragment_meta (fragment);
  ret = gst_media_presentation_set_init_segment (manager->mdp, pad_name,
      meta->name, GST_BUFFER_OFFSET (fragment), gst_buffer_get_size (fragment));
  g_free (pad_name);

  return ret;
}

static gboolean
gst_dash_manager_add_fragment (GstStreamsManager * b_manager,
    GstPad * pad, GstBuffer * fragment, GstMediaRepFile ** rep_file,
    GList ** removed_fragments)
{
  GstDashManager *manager;
  GstFragmentMeta *meta;
  gchar *pad_name;
  gboolean ret;

  manager = GST_DASH_MANAGER (b_manager);
  pad_name = gst_pad_get_name (pad);

  /* FIXME: handled rotating windows */
  *removed_fragments = NULL;
  if (!gst_dash_manager_render (b_manager, pad, rep_file)) {
    return FALSE;
  }

  meta = gst_buffer_get_fragment_meta (fragment);
  ret = gst_media_presentation_add_media_segment (manager->mdp,
      pad_name, meta->name, meta->index, GST_BUFFER_PTS (fragment),
      GST_BUFFER_DURATION (fragment), GST_BUFFER_OFFSET (fragment),
      gst_buffer_get_size (fragment));
  g_free (pad_name);

  return ret;
}

static void
gst_dash_manager_clear (GstStreamsManager * b_manager)
{
  GstDashManager *manager;

  manager = GST_DASH_MANAGER (b_manager);
  if (manager->mdp != NULL) {
    gst_media_presentation_free (manager->mdp);
  }

  manager->mdp = gst_media_presentation_new (MEDIA_PRESENTATION_TYPE_ONDEMAND,
      b_manager->is_live, manager->min_buffer_time);
}

static gboolean
gst_dash_manager_render (GstStreamsManager * b_manager, GstPad * pad,
    GstMediaRepFile ** rep_file)
{
  GstDashManager *manager;
  gchar *filename, *filepath, *mdp;
  GFile *file;
  GList *base_urls = NULL;
  gboolean ret = TRUE;

  manager = GST_DASH_MANAGER (b_manager);

  /* Update base url */
  base_urls = g_list_append (base_urls, g_strdup (b_manager->base_url));
  gst_media_presentation_set_base_urls (manager->mdp, base_urls);

  filename = g_strdup_printf ("%s%s", b_manager->title, MDP_EXTENSION);
  filepath = g_build_filename (b_manager->output_directory, filename, NULL);
  file = g_file_new_for_path (filepath);
  mdp = gst_media_presentation_render (manager->mdp);
  if (mdp != NULL) {
    *rep_file =
        gst_media_rep_file_new (gst_media_presentation_render (manager->mdp),
        file);
  } else {
    ret = FALSE;
  }
  g_free (filename);
  g_free (filepath);
  g_object_unref (file);

  return ret;
}
