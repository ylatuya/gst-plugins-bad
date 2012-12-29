/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
 *   Authors: Andoni Morales Alastruey <amorales@fluendo.com>
 *
 * m3u8.c:
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

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <glib.h>

#include "gstfragmented.h"
#include "m3u8.h"

#define GST_CAT_DEFAULT fragmented_debug

#define HLS_CODEC_AUDIO "mp4a"
#define HLS_CODEC_AAC_LC "mp4a.40.2"
#define HLS_CODEC_HE_AAC "mp4a.40.5"
#define HLS_CODEC_MP3 "mp4a.40.34"
#define HLS_CODEC_H264 "avc1"
#define HLS_CODEC_H264_BASE "avc1.42e0"
#define HLS_CODEC_H264_MAIN "avc1.4d40"
#define HLS_CODEC_H264_HIGH "avc1.6400"

static void gst_m3u8_variant_playlist_free (GstM3U8VariantPlaylist * m3u8);
static gboolean gst_m3u8_playlist_update (GstM3U8Playlist * m3u8, gchar * data,
    gboolean * updated);
static GstM3U8MediaFile *gst_m3u8_media_file_new (gchar * uri,
    gchar * title, GstClockTime duration, guint sequence, gint64 offset,
    gint64 length);
static void gst_m3u8_media_file_free (GstM3U8MediaFile * self);

static gboolean
_find_next (GstM3U8MediaFile * file, gconstpointer ptr)
{
  guint sequence = GPOINTER_TO_UINT (ptr);
  if (file->sequence >= sequence)
    return FALSE;
  GST_DEBUG ("Found fragment %d", file->sequence);
  return TRUE;
}

/*****************
 *    GstM3u8    *
 ****************/

static void
gst_m3u8_free (GstM3U8 * self)
{
  if (self->uri != NULL)
    g_free (self->uri);
}

static void
gst_m3u8_set_uri (GstM3U8 * self, gchar * uri)
{
  g_return_if_fail (self != NULL);

  if (self->uri)
    g_free (self->uri);
  self->uri = uri;
}

/**********************
 *    GstM3u8Media    *
 *********************/

static GstM3U8Media *
gst_m3u8_media_new (void)
{
  GstM3U8Media *media;

  media = g_new0 (GstM3U8Media, 1);
  media->media_type = GST_M3U8_MEDIA_TYPE_VIDEO;
  media->name = NULL;
  media->language = NULL;
  media->is_default = TRUE;
  media->autoselect = FALSE;
  media->playlist = NULL;
  return media;
}

static void
gst_m3u8_media_free (GstM3U8Media * media)
{
  if (media->group_id != NULL)
    g_free (media->group_id);

  if (media->name != NULL)
    g_free (media->name);

  if (media->language != NULL)
    g_free (media->language);

  if (media->uri != NULL)
    g_free (media->uri);
}

/***********************
 *    GstM3u8Stream    *
 **********************/

static GstM3U8Stream *
gst_m3u8_stream_new (void)
{
  GstM3U8Stream *stream;

  stream = g_new0 (GstM3U8Stream, 1);
  stream->first = FALSE;
  stream->video_alternates = g_hash_table_new (g_str_hash, g_str_equal);
  stream->audio_alternates = g_hash_table_new (g_str_hash, g_str_equal);
  stream->subtt_alternates = g_hash_table_new (g_str_hash, g_str_equal);
  stream->selected_video = NULL;
  stream->selected_audio = NULL;
  stream->selected_subtt = NULL;
  stream->i_frame = NULL;
  stream->video_codec = GST_M3U8_MEDIA_CODEC_NONE;
  stream->audio_codec = GST_M3U8_MEDIA_CODEC_NONE;
  stream->init_segment = NULL;
  return stream;
}

static void
gst_m3u8_stream_free (GstM3U8Stream * stream)
{
  if (stream->default_video != NULL)
    g_free (stream->default_video);

  if (stream->default_audio != NULL)
    g_free (stream->default_audio);

  if (stream->default_subtt != NULL)
    g_free (stream->default_subtt);

  if (stream->init_segment != NULL)
    gst_m3u8_media_file_free (stream->init_segment);

  g_hash_table_unref (stream->video_alternates);
  g_hash_table_unref (stream->audio_alternates);
  g_hash_table_unref (stream->subtt_alternates);

  g_free (stream);
}

static gboolean
gst_m3u8_stream_is_audio_video (GstM3U8Stream * stream)
{
  return (stream->video_codec != GST_M3U8_MEDIA_CODEC_NONE &&
      stream->audio_codec != GST_M3U8_MEDIA_CODEC_NONE) ||
      (stream->video_codec == GST_M3U8_MEDIA_CODEC_NONE &&
      stream->audio_codec == GST_M3U8_MEDIA_CODEC_NONE);
}

static gboolean
gst_m3u8_stream_is_audio_only (GstM3U8Stream * stream)
{
  return stream->video_codec == GST_M3U8_MEDIA_CODEC_NONE &&
      stream->audio_codec != GST_M3U8_MEDIA_CODEC_NONE;
}

static gboolean
gst_m3u8_stream_is_video_only (GstM3U8Stream * stream)
{
  return stream->audio_codec == GST_M3U8_MEDIA_CODEC_NONE &&
      stream->video_codec != GST_M3U8_MEDIA_CODEC_NONE;
}

static GstM3U8MediaFile *
gst_m3u8_stream_find_next (GstM3U8Stream * stream, guint sequence,
    GstClockTime end_position, GstM3U8MediaType media_type)
{
  GstM3U8Playlist *pl;
  GList *list;

  /* Subtitles might have a different target duration, so we must find
   * the current element based on the position and not the client sequence
   */
  if (media_type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
    GList *walk;
    GstClockTime pos = 0;

    pl = stream->selected_subtt;
    if (pl == NULL)
      return NULL;

    /* For a live playlist we must guess the first timestamp from the media
     * sequence and the target duration */
    if (!pl->endlist) {
      guint first_seq = pl->mediasequence - g_list_length (pl->files);
      pos = (GstClockTime) (first_seq * pl->targetduration);
    }
    if (pos > end_position)
      return NULL;

    for (walk = pl->files; walk; walk = walk->next) {
      GstM3U8MediaFile *media = GST_M3U8_MEDIA_FILE (walk->data);
      if ((pos + media->duration) > end_position)
        return media;
      pos += media->duration;
    }
    return NULL;
  }

  if (media_type == GST_M3U8_MEDIA_TYPE_VIDEO)
    pl = stream->selected_video;
  else if (media_type == GST_M3U8_MEDIA_TYPE_AUDIO)
    pl = stream->selected_audio;
  else
    return NULL;

  if (pl == NULL)
    return NULL;

  list = g_list_find_custom (pl->files, GUINT_TO_POINTER (sequence),
      (GCompareFunc) _find_next);
  if (list == NULL) {
    return NULL;
  }
  return GST_M3U8_MEDIA_FILE (list->data);
}

/***********************
 *    GstM3u8Playlist  *
 **********************/

static GstM3U8Playlist *
gst_m3u8_playlist_new (void)
{
  GstM3U8Playlist *m3u8;

  m3u8 = g_new0 (GstM3U8Playlist, 1);
  m3u8->files = NULL;
  m3u8->allowcache = NULL;
  m3u8->last_data = NULL;
  GST_M3U8 (m3u8)->uri = NULL;

  return m3u8;
}

static void
gst_m3u8_playlist_free (GstM3U8Playlist * self)
{
  if (self == NULL)
    return;

  gst_m3u8_free (GST_M3U8 (self));

  if (self->allowcache)
    g_free (self->allowcache);

  if (self->files != NULL) {
    g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
    g_list_free (self->files);
  }

  if (self->last_data != NULL)
    g_free (self->last_data);

  g_free (self);
}

static guint64
gst_m3u8_playlist_get_position (GstM3U8Playlist * pl, guint sequence)
{
  GList *l, *walk;
  guint64 timestamp = 0;

  l = g_list_find_custom (pl->files, GUINT_TO_POINTER (sequence),
      (GCompareFunc) _find_next);

  /* For a live playlist we must guess the first timestamp from the media
   * sequence and the target duration */
  if (!pl->endlist) {
    guint first_seq = pl->mediasequence - g_list_length (pl->files);
    timestamp = (GstClockTime) (first_seq * pl->targetduration);
  }

  for (walk = pl->files; walk; walk = walk->next) {
    if (walk == l)
      break;
    timestamp += GST_M3U8_MEDIA_FILE (walk->data)->duration;
  }
  return timestamp;
}

