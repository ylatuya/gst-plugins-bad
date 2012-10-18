/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstbaseadaptivesink.h:
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

#ifndef __GST_BASE_ADAPTIVE_SINK_H__
#define __GST_BASE_ADAPTIVE_SINK_H__

#include <gst/gst.h>
#include <gio/gio.h>
#include <errno.h>

#include "gstfragment.h"
#include "gststreamsmanager.h"

G_BEGIN_DECLS

#define GST_TYPE_BASE_ADAPTIVE_SINK \
  (gst_base_adaptive_sink_get_type())
#define GST_BASE_ADAPTIVE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_ADAPTIVE_SINK,GstBaseAdaptiveSink))
#define GST_BASE_ADAPTIVE_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_BASE_ADAPTIVE_SINK, GstBaseAdaptiveSinkClass))
#define GST_BASE_ADAPTIVE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_ADAPTIVE_SINK,GstBaseAdaptiveSinkClass))
#define GST_IS_BASE_ADAPTIVE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_ADAPTIVE_SINK))
#define GST_IS_BASE_ADAPTIVE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_ADAPTIVE_SINK))
GType gst_base_adaptive_sink_get_type (void);

typedef struct _GstBaseAdaptiveSink GstBaseAdaptiveSink;
typedef struct _GstBaseAdaptiveSinkClass GstBaseAdaptiveSinkClass;
typedef struct _GstBaseAdaptiveSinkClass GstBaseAdaptiveSinkClass;


struct _GstBaseAdaptiveSink
{
  GstElement parent;

  /* Properties */
  gboolean chunked;                /* True if streams's fragments are chunked in files */
  gboolean is_live;                /* Whether it's a live stream or an on-demand stream */
  gboolean write_to_disk;          /* True if fragments should be written to disk */
  gboolean delete_old_files;       /* True if fragments that are not valid anymore should ne deleted */
  gboolean prepend_headers;        /* Prepend headers to each fragment */
  gboolean needs_init_segment;     /* True if the initialization segment must be saved in a separate file */
  GstClockTime fragment_duration;  /* Duration of fragments in seconds */
  GstClockTime max_window;         /* Maximum window for DVR in seconds */
  gchar *md_name;                  /* name for the media representation file */
  gchar *base_url;                 /* base url for accessing fragments */
  gchar *output_directory;         /* output directory for new fragments and media representation files */
  gchar *fragment_prefix;          /* prefix used for naming fragments */
  const gchar *fragment_tpl;       /* Filename template for fragments */

  /*< protected >*/
  gboolean append_headers;    /* True if the headers should be appended to each fragment */
  GstCaps *supported_caps;    /* Supported streams caps */
  guint min_cache;            /* Minimum number of fragments to cache */

  /*< private >*/
  GstStreamsManager *streams_manager;
  GHashTable * pad_datas;
};

struct _GstBaseAdaptiveSinkClass
{
  GstElementClass parent_class;

  GstStreamsManager * (*create_streams_manager)  (GstBaseAdaptiveSink *sink);

  /* signals */
  void                (*eos)                     (GstBaseAdaptiveSink *sink);

  void                (*new_fragment)            (GstBaseAdaptiveSink *sink);

  void                (*new_playlist)            (GstBaseAdaptiveSink *sink);

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};


void gst_base_adaptive_sink_set_output_directory (GstBaseAdaptiveSink *sink, const gchar *output_dir);

void gst_base_adaptive_sink_set_md_name (GstBaseAdaptiveSink *sink, const gchar *name);

void gst_base_adaptive_sink_set_base_url (GstBaseAdaptiveSink *sink, const gchar *base_url);

void gst_base_adaptive_sink_set_fragment_prefix (GstBaseAdaptiveSink *sink, const gchar *fragment_prefix);

G_END_DECLS

#endif /* __GST_BASE_ADAPTIVE_SINK_H__ */
