/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstm3u8playlist.h:
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

#ifndef __GST_M3U8_PLAYLIST_H__
#define __GST_M3U8_PLAYLIST_H__

#include <glib.h>
#include <gio/gio.h>
#include <gst/baseadaptive/gstbaseplaylist.h>

G_BEGIN_DECLS

typedef struct _GstM3U8Playlist GstM3U8Playlist;
typedef struct _GstM3U8Entry GstM3U8Entry;
typedef struct _GstM3U8PlaylistClass GstM3U8PlaylistClass;

#define GST_TYPE_M3U8_PLAYLIST \
  (gst_base_playlist_get_type())
#define GST_M3U8_PLAYLIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_M3U8_PLAYLIST,GstM3U8Playlist))
#define GST_M3U8_PLAYLIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_M3U8_PLAYLIST, GstM3U8PlaylistClass))
#define GST_M3U8_PLAYLIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_M3U8_PLAYLIST,GstM3U8PlaylistClass))
#define GST_IS_M3U8_PLAYLIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_M3U8_PLAYLIST))
#define GST_IS_M3U8_PLAYLIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_M3U8_PLAYLIST))
#define GST_M3U8_PLAYLIST_CAST(obj) \
  ((GstM3U8Playlist *) (obj))

GType gst_m3u8_playlist_get_type (void);

struct _GstM3U8Playlist
{
  GstBasePlaylist parent;

  guint version;
  gboolean allow_cache;
  gint type;
  guint sequence_number;
};

struct _GstM3U8PlaylistClass
{
  GObjectClass parent_class;
};

GstM3U8Playlist * gst_m3u8_playlist_new (guint version, guint window_size,
                                         gboolean allow_cache);
guint gst_m3u8_playlist_n_entries (GstM3U8Playlist * playlist);

G_END_DECLS
#endif
