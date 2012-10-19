/* GStreamer
 * Copyright (C) 2012 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstm3u8manager.c:
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

#include "gstm3u8manager.h"

G_DEFINE_TYPE (GstM3u8Manager, gst_m3u8_manager, GST_TYPE_STREAMS_MANAGER);

static void gst_m3u8_manager_finalize (GObject * gobject);

static GFile *
gst_m3u8_manager_create_playlist_file (GstM3u8Manager * manager,
    const gchar * name)
{
  gchar *filename, *path;
  GFile *file;

  filename = g_strdup_printf ("%s.m3u8", name);
  path = g_build_filename (GST_STREAMS_MANAGER (manager)->output_directory,
      filename, NULL);
  file = g_file_new_for_path (path);
  g_free (filename);
  g_free (path);

  return file;
}

GstM3u8Manager *
gst_m3u8_manager_new (gchar * base_url, gchar * title, gchar * fragment_prefix,
    gchar * output_directory, gboolean is_live, gboolean chunked,
    GstClockTime window_size, gchar * variant_pl_name, guint max_version,
    gboolean allow_cache)
{
  GstM3u8Manager *manager;
  GstStreamsManager *bmanager;
  GFile *file;

  manager = GST_M3U8_MANAGER (g_object_new (GST_TYPE_M3U8_MANAGER, NULL));
  bmanager = GST_STREAMS_MANAGER (manager);

  /* Set base class properties */
  gst_streams_manager_set_output_directory (bmanager, output_directory);
  gst_streams_manager_set_base_url (bmanager, base_url);
  gst_streams_manager_set_title (bmanager, title);
  gst_streams_manager_set_fragment_prefix (bmanager, fragment_prefix);
  file = gst_m3u8_manager_create_playlist_file (manager, variant_pl_name);
  manager->variant_playlist =
      gst_m3u8_variant_playlist_new (variant_pl_name, base_url, file);
  bmanager->window_size = window_size;
  bmanager->is_live = is_live;
  bmanager->chunked = chunked;

  /* Set the subclass properties */
  manager->max_version = max_version;
  manager->allow_cache = allow_cache;

  return manager;
}

static gboolean
gst_m3u8_manager_render (GstStreamsManager * bmanager, GstPad * pad,
    GstMediaRepFile ** rep_file)
{
  GstM3u8Manager *manager;
  GstM3U8Playlist *playlist;

  manager = GST_M3U8_MANAGER (bmanager);

  playlist = gst_m3u8_variant_playlist_get_variant (manager->variant_playlist,
      gst_pad_get_name (pad));
  if (!playlist)
    return FALSE;

  *rep_file =
      gst_media_rep_file_new (gst_m3u8_playlist_render (playlist),
      playlist->file);

  return TRUE;
}

static gboolean
gst_m3u8_manager_add_stream (GstStreamsManager * bmanager, GstPad * pad,
    guint avg_bitrate, GList * substreams_caps, GstMediaRepFile ** rep_file)
{
  GstM3u8Manager *manager;
  GstM3U8Playlist *playlist;
  gchar *stream_name;
  GFile *file;

  manager = GST_M3U8_MANAGER (bmanager);

  stream_name = gst_pad_get_name (pad);

  file = gst_m3u8_manager_create_playlist_file (manager, stream_name);

  playlist = gst_m3u8_playlist_new (stream_name, bmanager->base_url, file,
      avg_bitrate, manager->max_version, bmanager->window_size / GST_SECOND,
      manager->allow_cache, bmanager->chunked);
  g_object_unref (file);

  if (!gst_m3u8_variant_playlist_add_variant (manager->variant_playlist,
          playlist)) {
    GST_ERROR_OBJECT (manager, "Could not add variant");
    gst_m3u8_playlist_free (playlist);
    return FALSE;
  }

  *rep_file =
      gst_media_rep_file_new (gst_m3u8_variant_playlist_render
      (manager->variant_playlist), manager->variant_playlist->file);
  return TRUE;
}

