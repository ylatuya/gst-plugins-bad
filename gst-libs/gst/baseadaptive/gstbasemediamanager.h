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


#ifndef __GST_BASE_MEDIA_MANAGER_H__
#define __GST_BASE_MEDIA_MANAGER_H__

#ifndef GST_USE_UNSTABLE_API
#warning "GstBaseMediaManager is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <glib.h>
#include <gst/gst.h>
#include "gstfragment.h"

G_BEGIN_DECLS
#define GST_TYPE_BASE_MEDIA_MANAGER \
  (gst_base_media_manager_get_type())
#define GST_BASE_MEDIA_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_MEDIA_MANAGER,GstBaseMediaManager))
#define GST_BASE_MEDIA_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_BASE_MEDIA_MANAGER, GstBaseMediaManagerClass))
#define GST_BASE_MEDIA_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_MEDIA_MANAGER,GstBaseMediaManagerClass))
#define GST_IS_BASE_MEDIA_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_MEDIA_MANAGER))
#define GST_IS_BASE_MEDIA_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_MEDIA_MANAGER))
#define GST_BASE_MEDIA_MANAGER_CAST(obj) \
  ((GstBaseMediaManager *) (obj))
GType gst_base_media_manager_get_type (void);

typedef struct _GstBaseMediaManager GstBaseMediaManager;
typedef struct _GstBaseMediaManagerPrivate GstBaseMediaManagerPrivate;
typedef struct _GstBaseMediaManagerClass GstBaseMediaManagerClass;
typedef struct _GstMediaManagerFile GstMediaManagerFile;
typedef struct _GstPeriod GstPeriod;
typedef struct _GstRemanager GstRemanager;
typedef struct _GstRemanagerSegment GstRemanagerSegment;

/**
 * GstMediaManagerFile:
 */
struct _GstMediaManagerFile
{
  gchar * filename;
  gchar * content;
};

/**
 * GstBaseMediaManager:
 */
struct _GstBaseMediaManager
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
 * GstBaseMediaManagerClass:
 */
struct _GstBaseMediaManagerClass
{
  GObjectClass parent;

  gboolean             (*add_stream)        (GstBaseMediaManager *self, GstPad *pad, GList *substreams_caps);
  gboolean             (*remove_stream)     (GstBaseMediaManager *self, GstPad *pad);
  gboolean             (*add_fragment)      (GstBaseMediaManager *self, GstPad *pad, GstFragment *fragment, GList **removed_fragments);
  gboolean             (*add_headers)       (GstBaseMediaManager *self, GstPad *pad, GstFragment *fragment);
  GstMediaManagerFile* (*render)            (GstBaseMediaManager *self, GstPad *pad);
  void                 (*clear)             (GstBaseMediaManager *self);

  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GstBaseMediaManager * gst_base_media_manager_new (void);
gboolean gst_base_media_manager_add_stream (GstBaseMediaManager * media_manager, GstPad * pad, GList *substreams_caps);
gboolean gst_base_media_manager_remove_stream (GstBaseMediaManager * media_manager, GstPad * pad);
gboolean gst_base_media_manager_add_headers (GstBaseMediaManager * media_manager, GstPad * pad, GstFragment *fragment);
gboolean gst_base_media_manager_add_fragment (GstBaseMediaManager * media_manager,
                                              GstPad * pad, GstFragment *fragment,
                                              GList **removed_fragments);
GstMediaManagerFile * gst_base_media_manager_render (GstBaseMediaManager * media_manager, GstPad *pad);
void gst_base_media_manager_set_base_url (GstBaseMediaManager * media_manager, gchar * base_url);
void gst_base_media_manager_set_title (GstBaseMediaManager * media_manager, gchar * title);
void gst_base_media_manager_set_fragment_prefix (GstBaseMediaManager * media_manager, gchar * fragment_prefix);
void gst_base_media_manager_clear (GstBaseMediaManager * media_manager);
void gst_base_media_manager_file_free (GstMediaManagerFile * file);

#endif /* __GST_BASE_MEDIA_MANAGER_H__ */
