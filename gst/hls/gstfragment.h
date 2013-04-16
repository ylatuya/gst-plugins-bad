/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2013 SoftAtHome. All rights reserved.
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
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_FRAGMENT (gst_fragment_get_type())
#define GST_FRAGMENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FRAGMENT,GstFragment))
#define GST_FRAGMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FRAGMENT,GstFragmentClass))
#define GST_IS_FRAGMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FRAGMENT))
#define GST_IS_FRAGMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FRAGMENT))

typedef struct _GstFragment GstFragment;
typedef struct _GstFragmentPrivate GstFragmentPrivate;
typedef struct _GstFragmentClass GstFragmentClass;

typedef enum
{
  GST_FRAGMENT_ENCODING_METHOD_NONE,
  GST_FRAGMENT_ENCODING_METHOD_AES_128,
  GST_FRAGMENT_ENCODING_METHOD_SAMPLE_AES_128,
} GstFragmentEncodingMethod;

struct _GstFragment
{
  GObject parent;

  gchar * name;                 /* Name of the fragment */
  gboolean completed;           /* Whether the fragment is complete or not */
  guint64 download_start_time;  /* Epoch time when the download started */
  guint64 download_stop_time;   /* Epoch time when the download finished */
  guint64 start_time;           /* Start time of the fragment */
  guint64 stop_time;            /* Stop time of the fragment */
  gboolean index;               /* Index of the fragment */
  gboolean discontinuous;       /* Whether this fragment is discontinuous or not */
  guint64 offset;               /* Offset of this fragment */
  guint64 length;               /* Length of this fragment */
  gint32 enc_method;            /* Encoding method for this fragment */

  /* For AES-128 encrypted fragments */
  gchar *key_url;               /* URL for the key */
  gchar *iv;                    /* Initial vector */

  GstFragmentPrivate *priv;
};

struct _GstFragmentClass
{
  GObjectClass parent_class;
};

GType gst_fragment_get_type (void);

GstBufferList * gst_fragment_get_buffer_list (GstFragment *fragment);
gboolean gst_fragment_set_headers (GstFragment *fragment, GstBuffer **buffer, guint count);
gboolean gst_fragment_add_buffer (GstFragment *fragment, GstBuffer *buffer);
GstBuffer * gst_fragment_get_buffer (GstFragment *fragment);
gsize gst_fragment_get_total_size (GstFragment * fragment);
void gst_fragment_clear (GstFragment *fragment);
GstFragment * gst_fragment_new (void);

G_END_DECLS
#endif /* __GSTFRAGMENT_H__ */
