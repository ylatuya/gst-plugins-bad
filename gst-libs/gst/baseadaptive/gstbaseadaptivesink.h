/* GStreamer
 * Copyright (C) 2011 Flumotion S.L.
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
#include "gstbasemediamanager.h"

G_BEGIN_DECLS

#define GST_TYPE_BASE_ADAPTIVE_SINK \
  (gst_base_adaptive_sink_get_type())
#define GST_BASE_ADAPTIVE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_ADAPTIVE_SINK,GstBaseAdaptiveSink))
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
  gboolean chunked;
  gboolean is_live;
  gboolean write_to_disk;
  gboolean delete_old_files;
  gchar * md_name;
  gchar *base_url;
  gchar *output_directory;
  gchar *fragment_prefix;

  /*< protected >*/
  gboolean append_headers;
  GstCaps *supported_caps;

  /*< private >*/
  GstBaseMediaManager *media_manager;
  GHashTable * pad_datas;
};

struct _GstBaseAdaptiveSinkClass
{
  GstElementClass parent_class;

  GstBaseMediaManager * (*create_media_manager)  (GstBaseAdaptiveSink *sink);

  /* signals */
  void        (*eos)            (GstBaseAdaptiveSink *sink);
  void        (*new_fragment)   (GstBaseAdaptiveSink *sink);
  void        (*new_playlist)   (GstBaseAdaptiveSink *sink);

  /*< private >*/
  gpointer     _gst_reserved[GST_PADDING];
};


G_END_DECLS

#endif /* __GST_BASE_ADAPTIVE_SINK_H__ */