/******************************
 *    GstM3u8VariantPlaylist  *
 *****************************/

static GstM3U8VariantPlaylist *
gst_m3u8_variant_playlist_new (void)
{
  GstM3U8VariantPlaylist *m3u8;

  m3u8 = g_new0 (GstM3U8VariantPlaylist, 1);
  m3u8->streams = NULL;
  m3u8->video_rendition_groups = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, NULL);
  m3u8->audio_rendition_groups = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, NULL);
  m3u8->subtt_rendition_groups = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, NULL);
  GST_M3U8 (m3u8)->uri = NULL;

  return m3u8;
}

static void
_free_medias (gchar * key, GList * medias, gpointer data)
{
  g_list_foreach (medias, (GFunc) gst_m3u8_media_free, NULL);
}

static void
gst_m3u8_variant_playlist_free (GstM3U8VariantPlaylist * self)
{
  if (self == NULL)
    return;

  gst_m3u8_free (GST_M3U8 (self));

  if (self->streams != NULL) {
    g_list_foreach (self->streams, (GFunc) gst_m3u8_stream_free, NULL);
    g_list_free (self->streams);
  }

  g_hash_table_foreach (self->video_rendition_groups, (GHFunc) _free_medias,
      NULL);
  g_hash_table_unref (self->video_rendition_groups);

  g_hash_table_foreach (self->audio_rendition_groups, (GHFunc) _free_medias,
      NULL);
  g_hash_table_unref (self->audio_rendition_groups);

  g_hash_table_foreach (self->subtt_rendition_groups, (GHFunc) _free_medias,
      NULL);
  g_hash_table_unref (self->subtt_rendition_groups);

  if (self->playlists != NULL) {
    g_list_foreach (self->playlists, (GFunc) gst_m3u8_playlist_free, NULL);
    g_list_free (self->playlists);
  }
  g_free (self);
}

/******************************
 *    GstM3u8MediaFile        *
 *****************************/

static GstM3U8MediaFile *
gst_m3u8_media_file_new (gchar * uri, gchar * title,
    GstClockTime duration, guint sequence, gint64 offset, gint64 length)
{
  GstM3U8MediaFile *file;

  file = g_new0 (GstM3U8MediaFile, 1);
  file->uri = uri;
  file->title = title;
  file->duration = duration;
  file->sequence = sequence;
  file->offset = offset;
  file->length = length;
  return file;
}

static void
gst_m3u8_media_file_free (GstM3U8MediaFile * self)
{
  g_return_if_fail (self != NULL);

  g_free (self->title);
  g_free (self->uri);
  g_free (self);
}

static GstFragment *
gst_m3u8_media_file_get_fragment (GstM3U8MediaFile * file, GstClockTime ts,
    gboolean discontinuous)
{
  GstFragment *frag = gst_fragment_new ();
  frag->discontinuous = discontinuous;
  g_free (frag->name);
  frag->name = g_strdup (file->uri);
  frag->start_time = ts;
  frag->stop_time = ts + file->duration;
  frag->offset = file->offset;
  frag->length = file->length;
  return frag;
}


static gboolean
bool_from_string (gchar * ptr, gboolean * val)
{
  if (g_str_equal (ptr, "NO")) {
    *val = FALSE;
  } else if (g_str_equal (ptr, "YES")) {
    *val = TRUE;
  } else {
    return FALSE;
  }
  return TRUE;
}

static gboolean
uint64_from_string (gchar * ptr, gchar ** endptr, guint64 * val)
{
  gchar *priv_endptr;

  *val = g_ascii_strtoull (ptr, &priv_endptr, 0);

  if (endptr)
    *endptr = priv_endptr;

  return ptr != priv_endptr;
}

static gboolean
int64_from_string (gchar * ptr, gchar ** endptr, gint64 * val)
{
  gchar *priv_endptr;

  *val = g_ascii_strtoll (ptr, &priv_endptr, 0);

  if (endptr)
    *endptr = priv_endptr;

  return ptr != priv_endptr;
}

static gboolean
int_from_string (gchar * ptr, gchar ** endptr, gint * val)
{
  guint64 v;
  gboolean ret;

  ret = uint64_from_string (ptr, endptr, &v);
  *val = (gint) v;
  return ret;
}

static gboolean
double_from_string (gchar * ptr, gchar ** endptr, gdouble * val)
{
  gchar *priv_endptr;

  *val = g_ascii_strtod (ptr, &priv_endptr);

  if (endptr)
    *endptr = priv_endptr;

  return ptr != priv_endptr;
}

static gboolean
parse_attributes (gchar ** ptr, gchar ** a, gchar ** v)
{
  gchar *end, *p, *w;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (*ptr != NULL, FALSE);
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (v != NULL, FALSE);

  /* [attribute=value,]* */

  *a = *ptr;
  *v = p = g_utf8_strchr (*ptr, -1, '=');
  end = w = g_utf8_strchr (*ptr, -1, ',');

  if (end) {
    if (p[0] == '=' && p[1] == '"') {
      /* value can be "codec1, codec2" so we need to jump the closing quotes */
      w = g_utf8_next_char (p);
      w = g_utf8_next_char (w);
      w = g_utf8_strchr (w, -1, '"');
      w = g_utf8_strchr (w, -1, ',');
    }
    end = w;
    do {
      end = g_utf8_next_char (end);
    } while (end && *end == ' ');
    *w = '\0';
  }

  if (*v) {
    *v = g_utf8_next_char (*v);
    *p = '\0';
  } else {
    GST_WARNING ("missing = after attribute");
    return FALSE;
  }

  *ptr = end;
  return TRUE;
}

static gint
gst_m3u8_stream_compare_by_bitrate (GstM3U8Stream * a, GstM3U8Stream * b)
{
  return (a->bandwidth - b->bandwidth);
}

static gboolean
gst_m3u8_parse_uri (GstM3U8 * self, gchar * data, gchar ** uri)
{
  gchar *r;

  if (!gst_uri_is_valid (data)) {
    gchar *slash;
    if (!self->uri) {
      GST_WARNING ("uri not set, can't build a valid uri");
      return FALSE;
    }
    slash = g_utf8_strrchr (self->uri, -1, '/');
    if (!slash) {
      GST_WARNING ("Can't build a valid uri");
      return FALSE;
    }

    *slash = '\0';
    *uri = g_strdup_printf ("%s/%s", self->uri, data);
    *slash = '/';
  } else {
    *uri = g_strdup (data);
  }

  r = g_utf8_strchr (*uri, -1, '\r');
  if (r)
    *r = '\0';

  return TRUE;
}

static gchar *
gst_m3u8_strip_quotes (gchar * data)
{
  gchar *start, *stop;

  if (data[0] == '\"')
    start = g_utf8_next_char (data);
  else
    start = g_strrstr (data, "\"");
  if (start == NULL)
    return NULL;
  stop = g_strrstr (start + 1, "\"");
  if (stop == NULL)
    return NULL;

  return g_strndup (start, stop - start);
}

static gboolean
gst_m3u8_parse_media_codec (gchar * data,
    GstM3U8MediaCodec * codec, GstM3U8MediaType * type)
{
  gboolean ret = TRUE;

  if (!g_strcmp0 (data, HLS_CODEC_AAC_LC)) {
    *codec = GST_M3U8_MEDIA_CODEC_AAC_LC;
    *type = GST_M3U8_MEDIA_TYPE_AUDIO;
  } else if (!g_strcmp0 (data, HLS_CODEC_HE_AAC)) {
    *codec = GST_M3U8_MEDIA_CODEC_HE_AAC;
    *type = GST_M3U8_MEDIA_TYPE_AUDIO;
  } else if (!g_strcmp0 (data, HLS_CODEC_MP3)) {
    *codec = GST_M3U8_MEDIA_CODEC_MP3;
    *type = GST_M3U8_MEDIA_TYPE_AUDIO;
  } else if (g_str_has_prefix (data, HLS_CODEC_AUDIO)) {
    *codec = GST_M3U8_MEDIA_CODEC_GENERIC_AUDIO;
    *type = GST_M3U8_MEDIA_TYPE_AUDIO;
  } else if (!g_str_has_prefix (data, HLS_CODEC_H264_BASE)) {
    *codec = GST_M3U8_MEDIA_CODEC_H264_BASE;
    *type = GST_M3U8_MEDIA_TYPE_VIDEO;
  } else if (!g_str_has_prefix (data, HLS_CODEC_H264_MAIN)) {
    *codec = GST_M3U8_MEDIA_CODEC_H264_MAIN;
    *type = GST_M3U8_MEDIA_TYPE_VIDEO;
  } else if (!g_str_has_prefix (data, HLS_CODEC_H264_HIGH)) {
    *codec = GST_M3U8_MEDIA_CODEC_H264_HIGH;
    *type = GST_M3U8_MEDIA_TYPE_VIDEO;
  } else if (!g_str_has_prefix (data, HLS_CODEC_H264)) {
    *codec = GST_M3U8_MEDIA_CODEC_GENERIC_H264;
    *type = GST_M3U8_MEDIA_TYPE_VIDEO;
  } else {
    ret = FALSE;
  }

  return ret;
}

