/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstbasemediamanager.h:
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


#ifndef __GST_STREAMS_MANAGER_H__
#define __GST_STREAMS_MANAGER_H__

#ifndef GST_USE_UNSTABLE_API
#warning "GstStreamsManager is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <glib.h>
#include <gst/gst.h>
#include "gstfragment.h"

G_BEGIN_DECLS
#define GST_TYPE_STREAMS_MANAGER \
  (gst_streams_manager_get_type())
#define GST_STREAMS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_STREAMS_MANAGER,GstStreamsManager))
#define GST_STREAMS_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_STREAMS_MANAGER, GstStreamsManagerClass))
#define GST_STREAMS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_STREAMS_MANAGER,GstStreamsManagerClass))
#define GST_IS_STREAMS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_STREAMS_MANAGER))
#define GST_IS_STREAMS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_STREAMS_MANAGER))
#define GST_STREAMS_MANAGER_CAST(obj) \
  ((GstStreamsManager *) (obj))
GType gst_streams_manager_get_type (void);

typedef struct _GstStreamsManager GstStreamsManager;
typedef struct _GstStreamsManagerPrivate GstStreamsManagerPrivate;
typedef struct _GstStreamsManagerClass GstStreamsManagerClass;
typedef struct _GstMediaRepresentationFile GstMediaRepresentationFile;
typedef struct _GstPeriod GstPeriod;
typedef struct _GstRemanager GstRemanager;
typedef struct _GstRemanagerSegment GstRemanagerSegment;

/**
 * GstMediaRepresentationFile:
 */
struct _GstMediaRepresentationFile
{
  gchar * filename;
  gchar * content;
};

/**
 * GstStreamsManager:
 */
struct _GstStreamsManager
{
  GObject parent;

  /* Properties */
  gchar * base_url;
  gchar * title;
  gchar * fragment_prefix;
  gboolean is_live;
  gboolean chunked;
  gboolean finished;
  gboolean with_headers;
  guint64 window_size;

  GMutex *lock;
};

/**
 * GstStreamsManagerClass:
 */
struct _GstStreamsManagerClass
{
  GObjectClass parent;

  gboolean             (*add_stream)        (GstStreamsManager *self, GstPad *pad, GList *substreams_caps);
  gboolean             (*remove_stream)     (GstStreamsManager *self, GstPad *pad);
  gboolean             (*add_fragment)      (GstStreamsManager *self, GstPad *pad, GstBuffer *fragment, GList **removed_fragments);
  gboolean             (*add_headers)       (GstStreamsManager *self, GstPad *pad, GstBuffer *fragment);
  GstMediaRepresentationFile* (*render)            (GstStreamsManager *self, GstPad *pad);
  void                 (*clear)             (GstStreamsManager *self);

  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GstStreamsManager * gst_streams_manager_new (void);
gboolean gst_streams_manager_add_stream (GstStreamsManager * streams_manager, GstPad * pad, GList *substreams_caps);
gboolean gst_streams_manager_remove_stream (GstStreamsManager * streams_manager, GstPad * pad);
gboolean gst_streams_manager_add_headers (GstStreamsManager * streams_manager, GstPad * pad, GstBuffer *fragment);
gboolean gst_streams_manager_add_fragment (GstStreamsManager * streams_manager,
                                              GstPad * pad, GstBuffer *fragment,
                                              GList **removed_fragments);
GstMediaRepresentationFile * gst_streams_manager_render (GstStreamsManager * streams_manager, GstPad *pad);
void gst_streams_manager_set_base_url (GstStreamsManager * streams_manager, gchar * base_url);
void gst_streams_manager_set_title (GstStreamsManager * streams_manager, gchar * title);
void gst_streams_manager_set_fragment_prefix (GstStreamsManager * streams_manager, gchar * fragment_prefix);
void gst_streams_manager_clear (GstStreamsManager * streams_manager);
void gst_media_representation_file_free (GstMediaRepresentationFile * file);

#endif /* __GST_STREAMS_MANAGER_H__ */
