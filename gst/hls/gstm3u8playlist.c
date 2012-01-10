/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstm3u8playlist.c:
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

#include <glib.h>

#include "gstfragmented.h"
#include "gstm3u8playlist.h"

#define GST_CAT_DEFAULT fragmented_debug

#define DEFAULT_PLAYLIST_VERSION 1
#define DEFAULT_WINDOW_SIZE -1
#define DEFAULT_ALLOW_CACHE FALSE

#define M3U8_HEADER_TAG "#EXTM3U\n"
#define M3U8_VERSION_TAG "#EXT-X-VERSION:%d\n"
#define M3U8_TARGETDURATION_TAG "#EXT-X-TARGETDURATION:%d\n"
#define M3U8_MEDIA_SEQUENCE_TAG "#EXT-X-MEDIA-SEQUENCE:%d\n"
#define M3U8_DISCONTINUITY_TAG "#EXT-X-DISCONTINUITY\n"
#define M3U8_INT_INF_TAG "#EXTINF:%d,%s\n%s\n"
#define M3U8_FLOAT_INF_TAG "#EXTINF:%.2f,%s\n%s\n"
#define M3U8_ENDLIST_TAG "#EXT-X-ENDLIST"

enum
{
  GST_M3U8_PLAYLIST_TYPE_EVENT,
  GST_M3U8_PLAYLIST_TYPE_VOD,
};

G_DEFINE_TYPE (GstM3U8Playlist, gst_m3u8_playlist, G_TYPE_OBJECT);

static void gst_m3u8_playlist_finalize (GObject * object);
static void gst_m3u8_playlist_render (GstM3U8Playlist * object);

static void
gst_m3u8_playlist_class_init (GstM3U8PlaylistClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBasePlaylistClass *playlist_class = GST_BASE_PLAYLIST_CLASS (klass);

  gobject_class->finalize = gst_m3u8_playlist_finalize;

  playlist_class->render = gst_m3u8_playlist_render;
}

static void
gst_m3u8_playlist_init (GstM3U8Playlist * playlist)
{
  playlist->version = DEFAULT_PLAYLIST_VERSION;
  playlist->window_size = DEFAULT_WINDOW_SIZE;
  playlist->allow_cache = DEFAULT_ALLOW_CACHE;
  playlist->type = GST_M3U8_PLAYLIST_TYPE_EVENT;
}

GstM3U8Playlist *
gst_m3u8_playlist_new (guint version, guint window_size, gboolean allow_cache)
{
  GstM3U8Playlist *playlist;

  playlist = GST_M3U8_PLAYLIST (g_object_new (GST_TYPE_BASE_PLAYLIST, NULL));
  playlist->version = version;
  playlist->window_size = window_size;
  playlist->allow_cache = allow_cache;

  return playlist;
}

static const gchar *
gst_m3u8_entry_render_with_version (GstEntry * entry, guint version)
{
  g_return_val_if_fail (entry != NULL, NULL);

  /* FIXME: Ensure the radix is always a '.' and not a ',' when printing
   * floating point number, but for now only use integers*/
  /* if (version < 3) */
  if (TRUE)
    return g_strdup_printf ("%s" M3U8_INT_INF_TAG,
        entry->discontinuous ? M3U8_DISCONTINUITY_TAG : "",
        (gint) (entry->duration / GST_SECOND), entry->title, entry->url);

  return g_strdup_printf ("%s" M3U8_FLOAT_INF_TAG,
      entry->discontinuous ? M3U8_DISCONTINUITY_TAG : "",
      (entry->duration / GST_SECOND), entry->title, entry->url);
}


static guint
gst_m3u8_playlist_target_duration (GstM3U8Playlist * playlist)
{
  gint i;
  GstM3U8Entry *entry;
  guint64 target_duration = 0;

  for (i = 0; i < playlist->entries->length; i++) {
    entry = (GstM3U8Entry *) g_queue_peek_nth (playlist->entries, i);
    if (entry->duration > target_duration)
      target_duration = entry->duration;
  }

  return (guint) (target_duration / GST_SECOND);
}

static void
render_entry (GstM3U8Entry * entry, GstM3U8Playlist * playlist)
{
  gchar *entry_str;

  entry_str = gst_m3u8_entry_render (entry, playlist->version);
  g_string_append_printf (playlist->playlist_str, "%s", entry_str);
  g_free (entry_str);
}

gchar *
gst_m3u8_playlist_render (GstM3U8Playlist * playlist)
{
  gchar *pl;

  g_return_val_if_fail (playlist != NULL, NULL);

  playlist->playlist_str = g_string_new ("");

  /* #EXTM3U */
  g_string_append_printf (playlist->playlist_str, M3U8_HEADER_TAG);
  /* #EXT-X-VERSION */
  g_string_append_printf (playlist->playlist_str, M3U8_VERSION_TAG,
      playlist->version);
  /* #EXT-X-MEDIA-SEQUENCE */
  g_string_append_printf (playlist->playlist_str, M3U8_MEDIA_SEQUENCE_TAG,
      playlist->sequence_number - playlist->entries->length);
  /* #EXT-X-TARGETDURATION */
  g_string_append_printf (playlist->playlist_str, M3U8_TARGETDURATION_TAG,
      gst_m3u8_playlist_target_duration (playlist));
  g_string_append_printf (playlist->playlist_str, "\n");

  /* Entries */
  g_queue_foreach (playlist->entries, (GFunc) render_entry, playlist);

  if (playlist->end_list)
    g_string_append_printf (playlist->playlist_str, M3U8_ENDLIST_TAG);

  pl = playlist->playlist_str->str;
  g_string_free (playlist->playlist_str, FALSE);
  return pl;
}

guint
gst_m3u8_playlist_n_entries (GstM3U8Playlist * playlist)
{
  g_return_val_if_fail (playlist != NULL, 0);

  return playlist->entries->length;
}