static gboolean
gst_m3u8_variant_playlist_parse (GstM3U8VariantPlaylist * self, gchar * data)
{
  gchar *end;
  gboolean first_stream = TRUE;
  GstM3U8Stream *stream = NULL;
  GstM3U8Stream *last_stream = NULL;
  gchar *audio_alternate = NULL;
  gchar *video_alternate = NULL;
  gchar *subtt_alternate = NULL;

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    GST_WARNING ("Data doesn't start with #EXTM3U");
    g_free (data);
    return FALSE;
  }

  data += 7;

  while (TRUE) {
    end = g_utf8_strchr (data, -1, '\n');
    if (end)
      *end = '\0';

    /* URI for an EXT-X-STREAM-INF:
     * Create a new GstM3U8Playlist for this URI. If the stream has alternative
     * renditions, fill the stream alternates with the correspoding group-id.
     * We must check that this URI is not listed in any of the alternates,
     * meaning that it's a playlist for a different media type (Audio or Video)*/
    if (data[0] != '#') {
      GstM3U8Playlist *pl = NULL;
      gchar *uri = NULL;
      gboolean seen = FALSE;

      if (stream == NULL) {
        GST_LOG ("%s: got uri without EXT-X-STREAM-INF, dropping", data);
        goto next_line;
      }

      if (!gst_m3u8_parse_uri (GST_M3U8 (self), data, &uri))
        goto next_line;


      if (audio_alternate != NULL) {
        /* This stream has audio alternative renditions already listed in a
         * rendition group. Fill the stream's alternate audio playlists */
        GList *walk =
            g_hash_table_lookup (self->audio_rendition_groups, audio_alternate);
        for (walk = walk; walk; walk = walk->next) {
          GstM3U8Media *media;

          if (walk == NULL)
            break;
          media = GST_M3U8_MEDIA (walk->data);
          if (media->uri == NULL) {
            if (gst_m3u8_stream_is_audio_only (stream)) {
              media->uri = g_strdup (uri);
              gst_m3u8_set_uri (GST_M3U8 (media->playlist), g_strdup (uri));
            }
          }
          g_hash_table_insert (stream->audio_alternates, media->name, media);
          if (media->is_default)
            stream->default_audio = g_strdup (media->name);
          seen |= !g_strcmp0 (GST_M3U8 (media->playlist)->uri, uri);
        }
        if (!seen) {
          /* This uri is not listed in any of the alternates, so it must be a
           * video playlist */
          pl = gst_m3u8_playlist_new ();
          gst_m3u8_set_uri (GST_M3U8 (pl), g_strdup (uri));
          stream->selected_video = pl;
        } else {
          stream->selected_video = NULL;
        }
      }
      if (video_alternate != NULL) {
        /* This stream has video alternative renditions already listed in a
         * rendition groups. Fill the stream's alternate video playlists */
        GList *walk =
            g_hash_table_lookup (self->video_rendition_groups, video_alternate);
        for (walk = walk; walk; walk = walk->next) {
          GstM3U8Media *media;

          if (walk == NULL)
            break;
          media = GST_M3U8_MEDIA (walk->data);
          if (media->uri == NULL) {
            if (gst_m3u8_stream_is_video_only (stream)) {
              media->uri = g_strdup (uri);
              gst_m3u8_set_uri (GST_M3U8 (media->playlist), g_strdup (uri));
            }
          }
          g_hash_table_insert (stream->video_alternates, media->name, media);
          if (media->is_default)
            stream->default_video = g_strdup (media->name);
          seen |= !g_strcmp0 (GST_M3U8 (media->playlist)->uri, uri);
        }
        if (!seen) {
          /* This uri is not listed in any of the alternates, so it must be an
           * audio playlist */
          pl = gst_m3u8_playlist_new ();
          gst_m3u8_set_uri (GST_M3U8 (pl), g_strdup (uri));
          stream->selected_audio = pl;
        }
      }
      if (subtt_alternate != NULL) {
        /* This stream has subtitles already listed in a rendition group.
         * Fill the stream's alternate subtitles playlists */
        GList *walk =
            g_hash_table_lookup (self->subtt_rendition_groups, subtt_alternate);
        for (walk = walk; walk; walk = walk->next) {
          GstM3U8Media *media;

          media = GST_M3U8_MEDIA (walk->data);
          if (media->is_default)
            stream->default_subtt = g_strdup (media->name);
          g_hash_table_insert (stream->subtt_alternates, media->name, media);
        }
      }

      if (audio_alternate == NULL && video_alternate == NULL) {
        /* No alternative renditions, this URI is the only one used for this
         * stream */
        pl = gst_m3u8_playlist_new ();
        gst_m3u8_set_uri (GST_M3U8 (pl), g_strdup (uri));
        stream->selected_video = pl;
      }

      if (uri)
        g_free (uri);
      if (pl != NULL) {
        GST_LOG ("Added new playlist with uri:%s", GST_M3U8 (pl)->uri);
        self->playlists = g_list_append (self->playlists, pl);
      }
      last_stream = stream;
      stream = NULL;
    }
    /* EXT-X-MEDIA:
     * Create a new GstM3U8Media and add it to the corresponding group */
    else if (g_str_has_prefix (data, "#EXT-X-MEDIA:")) {
      gchar *v, *a;
      gboolean error = FALSE, not_supported = FALSE;
      GstM3U8Media *media = gst_m3u8_media_new ();
      GHashTable *groups = NULL;
      GList *group = NULL;

      data = data + 13;

      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "TYPE")) {
          if (g_str_equal (v, "AUDIO")) {
            media->media_type = GST_M3U8_MEDIA_TYPE_AUDIO;
          } else if (g_str_equal (v, "VIDEO")) {
            media->media_type = GST_M3U8_MEDIA_TYPE_VIDEO;
          } else if (g_str_equal (v, "SUBTITLES")) {
            media->media_type = GST_M3U8_MEDIA_TYPE_SUBTITLES;
          } else {
            GST_WARNING ("Error while reading TYPE");
            error = TRUE;
            break;
          }
        } else if (g_str_equal (a, "GROUP-ID")) {
          media->group_id = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "NAME")) {
          media->name = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "LANGUAGE")) {
          media->language = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "DEFAULT")) {
          if (!bool_from_string (v, &media->is_default)) {
            GST_WARNING ("Error while reading DEFAULT");
          }
        } else if (g_str_equal (a, "AUTOSELECT")) {
          if (!bool_from_string (v, &media->autoselect)) {
            GST_WARNING ("Error while reading AUTOSELECT");
          }
        } else if (g_str_equal (a, "URI")) {
          gchar *uri = gst_m3u8_strip_quotes (v);
          gst_m3u8_parse_uri (GST_M3U8 (self), uri, &media->uri);
          g_free (uri);
        }
      }
      if (error) {
        gst_m3u8_media_free (media);
        return FALSE;
      }
      if (not_supported) {
        gst_m3u8_media_free (media);
        break;
      }

      media->playlist = gst_m3u8_playlist_new ();
      self->playlists = g_list_append (self->playlists, media->playlist);
      gst_m3u8_set_uri (GST_M3U8 (media->playlist), g_strdup (media->uri));
      if (media->media_type == GST_M3U8_MEDIA_TYPE_VIDEO) {
        groups = self->video_rendition_groups;
      } else if (media->media_type == GST_M3U8_MEDIA_TYPE_AUDIO) {
        groups = self->audio_rendition_groups;
      } else if (media->media_type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
        groups = self->subtt_rendition_groups;
      }
      if (groups != NULL && g_hash_table_contains (groups, media->group_id)) {
        group = g_hash_table_lookup (groups, media->group_id);
        group = g_list_append (group, media);
      } else {
        group = g_list_append (group, media);
        g_hash_table_insert (groups, g_strdup (media->group_id), group);
      }
      GST_LOG ("Added new media name:%s group_id:%s", media->name,
          media->group_id);

    }
    /* EXT-X-STREAM-INF:
     * Create a new GstM3U8Stream and wait for the URI which should be
     * parsed in the next line */
    else if (g_str_has_prefix (data, "#EXT-X-STREAM-INF:")) {
      gchar *v, *a;
      gboolean error = FALSE;

      if (stream != NULL) {
        GST_WARNING ("Found a stream without a uri..., dropping");
        gst_m3u8_stream_free (stream);
      }

      stream = gst_m3u8_stream_new ();
      stream->first = first_stream;
      first_stream = FALSE;

      data = data + 18;
      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "BANDWIDTH")) {
          if (!int_from_string (v, NULL, &stream->bandwidth)) {
            GST_WARNING ("Error while reading BANDWIDTH");
            error = TRUE;
            break;
          }
        } else if (g_str_equal (a, "PROGRAM-ID")) {
          if (!int_from_string (v, NULL, &stream->program_id)) {
            GST_WARNING ("Error while reading PROGRAM-ID");
            error = TRUE;
            break;
          }
        } else if (g_str_equal (a, "CODECS")) {
          gint i;
          gchar *codecs = gst_m3u8_strip_quotes (v);
          gchar **codecsa = g_strsplit (codecs, ",", 2);

          for (i = 0; i < 2; i++) {
            if (codecsa[i] != NULL) {
              GstM3U8MediaType type;
              GstM3U8MediaCodec codec;

              if (gst_m3u8_parse_media_codec (g_strstrip (codecsa[i]), &codec,
                      &type)) {
                if (type == GST_M3U8_MEDIA_TYPE_AUDIO)
                  stream->audio_codec = codec;
                else
                  stream->video_codec = codec;
              }
            }
          }
          g_strfreev (codecsa);
          g_free (codecs);
        } else if (g_str_equal (a, "VIDEO")) {
          if (video_alternate != NULL)
            g_free (video_alternate);
          video_alternate = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "AUDIO")) {
          if (audio_alternate != NULL)
            g_free (audio_alternate);
          audio_alternate = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "SUBTITLES")) {
          if (subtt_alternate != NULL)
            g_free (subtt_alternate);
          subtt_alternate = gst_m3u8_strip_quotes (v);
        } else if (g_str_equal (a, "RESOLUTION")) {
          gchar **values = g_strsplit (v, "x", 2);

          stream->width = g_ascii_strtoll (values[0], NULL, 0);
          stream->height = g_ascii_strtoll (values[1], NULL, 0);
        }
      }

      if (error) {
        gst_m3u8_stream_free (stream);
        return FALSE;
      }

      self->streams = g_list_append (self->streams, stream);
      GST_LOG ("Adding stream with bandwidth:%d", stream->bandwidth);
    }
    /* EXT-X-I-FRAME-STREAM-INF:
     * Create a new GstM3U8Media and add it to the corresponding group */
    else if (g_str_has_prefix (data, "#EXT-X-I-FRAME-STREAM-INF:")) {
      gchar *v, *a;
      GstM3U8Playlist *playlist;

      if (last_stream == NULL) {
        GST_WARNING ("Found an I-Frames URI without a valid stream");
        goto next_line;
      }

      data = data + 26;


      playlist = gst_m3u8_playlist_new ();
      last_stream->i_frame = playlist;
      while (data && parse_attributes (&data, &a, &v)) {

        if (g_str_equal (a, "BANDWIDTH")) {
          if (!int_from_string (v, NULL, &last_stream->bandwidth)) {
            GST_WARNING ("Error while reading BANDWIDTH");
          }
        } else if (g_str_equal (a, "PROGRAM-ID")) {
          if (!int_from_string (v, NULL, &last_stream->program_id)) {
            GST_WARNING ("Error while reading PROGRAM-ID");
          }
        } else if (g_str_equal (a, "URI")) {
          gchar *uri = gst_m3u8_strip_quotes (v);
          gst_m3u8_parse_uri (GST_M3U8 (self), uri,
              &(GST_M3U8 (playlist)->uri));
          g_free (uri);
        }
      }
      GST_LOG ("Added new playlist with uri:%s", GST_M3U8 (playlist)->uri);
      self->playlists = g_list_append (self->playlists, playlist);
      self->i_frame_streams =
          g_list_append (self->i_frame_streams, last_stream);
    } else {
      GST_LOG ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  if (audio_alternate)
    g_free (audio_alternate);
  if (video_alternate)
    g_free (video_alternate);
  if (subtt_alternate)
    g_free (subtt_alternate);

  /* order streams by bitrate */
  self->streams = g_list_sort (self->streams,
      (GCompareFunc) gst_m3u8_stream_compare_by_bitrate);

  return TRUE;
}

