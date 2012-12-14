/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * m3u8.h:
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

#ifndef __M3U8_H__
#define __M3U8_H__

#include <glib.h>
#include "gstfragment.h"

G_BEGIN_DECLS
typedef struct _GstM3U8 GstM3U8;
typedef struct _GstM3U8Playlist GstM3U8Playlist;
typedef struct _GstM3U8VariantPlaylist GstM3U8VariantPlaylist;
typedef struct _GstM3U8Stream GstM3U8Stream;
typedef struct _GstM3U8Media GstM3U8Media;
typedef struct _GstM3U8MediaFile GstM3U8MediaFile;
typedef struct _GstM3U8Client GstM3U8Client;

#define GST_M3U8(m) ((GstM3U8*)m)
#define GST_M3U8_PLAYLIST(m) ((GstM3U8Playlist*)m)
#define GST_M3U8_VARIANT_PLAYLIST(m) ((GstM3U8VariantPlaylist*)m)
#define GST_M3U8_STREAM(m) ((GstM3U8Stream*)m)
#define GST_M3U8_MEDIA(m) ((GstM3U8Media*)m)
#define GST_M3U8_MEDIA_FILE(f) ((GstM3U8MediaFile*)f)

#define GST_M3U8_CLIENT_LOCK(c) g_mutex_lock (c->lock);
#define GST_M3U8_CLIENT_UNLOCK(c) g_mutex_unlock (c->lock);

typedef enum
{
  GST_M3U8_MEDIA_TYPE_AUDIO,
  GST_M3U8_MEDIA_TYPE_VIDEO,
  GST_M3U8_MEDIA_TYPE_SUBTITLES,
} GstM3U8MediaType;

typedef enum
{
  GST_M3U8_MEDIA_CODEC_GENERIC_AUDIO,            /* mp4a */
  GST_M3U8_MEDIA_CODEC_AAC_LC,                   /* mp4a.40.2 */
  GST_M3U8_MEDIA_CODEC_HE_AAC,                   /* mp4a.40.5 */
  GST_M3U8_MEDIA_CODEC_MP3,                      /* mp4a.40.34 */
  GST_M3U8_MEDIA_CODEC_GENERIC_H264,             /* avc1 */
  GST_M3U8_MEDIA_CODEC_H264_BASE,                /* avc1.42e0XX */
  GST_M3U8_MEDIA_CODEC_H264_MAIN,                /* avc1.4d40XX */
  GST_M3U8_MEDIA_CODEC_H264_HIGH,                /* avc1.6400XX */
  GST_M3U8_MEDIA_CODEC_NONE,
} GstM3U8MediaCodec;

struct _GstM3U8
{
  gchar *uri;
};

struct _GstM3U8Playlist
{
  GstM3U8 base;

  gboolean endlist;             /* if ENDLIST has been reached */
  gint version;                 /* last EXT-X-VERSION */
  GstClockTime targetduration;  /* last EXT-X-TARGETDURATION */
  gchar *allowcache;            /* last EXT-X-ALLOWCACHE */

  GList *files;

  /*< private > */
  gchar *last_data;
  guint mediasequence;          /* EXT-X-MEDIA-SEQUENCE & increased with new media file */
};

struct _GstM3U8Media
{
  GstM3U8MediaType media_type;
  gchar *group_id;
  gchar *name;
  gchar *language;
  gchar *uri;
  gboolean is_default;
  gboolean autoselect;
  GstM3U8Playlist *playlist;
};

struct _GstM3U8Stream
{
  gint bandwidth;
  gint program_id;
  GstM3U8MediaCodec video_codec;
  GstM3U8MediaCodec audio_codec;
  gint width;
  gint height;

  gchar * default_audio;
  gchar * default_video;
  GHashTable *video_alternates;     /* Key:Name Value:GstM3U8Media* */
  GHashTable *audio_alternates;     /* Key:Name Value:GstM3U8Media* */
  GstM3U8Playlist *selected_video;
  GstM3U8Playlist *selected_audio;
};

