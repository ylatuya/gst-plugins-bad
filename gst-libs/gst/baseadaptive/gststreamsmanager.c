/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstbasemediamanager.c:
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

#include "gststreamsmanager.h"

G_DEFINE_TYPE (GstStreamsManager, gst_streams_manager, G_TYPE_OBJECT);

static void gst_streams_manager_finalize (GObject * gobject);

static void
gst_streams_manager_class_init (GstStreamsManagerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_streams_manager_finalize;
}

static void
gst_streams_manager_init (GstStreamsManager * bmm)
{
  bmm->base_url = g_strdup (".");
  bmm->title = g_strdup ("");
  bmm->fragment_prefix = g_strdup ("");
  bmm->window_size = 0;
  bmm->with_headers = FALSE;
  bmm->chunked = TRUE;
  bmm->finished = FALSE;
  bmm->with_headers = TRUE;
  bmm->window_size = 0;
  bmm->lock = g_mutex_new ();
}

GstStreamsManager *
gst_streams_manager_new (void)
{
  return GST_STREAMS_MANAGER (g_object_new (GST_TYPE_STREAMS_MANAGER, NULL));
}

static void
gst_streams_manager_finalize (GObject * gobject)
{
  GstStreamsManager *bmm = GST_STREAMS_MANAGER (gobject);

  if (bmm->base_url != NULL) {
    g_free (bmm->base_url);
    bmm->base_url = NULL;
  }

  if (bmm->title != NULL) {
    g_free (bmm->title);
    bmm->title = NULL;
  }

  if (bmm->fragment_prefix != NULL) {
    g_free (bmm->fragment_prefix);
    bmm->fragment_prefix = NULL;
  }

  if (bmm->lock != NULL) {
    g_mutex_free (bmm->lock);
    bmm->lock = NULL;
  }

  G_OBJECT_CLASS (gst_streams_manager_parent_class)->finalize (gobject);
}

GstMediaRepresentationFile *
gst_streams_manager_render (GstStreamsManager * bmm, GstPad * pad)
{
  GstStreamsManagerClass *bclass;
  GstMediaRepresentationFile *mfile = NULL;

  bclass = GST_STREAMS_MANAGER_GET_CLASS (bmm);

  g_mutex_lock (bmm->lock);
  if (bclass->render != NULL)
    mfile = bclass->render (bmm, pad);
  g_mutex_unlock (bmm->lock);

  return mfile;
}

gboolean
gst_streams_manager_add_stream (GstStreamsManager * bmm, GstPad * pad,
    GList * substreams_caps)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (bmm), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (bmm);

  g_mutex_lock (bmm->lock);
  if (bclass->add_stream != NULL)
    ret = bclass->add_stream (bmm, pad, substreams_caps);
  g_mutex_unlock (bmm->lock);

  return ret;
}

gboolean
gst_streams_manager_add_headers (GstStreamsManager * bmm,
    GstPad * pad, GstBuffer * fragment)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (bmm), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);
  g_return_val_if_fail (fragment != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (bmm);

  g_mutex_lock (bmm->lock);
  if (bclass->add_headers)
    ret = bclass->add_headers (bmm, pad, fragment);
  g_mutex_unlock (bmm->lock);

  return ret;
}

gboolean
gst_streams_manager_add_fragment (GstStreamsManager * bmm,
    GstPad * pad, GstBuffer * fragment, GList ** removed_fragments)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (bmm), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);
  g_return_val_if_fail (fragment != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (bmm);

  g_mutex_lock (bmm->lock);
  if (bclass->add_fragment)
    ret = bclass->add_fragment (bmm, pad, fragment, removed_fragments);
  g_mutex_unlock (bmm->lock);

  return ret;
}

void
gst_streams_manager_set_base_url (GstStreamsManager * bmm, gchar * url)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (bmm));

  if (bmm->base_url != NULL)
    g_free (bmm->base_url);

  bmm->base_url = url;
}

void
gst_streams_manager_set_title (GstStreamsManager * bmm, gchar * title)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (bmm));

  if (bmm->title != NULL)
    g_free (bmm->title);

  bmm->title = title;
}

void
gst_streams_manager_set_fragment_prefix (GstStreamsManager * bmm,
    gchar * prefix)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (bmm));

  if (bmm->fragment_prefix != NULL)
    g_free (bmm->fragment_prefix);

  bmm->fragment_prefix = prefix;
}

void
gst_streams_manager_clear (GstStreamsManager * bmm)
{
  GstStreamsManagerClass *bclass;

  g_return_if_fail (GST_IS_STREAMS_MANAGER (bmm));

  bclass = GST_STREAMS_MANAGER_GET_CLASS (bmm);

  g_mutex_lock (bmm->lock);
  if (bclass->clear)
    bclass->clear (bmm);
  g_mutex_unlock (bmm->lock);
  return;
}

void
gst_media_representation_file_free (GstMediaRepresentationFile * file)
{
  g_return_if_fail (file != NULL);

  g_free (file->content);
  g_free (file->filename);
  g_free (file);
}