static gboolean
gst_m3u8_playlist_update (GstM3U8Playlist * self, gchar * data,
    gboolean * updated)
{
  gint val;
  GstClockTime duration = 0;
  gchar *title = NULL, *end = NULL;
//  gboolean discontinuity;
  gint64 offset = -1, length = -1, acc_offset = 0;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (updated != NULL, FALSE);

  *updated = TRUE;

  /* check if the data changed since last update */
  if (self->last_data && g_str_equal (self->last_data, data)) {
    GST_DEBUG ("Playlist is the same as previous one");
    *updated = FALSE;
    g_free (data);
    return TRUE;
  }

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    GST_WARNING ("Data doesn't start with #EXTM3U");
    *updated = FALSE;
    g_free (data);
    return FALSE;
  }

  g_free (self->last_data);
  self->last_data = data;

  if (self->files) {
    g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
    g_list_free (self->files);
    self->files = NULL;
  }
  self->mediasequence = 0;

  data += 7;


  while (TRUE) {
    end = g_utf8_strchr (data, -1, '\n');
    if (end)
      *end = '\0';

    /* Uri for media segment */
    if (data[0] != '#') {
      GstM3U8MediaFile *file;
      gchar *uri = NULL;

      if (duration <= 0) {
        GST_LOG ("%s: got line without EXTINF, dropping", data);
        goto next_line;
      }

      if (!gst_m3u8_parse_uri (GST_M3U8 (self), data, &uri))
        goto next_line;


      file = gst_m3u8_media_file_new (uri, title, duration,
          self->mediasequence++, offset, length);
      duration = 0;
      title = NULL;
      offset = -1;
      length = -1;
      self->files = g_list_append (self->files, file);
    } else if (g_str_has_prefix (data, "#EXT-X-ENDLIST")) {
      self->endlist = TRUE;
    } else if (g_str_has_prefix (data, "#EXT-X-VERSION:")) {
      if (int_from_string (data + 15, &data, &val))
        self->version = val;
    } else if (g_str_has_prefix (data, "#EXT-X-TARGETDURATION:")) {
      if (int_from_string (data + 22, &data, &val))
        self->targetduration = val * GST_SECOND;
    } else if (g_str_has_prefix (data, "#EXT-X-MEDIA-SEQUENCE:")) {
      if (int_from_string (data + 22, &data, &val))
        self->mediasequence = val;
    } else if (g_str_has_prefix (data, "#EXT-X-DISCONTINUITY")) {
      /* discontinuity = TRUE; */
    } else if (g_str_has_prefix (data, "#EXT-X-PROGRAM-DATE-TIME:")) {
      /* <YYYY-MM-DDThh:mm:ssZ> */
      GST_DEBUG ("FIXME parse date");
    } else if (g_str_has_prefix (data, "#EXT-X-ALLOW-CACHE:")) {
      g_free (self->allowcache);
      self->allowcache = g_strdup (data + 19);
    } else if (g_str_has_prefix (data, "#EXTINF:")) {
      gdouble fval;
      if (!double_from_string (data + 8, &data, &fval)) {
        GST_WARNING ("Can't read EXTINF duration");
        goto next_line;
      }
      duration = fval * (gdouble) GST_SECOND;
      if (duration > self->targetduration)
        GST_WARNING ("EXTINF duration > TARGETDURATION");
      if (!data || *data != ',')
        goto next_line;
      data = g_utf8_next_char (data);
      if (data != end) {
        g_free (title);
        title = g_strdup (data);
      }
    } else if (g_str_has_prefix (data, "#EXT-X-BYTERANGE:")) {
      gchar **content;

      content = g_strsplit (data + 17, "@", 2);
      if (!int64_from_string (content[0], NULL, &length)) {
        GST_WARNING ("Error while reading the lenght in #EXT-X-BYTERANGE");
        g_strfreev (content);
        return FALSE;
      }
      if (content[1] == NULL) {
        offset = acc_offset;
        acc_offset += length;
      } else if (!int64_from_string (content[1], NULL, &offset)) {
        GST_WARNING ("Error while reading the offset in #EXT-X-BYTERANGE");
        g_strfreev (content);
        return FALSE;
      }
      g_strfreev (content);
    } else {
      GST_LOG ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  return TRUE;
}

static gboolean
gst_m3u8_is_variant_playlist (gchar * data)
{
  /* A variant playlist must have at least one EXT-X-STREAM-INF */
  return g_strrstr (data, "#EXT-X-STREAM-INF:") != NULL;
}

static void
gst_m3u8_client_select_defaults (GstM3U8Client * client)
{
  GList *walk;

  for (walk = client->main->streams; walk; walk = walk->next) {
    if (GST_M3U8_STREAM (walk->data)->first) {
      client->selected_stream = GST_M3U8_STREAM (walk->data);
    }
  }

  if (client->selected_stream->default_audio != NULL) {
    GST_M3U8_CLIENT_UNLOCK (client);
    gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
        client->selected_stream->default_audio);
    GST_M3U8_CLIENT_LOCK (client);
  }

  if (client->selected_stream->default_video != NULL) {
    GST_M3U8_CLIENT_UNLOCK (client);
    gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_VIDEO,
        client->selected_stream->default_video);
    GST_M3U8_CLIENT_LOCK (client);
  }

  if (client->selected_stream->default_subtt != NULL) {
    GList *defaults;
    GST_M3U8_CLIENT_UNLOCK (client);
    defaults =
        gst_m3u8_client_get_alternates (client, GST_M3U8_MEDIA_TYPE_SUBTITLES);
    if (client->selected_stream->default_subtt != NULL)
      g_free (client->selected_stream->default_subtt);
    client->selected_stream->default_subtt =
        g_strdup ((gchar *) defaults->data);
    gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
        client->selected_stream->default_subtt);
    GST_M3U8_CLIENT_LOCK (client);
  }

  if (client->selected_stream->selected_video == NULL) {
    /* The first stream should never be the audio-only fallback */
    if (g_list_length (client->main->streams) > 0)
      client->selected_stream =
          GST_M3U8_STREAM (g_list_first (client->main->streams)->next->data);
  }
}

