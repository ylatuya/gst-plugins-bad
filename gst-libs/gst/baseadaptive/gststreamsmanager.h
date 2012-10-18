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
typedef struct _GstMediaRepFile GstMediaRepFile;

/**
 * GstMediaRepFile:
 */
struct _GstMediaRepFile
{
  GFile * file;
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
  gchar * output_directory;
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

  gboolean    (*add_stream)      (GstStreamsManager *self,
                                  GstPad *pad, guint avg_bitrate,
                                  GList *substreams_caps,
                                  GstMediaRepFile **rep_file);

  gboolean    (*remove_stream)   (GstStreamsManager *self,
                                  GstPad *pad,
                                  GstMediaRepFile **rep_file);

  gboolean    (*add_fragment)    (GstStreamsManager *self,
                                  GstPad *pad, GstBuffer *fragment,
                                  GstMediaRepFile **rep_file,
                                  GList **removed_fragments);

  gboolean    (*eos)             (GstStreamsManager *self,
                                  GstPad *pad,
                                  GstMediaRepFile **rep_file);

  gboolean    (*add_headers)     (GstStreamsManager *self, GstPad *pad,
                                  GstBuffer *fragment);

  gboolean    (*render)          (GstStreamsManager *self, GstPad *pad,
                                  GstMediaRepFile **rep_file);

  void        (*clear)           (GstStreamsManager *self);

  gpointer _gst_reserved[GST_PADDING_LARGE];
};


GstStreamsManager * gst_streams_manager_new               (void);

gboolean          gst_streams_manager_add_stream          (GstStreamsManager * man,
                                                           GstPad * pad, guint avg_bitrate,
                                                           GList *streams_caps,
                                                           GstMediaRepFile **rep_file);

gboolean          gst_streams_manager_remove_stream       (GstStreamsManager * man,
                                                           GstPad * pad,
                                                           GstMediaRepFile **rep_file);

gboolean          gst_streams_manager_eos                 (GstStreamsManager * man,
                                                           GstPad * pad,
                                                           GstMediaRepFile **rep_file);

gboolean          gst_streams_manager_add_headers         (GstStreamsManager * man,
                                                           GstPad * pad, GstBuffer *frag);

gboolean          gst_streams_manager_add_fragment        (GstStreamsManager * man,
                                                           GstPad * pad, GstBuffer *frag,
                                                           GstMediaRepFile **rep_file,
                                                           GList **removed_fragments);

gboolean          gst_streams_manager_render              (GstStreamsManager * man,
                                                           GstPad *pad,
                                                           GstMediaRepFile ** file);

void              gst_streams_manager_set_base_url        (GstStreamsManager * man,
                                                           gchar * base_url);

void              gst_streams_manager_set_title           (GstStreamsManager * man,
                                                           gchar * title);

void              gst_streams_manager_clear               (GstStreamsManager * man);



void              gst_streams_manager_set_output_directory (GstStreamsManager * man,
                                                            gchar * output_directory);

void              gst_streams_manager_set_fragment_prefix  (GstStreamsManager * man,
                                                            gchar * prefix);

GstMediaRepFile * gst_media_rep_file_new                   (gchar *content,
                                                            GFile * file);

void              gst_media_rep_file_free                  (GstMediaRepFile * file);

#endif /* __GST_STREAMS_MANAGER_H__ */
