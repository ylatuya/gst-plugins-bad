/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstmediamanager.h:
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


#ifndef __GST_DASH_MANAGER_H__
#define __GST_DASH_MANAGER_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/baseadaptive/gststreamsmanager.h>
#include "mdp/gstmediapresentation.h"

G_BEGIN_DECLS
#define GST_TYPE_DASH_MANAGER \
  (gst_dash_manager_get_type())
#define GST_DASH_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DASH_MANAGER,GstDashManager))
#define GST_DASH_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DASH_MANAGER, GstDashManagerClass))
#define GST_DASH_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DASH_MANAGER,GstDashManagerClass))
#define GST_IS_DASH_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DASH_MANAGER))
#define GST_IS_DASH_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DASH_MANAGER))
#define GST_DASH_MANAGER_CAST(obj) \
  ((GstDashManager *) (obj))

GType gst_dash_manager_get_type (void);

typedef struct _GstDashManager GstDashManager;
typedef struct _GstDashManagerPrivate GstDashManagerPrivate;
typedef struct _GstDashManagerClass GstDashManagerClass;

/**
 * GstDashManager:
 */
struct _GstDashManager
{
  GstStreamsManager parent;

  GstMediaPresentation *mdp;
  guint64 min_buffer_time;
};

/**
 * GstDashManagerClass:
 */
struct _GstDashManagerClass
{
  GstStreamsManagerClass parent;

  gpointer _gst_reserved[GST_PADDING_LARGE];
};


GstDashManager * gst_dash_manager_new (gboolean is_live,
                                       gboolean chunked,
                                       guint64 min_buffer_time);

#endif /* __GST_DASH_MANAGER_H__ */