static GstM3U8Playlist *
gst_m3u8_client_get_current_playlist (GstM3U8Client * client)
{
  GstM3U8Stream *selected_stream = client->selected_stream;

  if (selected_stream == NULL)
    return NULL;

  if (selected_stream->selected_video != NULL) {
    return selected_stream->selected_video;
  } else if (selected_stream->selected_audio != NULL) {
    return selected_stream->selected_audio;
  } else {
    return NULL;
  }
}

static void
gst_m3u8_client_init_sequence (GstM3U8Client * client)
{
  GstM3U8Stream *stream = client->selected_stream;

  if (client->sequence == -1 && stream != NULL) {
    GstM3U8Playlist *pl = gst_m3u8_client_get_current_playlist (client);
    if (pl != NULL && g_list_length (pl->files) > 0) {
      client->sequence =
          GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data)->sequence;
    }
    GST_DEBUG ("Setting first sequence at %d", client->sequence);
  }
}

static void
gst_m3u8_client_update_alternate (GstM3U8Client * client)
{
  GList *walk;

  for (walk = client->main->streams; walk; walk = walk->next) {
    GstM3U8Stream *stream;
    GstM3U8Playlist *pl;

    if (walk == NULL)
      break;

    stream = GST_M3U8_STREAM (walk->data);

    if (client->video_alternate != NULL) {
      pl = GST_M3U8_MEDIA (g_hash_table_lookup (stream->video_alternates,
              client->video_alternate))->playlist;
      if (pl != NULL)
        stream->selected_video = pl;
    }
    if (client->audio_alternate != NULL) {
      /* If this stream has audio and video and the alternative rendition
       * is the default one, hence included in the muxed audio/video stream,
       * we don't have to use the audio alternative playlist */
      if (!g_strcmp0 (client->audio_alternate, stream->default_audio) &&
          gst_m3u8_stream_is_audio_video (stream)) {
        stream->selected_audio = NULL;
      } else {
        pl = GST_M3U8_MEDIA (g_hash_table_lookup (stream->audio_alternates,
                client->audio_alternate))->playlist;
        if (pl != NULL) {
          stream->selected_audio = pl;
        }
      }
    }
    if (client->subtt_alternate != NULL) {
      pl = GST_M3U8_MEDIA (g_hash_table_lookup (stream->subtt_alternates,
              client->subtt_alternate))->playlist;
      if (pl != NULL)
        stream->selected_subtt = pl;
    } else {
      stream->selected_subtt = NULL;
    }
  }
}

/*********************************************
 *                                           *
 *              Public API                   *
 *                                           *
 ********************************************/

GstM3U8Client *
gst_m3u8_client_new (const gchar * uri)
{
  GstM3U8Client *client;

  g_return_val_if_fail (uri != NULL, NULL);

  client = g_new0 (GstM3U8Client, 1);
  client->main = gst_m3u8_variant_playlist_new ();
  gst_m3u8_set_uri (GST_M3U8 (client->main), g_strdup (uri));
  client->lock = g_mutex_new ();
  client->selected_stream = NULL;
  client->audio_alternate = NULL;
  client->video_alternate = NULL;
  client->sequence = -1;
  client->update_failed_count = 0;

  return client;
}

void
gst_m3u8_client_free (GstM3U8Client * self)
{
  g_return_if_fail (self != NULL);

  if (self->main != NULL)
    gst_m3u8_variant_playlist_free (self->main);
  g_mutex_free (self->lock);

  if (self->video_alternate != NULL)
    g_free (self->video_alternate);

  if (self->audio_alternate != NULL)
    g_free (self->audio_alternate);

  if (self->subtt_alternate != NULL)
    g_free (self->subtt_alternate);

  g_free (self);
}

void
gst_m3u8_client_set_current (GstM3U8Client * self, GstM3U8Stream * stream)
{
  g_return_if_fail (self != NULL);

  GST_M3U8_CLIENT_LOCK (self);
  if (stream != self->selected_stream) {
    self->selected_stream = stream;
    self->update_failed_count = 0;
  }
  GST_M3U8_CLIENT_UNLOCK (self);
}

gboolean
gst_m3u8_client_parse_main_playlist (GstM3U8Client * self, gchar * data)
{
  gboolean ret = FALSE, updated = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (self);

  if (!gst_m3u8_is_variant_playlist (data)) {
    /* If if's a rendition playlist create a dummy stream and add it to
     * the variant playlist */
    GstM3U8Stream *stream = gst_m3u8_stream_new ();
    GstM3U8Playlist *pl = gst_m3u8_playlist_new ();
    GST_DEBUG ("Parsing rendition playlist");
    stream->selected_video = pl;
    stream->first = TRUE;
    gst_m3u8_set_uri (GST_M3U8 (stream->selected_video),
        g_strdup (GST_M3U8 (self->main)->uri));
    if (!gst_m3u8_playlist_update (stream->selected_video, data, &updated)) {
      gst_m3u8_stream_free (stream);
      gst_m3u8_playlist_free (pl);
      goto out;
    }
    self->main->streams = g_list_append (self->main->streams, stream);
    self->main->playlists = g_list_append (self->main->playlists, pl);
    gst_m3u8_client_select_defaults (self);
    gst_m3u8_client_init_sequence (self);
  } else {
    /* Parse the variant playlist */
    GST_DEBUG ("Parsing variant playlist");
    if (!gst_m3u8_variant_playlist_parse (self->main, data)) {
      g_free (data);
      goto out;
    }
    gst_m3u8_client_select_defaults (self);
    g_free (data);
  }

  ret = TRUE;

out:
  GST_M3U8_CLIENT_UNLOCK (self);
  return ret;
}

