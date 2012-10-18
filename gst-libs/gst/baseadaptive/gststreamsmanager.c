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
#include <gst/pbutils/pbutils.h>

G_DEFINE_TYPE (GstStreamsManager, gst_streams_manager, G_TYPE_OBJECT);

static void gst_streams_manager_finalize (GObject * gobject);

static void
gst_streams_manager_class_init (GstStreamsManagerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_streams_manager_finalize;
}

static void
gst_streams_manager_init (GstStreamsManager * man)
{
  man->base_url = g_strdup (".");
  man->title = g_strdup ("");
  man->fragment_prefix = g_strdup ("");
  man->window_size = 0;
  man->with_headers = FALSE;
  man->chunked = TRUE;
  man->finished = FALSE;
  man->with_headers = TRUE;
  man->window_size = 0;
  g_mutex_init (&man->lock);
}

GstStreamsManager *
gst_streams_manager_new (void)
{
  return GST_STREAMS_MANAGER (g_object_new (GST_TYPE_STREAMS_MANAGER, NULL));
}

static void
gst_streams_manager_finalize (GObject * gobject)
{
  GstStreamsManager *man = GST_STREAMS_MANAGER (gobject);

  if (man->base_url != NULL) {
    g_free (man->base_url);
    man->base_url = NULL;
  }

  if (man->title != NULL) {
    g_free (man->title);
    man->title = NULL;
  }

  if (man->fragment_prefix != NULL) {
    g_free (man->fragment_prefix);
    man->fragment_prefix = NULL;
  }

  g_mutex_clear (&man->lock);

  G_OBJECT_CLASS (gst_streams_manager_parent_class)->finalize (gobject);
}

gboolean
gst_streams_manager_render (GstStreamsManager * man, GstPad * pad,
    GstMediaRepFile ** file)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;;

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  g_mutex_lock (&man->lock);
  if (bclass->render != NULL)
    ret = bclass->render (man, pad, file);
  g_mutex_unlock (&man->lock);

  return ret;
}

gboolean
gst_streams_manager_add_stream (GstStreamsManager * man, GstPad * pad,
    GstDiscovererVideoInfo * video_info,
    GstDiscovererAudioInfo * audio_info,
    GstDiscovererSubtitleInfo * subtitle_info, GstMediaRepFile ** rep_file)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (man), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  *rep_file = NULL;
  g_mutex_lock (&man->lock);
  if (bclass->add_stream != NULL)
    ret = bclass->add_stream (man, pad, video_info, audio_info, subtitle_info,
        rep_file);
  g_mutex_unlock (&man->lock);

  return ret;
}

gboolean
gst_streams_manager_eos (GstStreamsManager * man, GstPad * pad,
    GstMediaRepFile ** rep_file)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  *rep_file = NULL;
  g_mutex_lock (&man->lock);
  if (bclass->eos != NULL)
    ret = bclass->eos (man, pad, rep_file);
  g_mutex_unlock (&man->lock);

  return ret;
}

gboolean
gst_streams_manager_add_headers (GstStreamsManager * man,
    GstPad * pad, GstBuffer * fragment)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (man), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);
  g_return_val_if_fail (fragment != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  g_mutex_lock (&man->lock);
  if (bclass->add_headers)
    ret = bclass->add_headers (man, pad, fragment);
  g_mutex_unlock (&man->lock);

  return ret;
}

gchar *
gst_streams_manager_fragment_name (GstStreamsManager * man,
    GstPad * pad, guint index)
{
  GstStreamsManagerClass *bclass;
  gchar *name;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (man), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  g_mutex_lock (&man->lock);
  if (bclass->fragment_name)
    name = bclass->fragment_name (man, pad, index);
  else
    name = g_strdup_printf ("%s_%s_%d.frag", man->title,
        man->fragment_prefix, index);
  g_mutex_unlock (&man->lock);

  return name;
}

gchar *
gst_streams_manager_headers_name (GstStreamsManager * man, GstPad * pad)
{
  GstStreamsManagerClass *bclass;
  gchar *name;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (man), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  g_mutex_lock (&man->lock);
  if (bclass->headers_name) {
    name = bclass->headers_name (man, pad);
  } else {
    gchar *pad_name;

    pad_name = gst_pad_get_name (pad);
    name = g_strdup_printf ("%s_%s.frag", man->title, pad_name);
    g_free (pad_name);

  }
  g_mutex_unlock (&man->lock);

  return name;
}

gboolean
gst_streams_manager_add_fragment (GstStreamsManager * man,
    GstPad * pad, GstBuffer * fragment, GstMediaRepFile ** rep_file,
    GList ** removed_fragments)
{
  GstStreamsManagerClass *bclass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_STREAMS_MANAGER (man), FALSE);
  g_return_val_if_fail (pad != NULL, FALSE);
  g_return_val_if_fail (fragment != NULL, FALSE);

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  *rep_file = NULL;
  *removed_fragments = NULL;

  g_mutex_lock (&man->lock);
  if (bclass->add_fragment)
    ret = bclass->add_fragment (man, pad, fragment, rep_file,
        removed_fragments);
  g_mutex_unlock (&man->lock);

  return ret;
}

void
gst_streams_manager_set_output_directory (GstStreamsManager * man,
    gchar * output_dir)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (man));

  if (man->output_directory != NULL)
    g_free (man->output_directory);

  man->output_directory = g_strdup (output_dir);
}

void
gst_streams_manager_set_base_url (GstStreamsManager * man, gchar * url)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (man));

  if (man->base_url != NULL)
    g_free (man->base_url);

  man->base_url = g_strdup (url);
}

void
gst_streams_manager_set_title (GstStreamsManager * man, gchar * title)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (man));

  if (man->title != NULL)
    g_free (man->title);

  man->title = g_strdup (title);
}

void
gst_streams_manager_set_fragment_prefix (GstStreamsManager * man,
    gchar * prefix)
{
  g_return_if_fail (GST_IS_STREAMS_MANAGER (man));

  if (man->fragment_prefix != NULL)
    g_free (man->fragment_prefix);

  man->fragment_prefix = g_strdup (prefix);
}

void
gst_streams_manager_clear (GstStreamsManager * man)
{
  GstStreamsManagerClass *bclass;

  g_return_if_fail (GST_IS_STREAMS_MANAGER (man));

  bclass = GST_STREAMS_MANAGER_GET_CLASS (man);

  g_mutex_lock (&man->lock);
  if (bclass->clear)
    bclass->clear (man);
  g_mutex_unlock (&man->lock);
  return;
}

void
gst_media_rep_file_free (GstMediaRepFile * rep_file)
{
  g_return_if_fail (rep_file != NULL);

  g_free (rep_file->content);
  g_object_unref (rep_file->file);
  g_free (rep_file);
}

GstMediaRepFile *
gst_media_rep_file_new (gchar * content, GFile * file)
{
  GstMediaRepFile *rep_file;

  rep_file = g_new0 (GstMediaRepFile, 1);

  rep_file->content = content;
  g_object_ref (file);
  rep_file->file = file;

  return rep_file;
}