struct _GstM3U8VariantPlaylist
{
  GstM3U8 base;

  GList *streams;                      /* list of GstM3U8Stream* in the main playlist */
  GHashTable *video_rendition_groups;  /* Key:Group-ID Value:GstM3U8Media* */
  GHashTable *audio_rendition_groups;  /* Key:Group-ID Value:GstM3U8Media* */

  GList *playlists;                /* Internal list of playlists ceated to be freed */
};

struct _GstM3U8MediaFile
{
  gchar *title;
  GstClockTime duration;
  gchar *uri;
  guint sequence;               /* the sequence nb of this file */
  gint64 offset;                /* offset for byte-ranges */
  gint64 length;                /* length for byte-ranges */
};

struct _GstM3U8Client
{
  GstM3U8VariantPlaylist *main;    /* main playlist */
  GstM3U8Stream *selected_stream;  /* currently selected stream */
  gchar *video_alternate;          /* selected video alternate */
  gchar *audio_alternate;          /* selected audio alternate */
  guint update_failed_count;
  gint sequence;                   /* the next sequence for this client */
  GMutex *lock;
};


G_BEGIN_DECLS

GstM3U8Client *gst_m3u8_client_new                    (const gchar * uri);

void gst_m3u8_client_free                             (GstM3U8Client * client);

gboolean gst_m3u8_client_parse_main_playlist          (GstM3U8Client * self,
                                                       gchar *data);

gboolean gst_m3u8_client_update                       (GstM3U8Client * client,
                                                       gchar * video_pl,
                                                       gchar *audio_pl);

void gst_m3u8_client_set_current                      (GstM3U8Client * client,
                                                       GstM3U8Stream * m3u8);

gboolean gst_m3u8_client_get_next_fragment            (GstM3U8Client * client,
                                                       GstFragment **video_fragment,
                                                       GstFragment **audio_fragment);

void gst_m3u8_client_get_current_position             (GstM3U8Client * client,
                                                       GstClockTime * timestamp);

GstClockTime gst_m3u8_client_get_duration             (GstM3U8Client * client);

GstClockTime gst_m3u8_client_get_target_duration      (GstM3U8Client * client);

const gchar *gst_m3u8_client_get_uri                  (GstM3U8Client * client);

void gst_m3u8_client_get_current_uri                  (GstM3U8Client * client,
                                                       const gchar **video_uri,
                                                       const gchar **audio_uri);

gboolean gst_m3u8_client_is_live                      (GstM3U8Client * client);

gboolean gst_m3u8_client_check_sequence_validity      (GstM3U8Client * client);

GstM3U8Stream * gst_m3u8_client_get_stream_for_bitrate (GstM3U8Client * client,
                                                        guint bitrate);

GList * gst_m3u8_client_get_streams_bitrates           (GstM3U8Client *client);

GstM3U8Stream * gst_m3u8_client_get_previous_stream    (GstM3U8Client * client);

GstM3U8Stream * gst_m3u8_client_get_next_stream        (GstM3U8Client * client);

gboolean gst_m3u8_client_seek                         (GstM3U8Client * client,
                                                       gint64 seek_time);

GList * gst_m3u8_client_get_alternates                (GstM3U8Client *client,
                                                       GstM3U8MediaType media_type);

gboolean gst_m3u8_client_set_alternate                (GstM3U8Client *client,
                                                       GstM3U8MediaType media_type,
                                                       const gchar *name);

gboolean gst_m3u8_client_video_stream_info            (GstM3U8Client *client, gchar * name,
                                                       guint *bitrate, gchar **title);

gboolean gst_m3u8_client_audio_stream_info            (GstM3U8Client *client, gchar * name,
                                                       gchar **lang, gchar **title);

G_END_DECLS
#endif /* __M3U8_H__ */
