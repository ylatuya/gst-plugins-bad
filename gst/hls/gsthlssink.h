/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gsthlssink.h:
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

#ifndef __GST_HLSSINK_H__
#define __GST_HLSSINK_H__

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <errno.h>

#include "gstfragment.h"
#include "gstm3u8playlist.h"

G_BEGIN_DECLS

#define GST_TYPE_HLS_SINK \
  (gst_hls_sink_get_type())
#define GST_HLS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HLS_SINK,GstHlsSink))
#define GST_HLS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HLS_SINK,GstHlsSinkClass))
#define GST_IS_HLS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HLS_SINK))
#define GST_IS_HLS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HLS_SINK))

typedef struct _GstHlsSink GstHlsSink;
typedef struct _GstHlsSinkClass GstHlsSinkClass;

struct _GstHlsSink
{
  GstBaseSink parent;

  GstM3U8Playlist *playlist;
  gchar * playlist_filepath;
  gboolean new_fragment;
  guint new_fragment_index;
  GstClockTime new_fragment_ts;
  GstFragment *fragment;
  GstFragment *last_fragment;
  GCancellable *fragment_cancellable;

  /* Properties */
  gchar *output_directory;
  gchar *fragment_prefix;
  gchar *playlist_name;
  gchar *playlist_url_prefix;
  gchar *title;
  gboolean write_to_disk;
  gboolean delete_old_files;

  int n_streamheaders;
  GstBuffer **streamheaders;
};

struct _GstHlsSinkClass
{
  GstBaseSinkClass parent_class;

  /* signals */
  void        (*eos)            (GstHlsSink *sink);
  void        (*new_fragment)   (GstHlsSink *sink);
  void        (*new_playlist)   (GstHlsSink *sink);

  /* actions */
  gchar *         (*pull_playlist)  (GstHlsSink *sink);
  GstFragment *   (*pull_fragment)  (GstHlsSink *sink);

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};

GType gst_hls_sink_get_type (void);

G_END_DECLS

#endif /* __GST_HLSSINK_H__ */
