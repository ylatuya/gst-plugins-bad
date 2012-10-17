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

#define M3U8_HEADER_TAG "#EXTM3U\n"
#define M3U8_VERSION_TAG "#EXT-X-VERSION:%d\n"
#define M3U8_TARGETDURATION_TAG "#EXT-X-TARGETDURATION:%d\n"
#define M3U8_MEDIA_SEQUENCE_TAG "#EXT-X-MEDIA-SEQUENCE:%d\n"
#define M3U8_DISCONTINUITY_TAG "#EXT-X-DISCONTINUITY\n"
#define M3U8_INT_INF_TAG "#EXTINF:%d,%s\n"
#define M3U8_FLOAT_INF_TAG "#EXTINF:%.2f,%s\n"
#define M3U8_ENDLIST_TAG "#EXT-X-ENDLIST"
#define M3U8_VARIANT_TAG "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%d\n"
#define M3U8_BYTERANGE_TAG "#EXT-X-BYTERANGE:%d@%d\n"

enum
{
  GST_M3U8_PLAYLIST_TYPE_EVENT,
  GST_M3U8_PLAYLIST_TYPE_VOD,
};

static GstM3U8Entry *
gst_m3u8_entry_new (gchar * url, GFile * file, gchar * title,
    gfloat duration, guint length, guint offset, gboolean discontinuous)
{
  GstM3U8Entry *entry;

  g_return_val_if_fail (url != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  entry = g_new0 (GstM3U8Entry, 1);
  entry->url = g_strdup (url);
  entry->file = file;
  entry->title = g_strdup (title);
  entry->duration = duration;
  entry->discontinuous = discontinuous;
  entry->length = length;
  entry->offset = offset;
  g_object_ref (entry->file);
  return entry;
}

static void
gst_m3u8_entry_free (GstM3U8Entry * entry)
{
  g_return_if_fail (entry != NULL);

  if (entry->url != NULL) {
    g_free (entry->url);
    entry->url = NULL;
  }

  if (entry->title != NULL) {
    g_free (entry->title);
    entry->title = NULL;
  }

  if (entry->file != NULL) {
    g_object_unref (entry->file);
    entry->file = NULL;
  }
}

static gchar *
gst_m3u8_entry_render (GstM3U8Entry * entry, guint version,
    gboolean use_byterange)
{
  const gchar *discont;
  gchar *byterange;
  gchar *ret;

  g_return_val_if_fail (entry != NULL, NULL);

  discont = entry->discontinuous ? M3U8_DISCONTINUITY_TAG : "";

  if (use_byterange) {
    byterange = g_strdup_printf (M3U8_BYTERANGE_TAG, entry->length,
        entry->offset);
  } else {
    byterange = g_strdup ("");
  }

  /* FIXME: Ensure the radix is always a '.' and not a ',' when printing
   * floating point number, but for now only use integers*/
  /* if (version < 3) */
  if (TRUE) {
    ret = g_strdup_printf ("%s" M3U8_INT_INF_TAG "%s%s\n", discont,
        (gint) entry->duration, entry->title, byterange, entry->url);
  } else {
    ret = g_strdup_printf ("%s" M3U8_FLOAT_INF_TAG "%s%s\n", discont,
        entry->duration, entry->title, byterange, entry->url);
  }

  g_free (byterange);
  return ret;
}

GstM3U8Playlist *
gst_m3u8_playlist_new (gchar * name, gchar * base_url, GFile * file,
    gint bitrate, guint version, guint window_size, gboolean allow_cache,
    gboolean chunked)
{
  GstM3U8Playlist *playlist;

  playlist = g_new0 (GstM3U8Playlist, 1);
  playlist->name = g_strdup (name);
  playlist->base_url = g_strdup (base_url);
  playlist->bitrate = bitrate;
  playlist->version = version;
  playlist->window_size = window_size;
  playlist->allow_cache = allow_cache;
  playlist->type = GST_M3U8_PLAYLIST_TYPE_EVENT;
  playlist->end_list = FALSE;
  playlist->entries = g_queue_new ();
  g_object_ref (file);
  playlist->file = file;

  if (!chunked && version < 7) {
    GST_WARNING ("Byteranges media segments are not support for versions < 7");
    chunked = TRUE;
  }
  playlist->chunked = chunked;

  return playlist;
}

void
gst_m3u8_playlist_free (GstM3U8Playlist * playlist)
{
  g_return_if_fail (playlist != NULL);

  g_queue_foreach (playlist->entries, (GFunc) gst_m3u8_entry_free, NULL);
  g_queue_free (playlist->entries);

  if (playlist->name != NULL) {
    g_free (playlist->name);
    playlist->name = NULL;
  }

  if (playlist->base_url != NULL) {
    g_free (playlist->base_url);
    playlist->base_url = NULL;
  }

  if (playlist->file != NULL) {
    g_object_unref (playlist->file);
    playlist->file = NULL;
  }

  g_free (playlist);
}

static guint
gst_m3u8_playlist_target_duration (GstM3U8Playlist * playlist)
{
  gint i;
  GstM3U8Entry *entry;
  gfloat target_duration = 0;

  for (i = 0; i < playlist->entries->length; i++) {
    entry = (GstM3U8Entry *) g_queue_peek_nth (playlist->entries, i);
    if (entry->duration > target_duration)
      target_duration = entry->duration;
  }

  return target_duration;
}

GList *
gst_m3u8_playlist_add_entry (GstM3U8Playlist * playlist,
    gchar * url, GFile * file, gchar * title,
    gfloat duration, guint length, guint offset, guint index,
    gboolean discontinuous)
{
  GstM3U8Entry *entry;
  GList *old_files = NULL;

  g_return_val_if_fail (playlist != NULL, FALSE);
  g_return_val_if_fail (url != NULL, FALSE);
  g_return_val_if_fail (title != NULL, FALSE);

  if (playlist->type == GST_M3U8_PLAYLIST_TYPE_VOD)
    return FALSE;

  entry = gst_m3u8_entry_new (url, file, title, duration, length, offset,
      discontinuous);

  if (playlist->window_size != 0) {
    /* Delete old entries from the playlist */
    while (gst_m3u8_playlist_target_duration (playlist) >=
        playlist->window_size) {
      GstM3U8Entry *old_entry;

      old_entry = g_queue_pop_head (playlist->entries);
      g_object_ref (old_entry->file);
      old_files = g_list_prepend (old_files, old_entry->file);
      gst_m3u8_entry_free (old_entry);
    }
  }

  playlist->sequence_number = index + 1;
  g_queue_push_tail (playlist->entries, entry);

  return old_files;
}

static void
render_entry (GstM3U8Entry * entry, GstM3U8Playlist * playlist)
{
  gchar *entry_str;

  entry_str = gst_m3u8_entry_render (entry, playlist->version,
      !playlist->chunked);
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
//  g_string_append_printf (playlist->playlist_str, M3U8_VERSION_TAG,
//      playlist->version);
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

void
gst_m3u8_playlist_clear (GstM3U8Playlist * playlist)
{
  g_return_if_fail (playlist != NULL);

  g_queue_foreach (playlist->entries, (GFunc) gst_m3u8_entry_free, NULL);
  g_queue_clear (playlist->entries);
}

guint
gst_m3u8_playlist_n_entries (GstM3U8Playlist * playlist)
{
  g_return_val_if_fail (playlist != NULL, 0);

  return playlist->entries->length;
}

GstM3U8VariantPlaylist *
gst_m3u8_variant_playlist_new (gchar * name, gchar * base_url, GFile * file)
{
  GstM3U8VariantPlaylist *playlist;

  playlist = g_new0 (GstM3U8VariantPlaylist, 1);
  playlist->variants = g_hash_table_new (g_str_hash, g_str_equal);
  playlist->playlist_str = NULL;
  playlist->name = g_strdup (name);
  playlist->base_url = g_strdup (base_url);
  playlist->file = file;
  g_object_ref (playlist->file);;
  return playlist;
}

void
gst_m3u8_variant_playlist_free (GstM3U8VariantPlaylist * playlist)
{
  if (playlist->variants != NULL) {
    g_hash_table_destroy (playlist->variants);
    playlist->variants = NULL;
  }

  if (playlist->playlist_str != NULL) {
    g_string_free (playlist->playlist_str, TRUE);
    playlist->playlist_str = NULL;
  }

  if (playlist->name != NULL) {
    g_free (playlist->name);
    playlist->name = NULL;
  }

  if (playlist->base_url != NULL) {
    g_free (playlist->base_url);
    playlist->base_url = NULL;
  }

  if (playlist->file != NULL) {
    g_object_unref (playlist->file);
    playlist->file = NULL;
  }

  g_free (playlist);
}

static void
render_variant (gchar * key, GstM3U8Playlist * playlist, GString * string)
{
  g_string_append_printf (string, M3U8_VARIANT_TAG, playlist->bitrate);
  g_string_append_printf (string, "%s/%s.m3u8\n", playlist->base_url,
      playlist->name);
}

static void
gst_m3u8_variant_playlist_update (GstM3U8VariantPlaylist * playlist)
{
  if (playlist->playlist_str)
    g_string_free (playlist->playlist_str, TRUE);

  playlist->playlist_str = g_string_new ("");

  /* #EXTM3U */
  g_string_append_printf (playlist->playlist_str, M3U8_HEADER_TAG);
  /* Variants */
  g_hash_table_foreach (playlist->variants, (GHFunc) render_variant,
      playlist->playlist_str);
}

gboolean
gst_m3u8_variant_playlist_add_variant (GstM3U8VariantPlaylist * playlist,
    GstM3U8Playlist * variant)
{
  g_return_val_if_fail (playlist != NULL, FALSE);
  g_return_val_if_fail (variant != NULL, FALSE);

  if (g_hash_table_lookup (playlist->variants, variant->name)) {
    return FALSE;
  }
  g_hash_table_insert (playlist->variants, variant->name, variant);
  gst_m3u8_variant_playlist_update (playlist);
  return TRUE;
}

GstM3U8Playlist *
gst_m3u8_variant_playlist_get_variant (GstM3U8VariantPlaylist * playlist,
    gchar * name)
{
  g_return_val_if_fail (playlist != NULL, NULL);

  return g_hash_table_lookup (playlist->variants, name);
}

gboolean
gst_m3u8_variant_playlist_remove_variant (GstM3U8VariantPlaylist * playlist,
    gchar * name, gboolean free)
{
  GstM3U8Playlist *variant;

  g_return_val_if_fail (playlist != NULL, FALSE);

  variant = gst_m3u8_variant_playlist_get_variant (playlist, name);
  if (variant)
    return FALSE;

  g_hash_table_remove (playlist->variants, name);
  if (free)
    gst_m3u8_playlist_free (variant);
  gst_m3u8_variant_playlist_update (playlist);

  return TRUE;
}

gchar *
gst_m3u8_variant_playlist_render (GstM3U8VariantPlaylist * playlist)
{
  g_return_val_if_fail (playlist != NULL, NULL);

  return g_strdup (playlist->playlist_str->str);
}