static gboolean
gst_m3u8_manager_add_fragment (GstStreamsManager * bmanager,
    GstPad * pad, GstBuffer * fragment,
    GstMediaRepFile ** rep_file, GList ** removed_fragments)
{
  GstM3u8Manager *manager;
  GstM3U8Playlist *playlist;
  GstFragmentMeta *meta;
  GstClockTime duration;
  gboolean ret = TRUE;

  manager = GST_M3U8_MANAGER (bmanager);

  playlist = gst_m3u8_variant_playlist_get_variant (manager->variant_playlist,
      gst_pad_get_name (pad));
  if (playlist == NULL) {
    *removed_fragments = NULL;
    *rep_file = NULL;
    return FALSE;
  }

  meta = gst_buffer_get_fragment_meta (fragment);
  duration = gst_fragment_get_duration (fragment);

  *removed_fragments =
      gst_m3u8_playlist_add_entry (playlist, meta->name, meta->file,
      bmanager->title, ((gfloat) duration) / GST_SECOND,
      gst_buffer_get_size (fragment), meta->offset, meta->index,
      meta->discontinuous);
  *rep_file =
      gst_media_rep_file_new (gst_m3u8_playlist_render (playlist),
      playlist->file);
  return ret;
}

static gboolean
gst_m3u8_manager_remove_stream (GstStreamsManager * bmanager, GstPad * pad,
    GstMediaRepFile ** rep_file)
{
  GstM3u8Manager *manager = GST_M3U8_MANAGER (bmanager);
  gboolean ret;

  ret = gst_m3u8_variant_playlist_remove_variant (manager->variant_playlist,
      gst_pad_get_name (pad));

  *rep_file =
      gst_media_rep_file_new (gst_m3u8_variant_playlist_render
      (manager->variant_playlist), manager->variant_playlist->file);
  return ret;
}

static gboolean
gst_m3u8_manager_eos (GstStreamsManager * bmanager, GstPad * pad,
    GstMediaRepFile ** rep_file)
{
  GstM3U8Playlist *playlist;
  GstM3u8Manager *manager = GST_M3U8_MANAGER (bmanager);

  playlist = gst_m3u8_variant_playlist_get_variant (manager->variant_playlist,
      gst_pad_get_name (pad));
  if (playlist == NULL) {
    *rep_file = NULL;
    return FALSE;
  }

  playlist->end_list = TRUE;
  *rep_file =
      gst_media_rep_file_new (gst_m3u8_playlist_render (playlist),
      playlist->file);
  return TRUE;
}

static void
remove_stream (gchar * name, GstM3U8Playlist * playlist,
    GstM3u8Manager * manager)
{
  gst_m3u8_variant_playlist_remove_variant (manager->variant_playlist, name);
}

static void
gst_m3u8_manager_clear (GstStreamsManager * bmanager)
{
  GstM3u8Manager *manager = GST_M3U8_MANAGER (bmanager);

  g_hash_table_foreach (manager->variant_playlist->variants,
      (GHFunc) remove_stream, manager);
}

static void
gst_m3u8_manager_finalize (GObject * gobject)
{
  GstM3u8Manager *manager = GST_M3U8_MANAGER (gobject);

  gst_m3u8_manager_clear (GST_STREAMS_MANAGER (manager));

  if (manager->variant_playlist != NULL) {
    gst_m3u8_variant_playlist_free (manager->variant_playlist);
    manager->variant_playlist = NULL;
  }

  G_OBJECT_CLASS (gst_m3u8_manager_parent_class)->finalize (gobject);
}

static void
gst_m3u8_manager_class_init (GstM3u8ManagerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstStreamsManagerClass *manager_class = GST_STREAMS_MANAGER_CLASS (klass);

  gobject_class->finalize = gst_m3u8_manager_finalize;
  manager_class->add_stream = gst_m3u8_manager_add_stream;
  manager_class->remove_stream = gst_m3u8_manager_remove_stream;
  manager_class->add_fragment = gst_m3u8_manager_add_fragment;
  manager_class->eos = gst_m3u8_manager_eos;
  manager_class->render = gst_m3u8_manager_render;
  manager_class->clear = gst_m3u8_manager_clear;
}

static void
gst_m3u8_manager_init (GstM3u8Manager * manager)
{
  manager->max_version = 3;
  manager->allow_cache = FALSE;
}