gboolean
gst_m3u8_client_update_i_frame (GstM3U8Client * self, gchar * data)
{
  gboolean updated = TRUE;
  gboolean ret = FALSE;
  GstM3U8Stream *selected;

  g_return_val_if_fail (self != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (self);

  selected = self->selected_stream;
  /* Update the playlists for the selected streams */
  if (selected->i_frame == NULL) {
    GST_WARNING ("The following stream doesn't have an I-Frame playlist");
    goto out;
  }

  if (!gst_m3u8_playlist_update (selected->i_frame, data, &updated))
    goto out;
  if (!updated) {
    self->update_failed_count++;
    goto out;
  }

  /* The initialiation segment is defined by #EXT-X-MAP, which only appeared in
   * the version 5 of the protocol, we must assume the initialization segment
   * with the PAT/PMT tables is at the very beginning of the first segment */
  if (selected->init_segment == NULL) {
    GstM3U8MediaFile *file, *init_segment;

    file = GST_M3U8_MEDIA_FILE (selected->i_frame->files->data);
    selected->init_segment = init_segment =
        gst_m3u8_media_file_new (g_strdup (file->uri), NULL, 0, 0, 0,
        file->offset);
  }

  ret = TRUE;
out:
  GST_M3U8_CLIENT_UNLOCK (self);
  return ret;
}

gboolean
gst_m3u8_client_update (GstM3U8Client * self, gchar * video_data,
    gchar * audio_data, gchar * subtt_data)
{
  gboolean updated = TRUE;
  gboolean ret = FALSE;
  GstM3U8Stream *selected;

  g_return_val_if_fail (self != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (self);

  selected = self->selected_stream;
  /* Update the playlists for the selected streams */
  if (selected->selected_video != NULL && video_data != NULL) {
    if (!gst_m3u8_playlist_update (selected->selected_video,
            video_data, &updated))
      goto out;
    if (!updated) {
      self->update_failed_count++;
      goto out;
    }
  }
  if (selected->selected_audio != NULL && audio_data != NULL) {
    if (!gst_m3u8_playlist_update (selected->selected_audio,
            audio_data, &updated))
      goto out;
    if (!updated) {
      self->update_failed_count++;
      goto out;
    }
  }
  if (selected->selected_subtt != NULL && subtt_data != NULL) {
    if (!gst_m3u8_playlist_update (selected->selected_subtt,
            subtt_data, &updated))
      goto out;
    if (!updated) {
      self->update_failed_count++;
      goto out;
    }
  }

  if (G_UNLIKELY (self->sequence == -1)) {
    gst_m3u8_client_init_sequence (self);
  }

  ret = TRUE;
out:
  GST_M3U8_CLIENT_UNLOCK (self);
  return ret;
}

void
gst_m3u8_client_get_current_position (GstM3U8Client * client,
    GstClockTime * video, GstClockTime * audio)
{
  GstClockTime audio_ts, video_ts;
  GstM3U8Playlist *video_pl, *audio_pl;
  GstM3U8Stream *selected_stream = client->selected_stream;

  video_ts = GST_CLOCK_TIME_NONE;
  audio_ts = GST_CLOCK_TIME_NONE;

  if (selected_stream == NULL)
    goto exit;

  video_pl = selected_stream->selected_video;
  audio_pl = selected_stream->selected_audio;
  if (video_pl == NULL && audio_pl == NULL)
    goto exit;

  if (video_pl != NULL) {
    video_ts = gst_m3u8_playlist_get_position (video_pl, client->sequence);
  }
  if (audio_pl != NULL) {
    audio_ts = gst_m3u8_playlist_get_position (audio_pl, client->sequence);
  }

  if (audio_ts == -1)
    audio_ts = video_ts;
  if (video_ts == -1)
    video_ts = audio_ts;

  GST_DEBUG ("Current position video:%" GST_TIME_FORMAT
      " audio:%" GST_TIME_FORMAT, GST_TIME_ARGS (video_ts),
      GST_TIME_ARGS (audio_ts));

exit:
  if (audio != NULL)
    *audio = audio_ts;
  if (video != NULL)
    *video = video_ts;
}

gboolean
gst_m3u8_client_get_next_fragment (GstM3U8Client * client,
    GstFragment ** video_fragment, GstFragment ** audio_fragment,
    GstFragment ** subtt_fragment)
{
  GstM3U8MediaFile *video_file, *audio_file, *subtt_file;
  GstClockTime video_ts, audio_ts, end_timestamp;
  gboolean updated = FALSE;
  guint client_sequence;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream != NULL, FALSE);
  g_return_val_if_fail (video_fragment != NULL || audio_fragment != NULL
      || subtt_fragment != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);

  gst_m3u8_client_get_current_position (client, &video_ts, &audio_ts);
  end_timestamp =
      gst_m3u8_client_get_current_playlist (client)->targetduration + video_ts;

  GST_DEBUG ("Looking for fragment %d", client->sequence);
  *video_fragment = NULL;
  *audio_fragment = NULL;
  *subtt_fragment = NULL;
  video_file = gst_m3u8_stream_find_next (client->selected_stream,
      client->sequence, end_timestamp, GST_M3U8_MEDIA_TYPE_VIDEO);
  audio_file = gst_m3u8_stream_find_next (client->selected_stream,
      client->sequence, end_timestamp, GST_M3U8_MEDIA_TYPE_AUDIO);
  subtt_file = gst_m3u8_stream_find_next (client->selected_stream,
      client->sequence, end_timestamp, GST_M3U8_MEDIA_TYPE_SUBTITLES);

  if (video_file == NULL && audio_file == NULL && subtt_file == NULL) {
    GST_M3U8_CLIENT_UNLOCK (client);
    return FALSE;
  }


  client_sequence = client->sequence;

  if (video_file != NULL) {
    *video_fragment = gst_m3u8_media_file_get_fragment (video_file, video_ts,
        client_sequence != video_file->sequence);
    client->sequence = video_file->sequence + 1;
    updated = TRUE;
  }

  if (audio_file != NULL) {
    *audio_fragment = gst_m3u8_media_file_get_fragment (audio_file, audio_ts,
        client_sequence != audio_file->sequence);
    if (!updated) {
      client->sequence = audio_file->sequence + 1;
      updated = TRUE;
    }
  }

  if (subtt_file != NULL) {
    *subtt_fragment = gst_m3u8_media_file_get_fragment (subtt_file, video_ts,
        client_sequence != subtt_file->sequence);
    if (!updated) {
      client->sequence = subtt_file->sequence + 1;
    }
  }

  GST_M3U8_CLIENT_UNLOCK (client);
  return TRUE;
}

gboolean
gst_m3u8_client_get_prev_i_frames (GstM3U8Client * client,
    GstFragment ** init_segment, GList ** i_frames)
{
  GstClockTime timestamp, end_timestamp, pos = 0;
  GstM3U8Playlist *pl;
  GList *walk;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (i_frames != NULL, FALSE);
  g_return_val_if_fail (init_segment != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream->i_frame != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);

  pl = client->selected_stream->i_frame;
  *i_frames = NULL;
  *init_segment =
      gst_m3u8_media_file_get_fragment (client->selected_stream->init_segment,
      0, FALSE);

  gst_m3u8_client_get_current_position (client, &end_timestamp, NULL);
  timestamp = end_timestamp -
      gst_m3u8_client_get_current_playlist (client)->targetduration;

  /* Get a list of all the i_frames that are inside the segment boundaries */

  /* For a live playlist we must guess the first timestamp from the media
   * sequence and the target duration */
  if (!pl->endlist) {
    guint first_seq = pl->mediasequence - g_list_length (pl->files);
    pos = (GstClockTime) (first_seq * pl->targetduration);
  }
  if (pos > timestamp)
    return FALSE;

  for (walk = pl->files; walk; walk = walk->next) {
    GstM3U8MediaFile *media = GST_M3U8_MEDIA_FILE (walk->data);

    if (pos >= timestamp) {
      *i_frames = g_list_append (*i_frames,
          gst_m3u8_media_file_get_fragment (media, pos, FALSE));
    }
    pos += media->duration;
    if (pos > end_timestamp)
      break;
  }

  client->sequence -= 1;
  *i_frames = g_list_reverse (*i_frames);

  GST_M3U8_CLIENT_UNLOCK (client);
  return TRUE;
}

gboolean
gst_m3u8_client_get_next_i_frames (GstM3U8Client * client,
    GstFragment ** init_segment, GList ** i_frames)
{
  GstClockTime timestamp, end_timestamp, pos = 0;
  GstM3U8Playlist *pl;
  GList *walk;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (i_frames != NULL, FALSE);
  g_return_val_if_fail (init_segment != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream->i_frame != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);

  pl = client->selected_stream->i_frame;
  *i_frames = NULL;
  *init_segment =
      gst_m3u8_media_file_get_fragment (client->selected_stream->init_segment,
      0, FALSE);

  gst_m3u8_client_get_current_position (client, &timestamp, NULL);
  end_timestamp =
      gst_m3u8_client_get_current_playlist (client)->targetduration + timestamp;

  /* Get a list of all the i_frames that are inside the segment boundaries */

  /* For a live playlist we must guess the first timestamp from the media
   * sequence and the target duration */
  if (!pl->endlist) {
    guint first_seq = pl->mediasequence - g_list_length (pl->files);
    pos = (GstClockTime) (first_seq * pl->targetduration);
  }
  if (pos > timestamp)
    return FALSE;

  for (walk = pl->files; walk; walk = walk->next) {
    GstM3U8MediaFile *media = GST_M3U8_MEDIA_FILE (walk->data);

    if (pos >= timestamp) {
      *i_frames = g_list_append (*i_frames,
          gst_m3u8_media_file_get_fragment (media, pos, FALSE));
    }
    pos += media->duration;
    if (pos > end_timestamp)
      break;
  }

  client->sequence += 1;

  GST_M3U8_CLIENT_UNLOCK (client);
  return TRUE;
}

static void
_sum_duration (GstM3U8MediaFile * self, GstClockTime * duration)
{
  *duration += self->duration;
}

GstClockTime
gst_m3u8_client_get_duration (GstM3U8Client * client)
{
  GstClockTime duration = 0;
  GstM3U8Playlist *pl;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_CLIENT_LOCK (client);
  pl = gst_m3u8_client_get_current_playlist (client);
  if (pl == NULL || !pl->endlist) {
    /* We can only get the duration for on-demand streams */
    duration = GST_CLOCK_TIME_NONE;
    goto exit;
  }

  g_list_foreach (pl->files, (GFunc) _sum_duration, &duration);


exit:
  GST_DEBUG ("Total duration is %" GST_TIME_FORMAT, GST_TIME_ARGS (duration));
  GST_M3U8_CLIENT_UNLOCK (client);
  return duration;
}

GstClockTime
gst_m3u8_client_get_target_duration (GstM3U8Client * client)
{
  GstClockTime duration = 0;
  GstM3U8Playlist *pl;

  g_return_val_if_fail (client != NULL, GST_CLOCK_TIME_NONE);

  GST_M3U8_CLIENT_LOCK (client);
  pl = gst_m3u8_client_get_current_playlist (client);
  if (pl != NULL)
    duration = pl->targetduration;
  GST_M3U8_CLIENT_UNLOCK (client);
  return duration;
}

const gchar *
gst_m3u8_client_get_uri (GstM3U8Client * client)
{
  const gchar *uri;

  g_return_val_if_fail (client != NULL, NULL);

  GST_M3U8_CLIENT_LOCK (client);
  uri = GST_M3U8 (client->main)->uri;
  GST_M3U8_CLIENT_UNLOCK (client);
  return uri;
}

void
gst_m3u8_client_get_current_uri (GstM3U8Client * client,
    const gchar ** video_uri, const gchar ** audio_uri,
    const gchar ** subtt_uri)
{
  GstM3U8Playlist *pl;
  gboolean is_live;

  g_return_if_fail (client != NULL);

  *video_uri = NULL;
  *audio_uri = NULL;
  *subtt_uri = NULL;

  is_live = gst_m3u8_client_is_live (client);

  /* Update only playlists for live streams or if the playlist hasn't been
   * loaded yet */
  GST_M3U8_CLIENT_LOCK (client);
  if (client->selected_stream->selected_video != NULL) {
    pl = GST_M3U8_PLAYLIST (client->selected_stream->selected_video);
    if (is_live || pl->last_data == NULL)
      *video_uri = GST_M3U8 (pl)->uri;
  }
  if (client->selected_stream->selected_audio != NULL) {
    pl = GST_M3U8_PLAYLIST (client->selected_stream->selected_audio);
    if (is_live || pl->last_data == NULL)
      *audio_uri = GST_M3U8 (pl)->uri;
  }
  if (client->selected_stream->selected_subtt != NULL) {
    pl = GST_M3U8_PLAYLIST (client->selected_stream->selected_subtt);
    if (is_live || pl->last_data == NULL)
      *subtt_uri = GST_M3U8 (pl)->uri;
  }
  GST_M3U8_CLIENT_UNLOCK (client);
}

const gchar *
gst_m3u8_client_get_i_frame_uri (GstM3U8Client * client)
{
  GstM3U8Playlist *pl;
  gboolean is_live;
  const gchar *ret = NULL;

  g_return_val_if_fail (client != NULL, NULL);

  is_live = gst_m3u8_client_is_live (client);

  /* Update only playlists for live streams or if the playlist hasn't been
   * loaded yet */
  GST_M3U8_CLIENT_LOCK (client);
  if (client->selected_stream->i_frame != NULL) {
    pl = GST_M3U8_PLAYLIST (client->selected_stream->i_frame);
    if (is_live || pl->last_data == NULL)
      ret = GST_M3U8 (pl)->uri;
  }
  GST_M3U8_CLIENT_UNLOCK (client);

  return ret;
}

gboolean
gst_m3u8_client_supports_trick_modes (GstM3U8Client * client)
{
  g_return_val_if_fail (client != NULL, FALSE);

  if (gst_m3u8_client_is_live (client))
    return FALSE;

  return client->selected_stream->i_frame != NULL;
}

gboolean
gst_m3u8_client_is_live (GstM3U8Client * client)
{
  GstM3U8Playlist *pl;
  gboolean ret;

  g_return_val_if_fail (client != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);
  pl = gst_m3u8_client_get_current_playlist (client);
  if (pl == NULL || pl->endlist)
    ret = FALSE;
  else
    ret = TRUE;
  GST_DEBUG ("Client is %slive", ret ? "" : "not ");
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

void
gst_m3u8_client_set_max_resolution (GstM3U8Client * client, gchar * resolution)
{
  gchar **values;
  gint width, height;

  GST_M3U8_CLIENT_LOCK (client);

  if (resolution == NULL) {
    client->max_resolution = 0;
  } else {
    values = g_strsplit (resolution, "x", 2);

    width = g_ascii_strtoll (values[0], NULL, 0);
    height = g_ascii_strtoll (values[1], NULL, 0);

    client->max_resolution = width * height;
  }

  GST_M3U8_CLIENT_UNLOCK (client);
}

GstM3U8Stream *
gst_m3u8_client_get_stream_for_bitrate (GstM3U8Client * client, guint bitrate)
{
  GList *list;
  GstM3U8Stream *stream, *target_stream;
  guint resolution;

  GST_M3U8_CLIENT_LOCK (client);
  list = g_list_first (client->main->streams);
  stream = target_stream = GST_M3U8_STREAM (list->data);

  /*  Go to the highest possible bandwidth allowed */
  while (stream->bandwidth < bitrate) {
    target_stream = stream;
    list = g_list_next (list);
    if (!list)
      break;
    stream = GST_M3U8_STREAM (list->data);
  }

  if (client->max_resolution != 0) {
    resolution = stream->width * stream->height;
    /* Check that this stream mets the resolution constraints */
    while (resolution > client->max_resolution) {
      list = g_list_previous (list);
      stream = GST_M3U8_STREAM (g_list_previous (list)->data);
      resolution = stream->width * stream->height;
    }
  }

  GST_DEBUG ("Selected stream with bitrate %d for %d", stream->bandwidth,
      bitrate);
  GST_M3U8_CLIENT_UNLOCK (client);

  return target_stream;
}

GstM3U8Stream *
gst_m3u8_client_get_previous_stream (GstM3U8Client * client)
{
  return NULL;
}

GstM3U8Stream *
gst_m3u8_client_get_next_stream (GstM3U8Client * client)
{
  return NULL;
}

gboolean
gst_m3u8_client_seek (GstM3U8Client * client, gint64 seek_time)
{
  GList *list;
  GstClockTime ts;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *fragment, *target_fragment;
  gboolean ret = FALSE;

  GST_M3U8_CLIENT_LOCK (client);
  pl = gst_m3u8_client_get_current_playlist (client);
  if (pl == NULL)
    goto exit;

  list = g_list_first (pl->files);
  fragment = target_fragment = GST_M3U8_MEDIA_FILE (list->data);

  if (!pl->endlist) {
    ts = fragment->sequence * pl->targetduration;
  } else {
    ts = 0;
  }

  if (seek_time < ts) {
    GST_WARNING ("Invalid seek, %" GST_TIME_FORMAT " is earlier than start "
        "time %" GST_TIME_FORMAT, GST_TIME_ARGS (seek_time),
        GST_TIME_ARGS (ts));
    goto exit;
  }

  /*  Go to the fragment with the highest ts possible */
  while (ts < seek_time) {
    target_fragment = fragment;
    list = g_list_next (list);
    if (!list) {
      break;
    }
    fragment = GST_M3U8_MEDIA_FILE (list->data);
    ts += fragment->duration;
    if (ts == seek_time) {
      target_fragment = fragment;
      break;
    }
  }

  if (ts + target_fragment->duration <= seek_time) {
    GST_WARNING ("Invalid seek, %" GST_TIME_FORMAT " is later than end "
        "time %" GST_TIME_FORMAT, GST_TIME_ARGS (seek_time),
        GST_TIME_ARGS (ts + target_fragment->duration));
    goto exit;
  }

  client->sequence = target_fragment->sequence;
  ret = TRUE;
  GST_DEBUG ("Seeking successfully to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (seek_time));

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

GList *
gst_m3u8_client_get_alternates (GstM3U8Client * client,
    GstM3U8MediaType media_type)
{
  GHashTable *alternates = NULL;
  GList *names = NULL, *walk;

  GST_M3U8_CLIENT_LOCK (client);
  if (media_type == GST_M3U8_MEDIA_TYPE_VIDEO) {
    alternates = client->selected_stream->video_alternates;
  } else if (media_type == GST_M3U8_MEDIA_TYPE_AUDIO) {
    alternates = client->selected_stream->audio_alternates;
  } else if (media_type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
    alternates = client->selected_stream->subtt_alternates;
  }
  if (alternates == NULL) {
    GST_DEBUG ("This streams has no alternative %s rendition",
        media_type == GST_M3U8_MEDIA_TYPE_VIDEO ? "video" : "audio");
    goto exit;
  }

  for (walk = g_hash_table_get_values (alternates); walk; walk = walk->next) {
    GstM3U8Media *media = GST_M3U8_MEDIA (walk->data);
    if (media->is_default && media_type != GST_M3U8_MEDIA_TYPE_SUBTITLES)
      names = g_list_prepend (names, media->name);
    else
      names = g_list_append (names, media->name);
  }

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return names;
}

gboolean
gst_m3u8_client_set_alternate (GstM3U8Client * client,
    GstM3U8MediaType media_type, const gchar * name)
{
  GHashTable *groups = NULL;
  gboolean ret = FALSE;
  gchar *alt_name = NULL;

  GST_M3U8_CLIENT_LOCK (client);
  if (media_type == GST_M3U8_MEDIA_TYPE_VIDEO)
    groups = client->selected_stream->video_alternates;
  else if (media_type == GST_M3U8_MEDIA_TYPE_AUDIO)
    groups = client->selected_stream->audio_alternates;
  else if (media_type == GST_M3U8_MEDIA_TYPE_SUBTITLES)
    groups = client->selected_stream->subtt_alternates;

  if (name != NULL)
    alt_name = g_strdup (name);

  if (alt_name != NULL && !g_hash_table_contains (groups, alt_name)) {
    GST_WARNING ("Could not find and alternative %s rendition alt_named %s",
        media_type == GST_M3U8_MEDIA_TYPE_VIDEO ? "video" : "audio", alt_name);
    goto exit;
  }

  if (media_type == GST_M3U8_MEDIA_TYPE_VIDEO) {
    if (client->video_alternate != NULL)
      g_free (client->video_alternate);
    client->video_alternate = alt_name;
  } else if (media_type == GST_M3U8_MEDIA_TYPE_AUDIO) {
    if (client->audio_alternate != NULL)
      g_free (client->audio_alternate);
    client->audio_alternate = alt_name;
  } else if (media_type == GST_M3U8_MEDIA_TYPE_SUBTITLES) {
    if (client->subtt_alternate != NULL)
      g_free (client->subtt_alternate);
    client->subtt_alternate = alt_name;
  }
  GST_DEBUG ("Selected alternative %s rendition: %s",
      media_type == GST_M3U8_MEDIA_TYPE_VIDEO ? "video" : "audio", alt_name);
  gst_m3u8_client_update_alternate (client);
  ret = TRUE;

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

gboolean
gst_m3u8_client_check_sequence_validity (GstM3U8Client * client)
{
  guint last_sequence;
  GstM3U8Playlist *pl;
  gboolean ret = TRUE;

  GST_M3U8_CLIENT_LOCK (client);
  pl = gst_m3u8_client_get_current_playlist (client);
  if (pl == NULL || pl->endlist)
    goto exit;

  last_sequence = pl->mediasequence - 1;
  GST_DEBUG ("Last sequence is: %d", last_sequence);

  if (client->sequence > last_sequence - 3) {
    GST_DEBUG ("Sequence is beyond playlist. Moving back to %d",
        last_sequence - 3);
    client->sequence = last_sequence - 3;
    ret = FALSE;
  }

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

GList *
gst_m3u8_client_get_streams_bitrates (GstM3U8Client * client)
{
  GList *bitrates = NULL;
  GList *walk;

  GST_M3U8_CLIENT_LOCK (client);
  for (walk = client->main->streams; walk; walk = walk->next) {
    if (walk == NULL)
      break;
    bitrates =
        g_list_append (bitrates,
        GUINT_TO_POINTER (GST_M3U8_STREAM (walk->data)->bandwidth));
  }
  GST_M3U8_CLIENT_UNLOCK (client);

  return bitrates;
}

gboolean
gst_m3u8_client_video_stream_info (GstM3U8Client * client, gchar * name,
    guint * bitrate, gchar ** title)
{
  gboolean ret = FALSE;

  GST_M3U8_CLIENT_LOCK (client);

  if (client->selected_stream == NULL)
    goto exit;

  if (bitrate)
    *bitrate = client->selected_stream->bandwidth;

  if (client->video_alternate != NULL) {
    GstM3U8Media *media;

    media = g_hash_table_lookup (client->selected_stream->video_alternates,
        name);
    if (media != NULL) {
      if (title != NULL)
        *title = g_strdup (media->name);
    }
  }
  ret = TRUE;

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

gboolean
gst_m3u8_client_audio_stream_info (GstM3U8Client * client, gchar * name,
    gchar ** lang, gchar ** title)
{
  gboolean ret = FALSE;

  GST_M3U8_CLIENT_LOCK (client);

  if (client->selected_stream == NULL ||
      client->selected_stream->audio_alternates == NULL)
    goto exit;

  if (client->audio_alternate != NULL) {
    GstM3U8Media *media;

    media = g_hash_table_lookup (client->selected_stream->audio_alternates,
        name);
    if (media != NULL) {
      if (lang != NULL)
        *lang = g_strdup (media->language);
      if (title != NULL) {
        *title = g_strdup (media->name);
      }
    }
  }
  ret = TRUE;

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

gboolean
gst_m3u8_client_subs_stream_info (GstM3U8Client * client, gchar * name,
    gchar ** lang, gchar ** title)
{
  gboolean ret = FALSE;
  GstM3U8Media *media;

  GST_M3U8_CLIENT_LOCK (client);

  if (client->selected_stream == NULL ||
      client->selected_stream->subtt_alternates == NULL)
    goto exit;

  media = g_hash_table_lookup (client->selected_stream->subtt_alternates, name);
  if (media != NULL) {
    if (lang != NULL)
      *lang = g_strdup (media->language);
    if (title != NULL)
      *title = g_strdup (media->name);
  }
  ret = TRUE;

exit:
  GST_M3U8_CLIENT_UNLOCK (client);
  return ret;
}

guint64
gst_m3u8_client_get_current_fragment_duration (GstM3U8Client * client)
{
  guint64 dur;
  GstM3U8Playlist *pl;
  GList *list;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->selected_stream != NULL, FALSE);

  GST_M3U8_CLIENT_LOCK (client);

  pl = gst_m3u8_client_get_current_playlist (client);
  list = g_list_find_custom (pl->files, GUINT_TO_POINTER (client->sequence - 1),
      (GCompareFunc) _find_next);
  if (list == NULL) {
    dur = -1;
  } else {
    dur = GST_M3U8_MEDIA_FILE (list->data)->duration;
  }

  GST_M3U8_CLIENT_UNLOCK (client);
  return dur;
}
