/* GStreamer
 * Copyright (C) 2012 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstfragment.h:
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

#ifndef __GSTFRAGMENT_H__
#define __GSTFRAGMENT_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_FRAGMENT_META_API_TYPE (gst_fragment_meta_api_get_type())
#define gst_buffer_get_fragment_meta(b) \
  ((GstFragmentMeta*)gst_buffer_get_meta((b), GST_FRAGMENT_META_API_TYPE))

typedef struct _GstFragmentMeta GstFragmentMeta;

struct _GstFragmentMeta
{
  GstMeta parent;

  gchar * name;                 /* Name of the fragment */
  gboolean completed;           /* Whether the fragment is complete or not */
  guint64 download_start_time;  /* Epoch time when the download started */
  guint64 download_stop_time;   /* Epoch time when the download finished */
  gboolean index;               /* Index of the fragment */
  gboolean discontinuous;       /* Whether this fragment is discontinuous or not */
  GFile *file;                  /* File where this fragment is stored in disk */
  GstBuffer *headers;
};

GType gst_fragment_meta_api_get_type (void);

gboolean gst_fragment_set_headers (GstBuffer *fragment, GstBuffer *headers);
gboolean gst_fragment_add_buffer (GstBuffer *fragment, GstBuffer *buffer);
void gst_fragment_set_name (GstBuffer *buffer, gchar *name);
void gst_fragment_set_file (GstBuffer *fragment, GFile *file);
GstBuffer * gst_fragment_new (void);

G_END_DECLS
#endif /* __GSTFRAGMENT_H__ */
