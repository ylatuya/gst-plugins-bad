/* GStreamer
 * Copyright (C) 2012 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstm3u8manager.h:
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


#ifndef __GST_M3U8_MANAGER_H__
#define __GST_M3U8_MANAGER_H__

#include <glib.h>
#include <gst/gst.h>
#include "gstm3u8playlist.h"
#include "gst/baseadaptive/gststreamsmanager.h"

G_BEGIN_DECLS
#define GST_TYPE_M3U8_MANAGER \
  (gst_m3u8_manager_get_type())
#define GST_M3U8_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_M3U8_MANAGER,GstM3u8Manager))
#define GST_M3U8_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_M3U8_MANAGER, GstM3u8ManagerClass))
#define GST_M3U8_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_M3U8_MANAGER,GstM3u8ManagerClass))
#define GST_IS_M3U8_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_M3U8_MANAGER))
#define GST_IS_M3U8_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_M3U8_MANAGER))
#define GST_M3U8_MANAGER_CAST(obj) \
  ((GstM3u8Manager *) (obj))
GType gst_m3u8_manager_get_type (void);

typedef struct _GstM3u8Manager GstM3u8Manager;
typedef struct _GstM3u8ManagerPrivate GstM3u8ManagerPrivate;
typedef struct _GstM3u8ManagerClass GstM3u8ManagerClass;

/**
 * GstM3u8Manager:
 */
struct _GstM3u8Manager
{
  GstStreamsManager parent;

  GstM3U8VariantPlaylist *variant_playlist;
  guint max_version;
  gboolean allow_cache;
};

/**
 * GstM3u8ManagerClass:
 */
struct _GstM3u8ManagerClass
{
  GstStreamsManagerClass parent;
};

GstM3u8Manager * gst_m3u8_manager_new (gchar *base_url, gchar *title, gchar *fragment_prefix,
                                       gchar *output_directory, gboolean is_live, gboolean chunked,
                                       guint64 window_size, gchar *variant_pl_name, guint max_version,
                                       gboolean allow_cache);

#endif /* __GST_M3U8_MANAGER_H__ */
