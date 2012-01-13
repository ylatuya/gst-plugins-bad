/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstfragment.c:
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

#include <glib.h>
#include <gst/gstbuffer.h>
#include <gst/gstbufferlist.h>
#include <string.h>
#include "gstadaptive.h"
#include "gstfragment.h"
#include "gstadaptive.h"


#define GST_FRAGMENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_FRAGMENT, GstFragmentPrivate))

enum
{
  PROP_0,
  PROP_INDEX,
  PROP_NAME,
  PROP_DURATION,
  PROP_DISCONTINOUS,
  PROP_BUFFER_LIST,
  PROP_BUFFER,
  PROP_LAST
};

struct _GstFragmentPrivate
{
  GstBufferList *buffer_list;
  GstBufferListIterator *buffer_iterator;
  gboolean can_set_headers;
  gboolean has_headers;
};

G_DEFINE_TYPE (GstFragment, gst_fragment, G_TYPE_OBJECT);

static void gst_fragment_dispose (GObject * object);
static void gst_fragment_finalize (GObject * object);

static void
gst_fragment_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GstFragment *fragment = GST_FRAGMENT (object);

  switch (property_id) {
    case PROP_INDEX:
      g_value_set_uint (value, fragment->index);
      break;

    case PROP_NAME:
      g_value_set_string (value, fragment->name);
      break;

    case PROP_DURATION:
      g_value_set_uint64 (value, fragment->stop_ts - fragment->start_ts);
      break;

    case PROP_DISCONTINOUS:
      g_value_set_boolean (value, fragment->discontinuous);
      break;

    case PROP_BUFFER_LIST:
      gst_value_set_mini_object (value,
          GST_MINI_OBJECT (gst_fragment_get_buffer_list (fragment)));
      break;

    case PROP_BUFFER:
      gst_value_set_mini_object (value,
          GST_MINI_OBJECT (gst_fragment_get_buffer (fragment)));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_fragment_class_init (GstFragmentClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstFragmentPrivate));

  gobject_class->get_property = gst_fragment_get_property;
  gobject_class->dispose = gst_fragment_dispose;
  gobject_class->finalize = gst_fragment_finalize;

  g_object_class_install_property (gobject_class, PROP_INDEX,
      g_param_spec_uint ("index", "Index", "Index of the fragment", 0,
          G_MAXUINT, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "Name",
          "Name of the fragment (eg:fragment-12.ts)", NULL, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_DISCONTINOUS,
      g_param_spec_boolean ("discontinuous", "Discontinous",
          "Whether this fragment has a discontinuity or not",
          FALSE, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_DURATION,
      g_param_spec_uint64 ("duration", "Fragment duration",
          "Duration of the fragment", 0, G_MAXUINT64, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_BUFFER_LIST,
      gst_param_spec_mini_object ("buffer-list", "Buffer List",
          "A list with the fragment's buffers", GST_TYPE_BUFFER_LIST,
          G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_BUFFER,
      gst_param_spec_mini_object ("buffer", "Buffer List",
          "A list with the fragment's buffers", GST_TYPE_BUFFER,
          G_PARAM_READABLE));
}

static void
gst_fragment_init (GstFragment * fragment)
{
  GstFragmentPrivate *priv;

  fragment->priv = priv = GST_FRAGMENT_GET_PRIVATE (fragment);
  fragment->priv->buffer_list = gst_buffer_list_new ();
  fragment->priv->buffer_iterator =
      gst_buffer_list_iterate (fragment->priv->buffer_list);
  gst_buffer_list_iterator_add_group (fragment->priv->buffer_iterator);
  fragment->priv->can_set_headers = TRUE;
  fragment->priv->has_headers = FALSE;
  fragment->download_start_time = g_get_real_time ();
  fragment->start_ts = 0;
  fragment->stop_ts = 0;
  fragment->offset = 0;
  fragment->size = 0;
  fragment->index = 0;
  fragment->name = g_strdup ("");
  fragment->completed = FALSE;
  fragment->discontinuous = FALSE;
}

GstFragment *
gst_fragment_new (void)
{
  return GST_FRAGMENT (g_object_new (GST_TYPE_FRAGMENT, NULL));
}

static void
gst_fragment_finalize (GObject * gobject)
{
  GstFragment *fragment = GST_FRAGMENT (gobject);

  g_free (fragment->name);

  G_OBJECT_CLASS (gst_fragment_parent_class)->finalize (gobject);
}

void
gst_fragment_dispose (GObject * object)
{
  GstFragment *fragment = GST_FRAGMENT (object);

  if (fragment->priv->buffer_list != NULL) {
    gst_buffer_list_iterator_free (fragment->priv->buffer_iterator);
    gst_buffer_list_unref (fragment->priv->buffer_list);
    fragment->priv->buffer_list = NULL;
  }

  G_OBJECT_CLASS (gst_fragment_parent_class)->dispose (object);
}

GstBufferList *
gst_fragment_get_buffer_list (GstFragment * fragment)
{
  g_return_val_if_fail (fragment != NULL, NULL);

  if (!fragment->completed)
    return NULL;

  gst_buffer_list_ref (fragment->priv->buffer_list);
  return fragment->priv->buffer_list;
}

GstBuffer *
gst_fragment_get_buffer (GstFragment * fragment)
{
  GstBufferListIterator *it;
  GstBuffer *buf;
  g_return_val_if_fail (fragment != NULL, NULL);

  if (!fragment->completed)
    return NULL;


  it = gst_buffer_list_iterate (fragment->priv->buffer_list);
  gst_buffer_list_iterator_next_group (it);
  buf = gst_buffer_list_iterator_merge_group (it);
  GST_BUFFER_DURATION (buf) = fragment->stop_ts - fragment->start_ts;
  GST_BUFFER_TIMESTAMP (buf) = fragment->start_ts;
  return buf;
}

gboolean
gst_fragment_set_headers (GstFragment * fragment, GstBuffer ** buffer,
    guint count)
{
  guint i;
  guint size = 0;
  GstBuffer *header;
  guint8 *ptr;

  g_return_val_if_fail (fragment != NULL, FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (count != 0, FALSE);

  if (!fragment->priv->can_set_headers)
    return FALSE;

  /* calculate the size of the merged buffer */
  for (i = 0; i < count; i++)
    size += GST_BUFFER_SIZE (buffer[i]);
  fragment->size += size;

  /* allocate a new buffer + */
  header = gst_buffer_new_and_alloc (size);

  /* copy all data buffers */
  ptr = GST_BUFFER_DATA (header);
  for (i = 0; i < count; i++) {
    memcpy (ptr, GST_BUFFER_DATA (buffer[i]), GST_BUFFER_SIZE (buffer[i]));
    ptr += GST_BUFFER_SIZE (buffer[i]);
  }

  gst_buffer_list_iterator_add (fragment->priv->buffer_iterator, header);
  fragment->priv->has_headers = TRUE;
  return TRUE;
}

gboolean
gst_fragment_add_buffer (GstFragment * fragment, GstBuffer * buffer)
{
  g_return_val_if_fail (fragment != NULL, FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);

  if (fragment->completed) {
    GST_WARNING ("Fragment is completed, could not add more buffers");
    return FALSE;
  }

  /* if this is the first buffer forbid setting the headers */
  if (G_UNLIKELY (fragment->priv->can_set_headers)) {
    fragment->priv->can_set_headers = FALSE;
  }

  GST_DEBUG ("Adding new buffer to the fragment");
  gst_buffer_ref (buffer);
  gst_buffer_list_iterator_add (fragment->priv->buffer_iterator, buffer);
  fragment->size += GST_BUFFER_SIZE (buffer);
  return TRUE;
}

void
gst_fragment_set_name (GstFragment * fragment, gchar * name)
{
  g_return_if_fail (GST_IS_FRAGMENT (fragment));

  if (fragment->name != NULL)
    g_free (fragment->name);

  fragment->name = name;
}

guint64
gst_fragment_get_duration (GstFragment * fragment)
{
  g_return_val_if_fail (GST_IS_FRAGMENT (fragment), GST_CLOCK_TIME_NONE);

  if (!(GST_CLOCK_TIME_IS_VALID (fragment->start_ts) &&
          GST_CLOCK_TIME_IS_VALID (fragment->stop_ts)))
    return GST_CLOCK_TIME_NONE;

  return fragment->stop_ts - fragment->start_ts;
}
