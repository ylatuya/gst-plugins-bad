/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstkeyunitsscheduler.h:
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


#ifndef __GST_KEY_UNITS_SCHEDULER_H__
#define __GST_KEY_UNITS_SCHEDULER_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS
#define GST_TYPE_KEY_UNITS_SCHEDULER \
  (gst_key_units_scheduler_get_type())
#define GST_KEY_UNITS_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KEY_UNITS_SCHEDULER,GstKeyUnitsScheduler))
#define GST_KEY_UNITS_SCHEDULER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KEY_UNITS_SCHEDULER,GstKeyUnitsSchedulerClass))
#define GST_IS_KEY_UNITS_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KEY_UNITS_SCHEDULER))
#define GST_IS_KEY_UNITS_SCHEDULER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KEY_UNITS_SCHEDULER))
typedef struct _GstKeyUnitsScheduler GstKeyUnitsScheduler;
typedef struct _GstKeyUnitsSchedulerClass GstKeyUnitsSchedulerClass;

/**
 * GstKeyUnitsScheduler:
 *
 * Opaque #GstKeyUnitsScheduler data structure.
 */
struct _GstKeyUnitsScheduler
{
  GstBaseTransform parent;

  /* Properties */
  gint64 interval;

  guint count;
  GstClockTime last_ts;
};

struct _GstKeyUnitsSchedulerClass
{
  GstBaseTransformClass parent_class;
};

GType gst_key_units_scheduler_get_type (void);

G_END_DECLS
#endif /* __GST_KEY_UNITS_SCHEDULER_H__ */
