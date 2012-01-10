/* GStreamer
 * Copyright (C) 2011 Flumotion S.L
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

#include "gstmediamanager.h"
#include "mdp/gstmediapresentation.h"

#define ISOFF_HEADERS_EXTENSION ".m4v"
#define ISOFF_FRAGMENT_EXTENSION ".m4s"
#define MDP_EXTENSION ".mdp"


static gboolean gst_media_manager_add_stream (GstBaseMediaManager * manager,
    GstPad * pad, GList * substreams_caps);
static gboolean gst_media_manager_remove_stream (GstBaseMediaManager * manager,
    GstPad * pad);
static gboolean gst_media_manager_add_fragment (GstBaseMediaManager * manager,
    GstPad * pad, GstFragment * fragment, GList ** removed_fragments);
static gboolean gst_media_manager_add_headers (GstBaseMediaManager * manager,
    GstPad * pad, GstFragment * fragment);
static GstMediaManagerFile *gst_media_manager_render (GstBaseMediaManager *
    manager, GstPad * pad);

G_DEFINE_TYPE (GstMediaManager, gst_media_manager, GST_TYPE_BASE_MEDIA_MANAGER);

static void
gst_media_manager_class_init (GstMediaManagerClass * klass)
{
  GstBaseMediaManagerClass *manager_class =
      GST_BASE_MEDIA_MANAGER_CLASS (klass);

  manager_class->add_stream = gst_media_manager_add_stream;
  manager_class->remove_stream = gst_media_manager_remove_stream;
  manager_class->add_fragment = gst_media_manager_add_fragment;
  manager_class->add_headers = gst_media_manager_add_headers;
  manager_class->render = gst_media_manager_render;
}

static void
gst_media_manager_init (GstMediaManager * manager)
{
  GstBaseMediaManager *base_manager;

  base_manager = GST_BASE_MEDIA_MANAGER (manager);
  base_manager->base_url = g_strdup ("./");
  base_manager->title = g_strdup ("dash");
  base_manager->fragment_prefix = g_strdup ("segment");
  base_manager->chunked = TRUE;
  base_manager->is_live = FALSE;
  base_manager->finished = FALSE;
  base_manager->window_size = GST_CLOCK_TIME_NONE;

  manager->mdp = gst_media_presentation_new (MEDIA_PRESENTATION_TYPE_ONDEMAND);
}

GstMediaManager *
gst_media_manager_new (void)
{
  GstMediaManager *manager;

  manager = GST_MEDIA_MANAGER (g_object_new (GST_TYPE_MEDIA_MANAGER, NULL));
  return manager;
}


static gboolean
gst_media_manager_add_stream (GstBaseMediaManager * b_manager, GstPad * pad,
    GList * substreams_caps)
{
  GstMediaManager *manager;
  GstCaps *caps;
  GstStructure *s;
  const gchar *pad_mime;
  const gchar *mime;
  StreamType type;
  gint fps_n, fps_d;
  gint width = 0;
  gint height = 0;
  gint par_n = 0;
  gint par_d = 0;
  gint samplerate = 0;
  gdouble framerate = 0;


  manager = GST_MEDIA_MANAGER (b_manager);

  if (g_list_length (substreams_caps) != 1) {
    GST_ERROR ("The stream contains %d muxed streams instead of 1",
        g_list_length (substreams_caps));
    return FALSE;
  }

  s = gst_caps_get_structure (gst_pad_get_caps (pad), 0);
  pad_mime = gst_structure_get_name (s);

  /* Extract substream metadata from the caps */
  caps = (GstCaps *) g_list_first (substreams_caps)->data;
  s = gst_caps_get_structure (caps, 0);
  mime = gst_structure_get_name (s);

  gst_structure_get_int (s, "height", &height);
  gst_structure_get_int (s, "width", &width);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);
  if (gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d))
    framerate = ((gdouble) fps_n) / fps_d;

  gst_structure_get_int (s, "samplerate", &samplerate);

  if (g_str_has_prefix (mime, "audio"))
    type = STREAM_TYPE_AUDIO;
  else if (g_str_has_prefix (mime, "video"))
    type = STREAM_TYPE_VIDEO;
  else {
    GST_ERROR ("The stream doesn't contain audio or video.");
    return FALSE;
  }

  /* FIXME: Add audio channels */
  return gst_media_presentation_add_stream (manager->mdp, type,
      gst_pad_get_name (pad), pad_mime, width,
      height, par_n, par_d, framerate, NULL, samplerate);
}

static gboolean
gst_media_manager_remove_stream (GstBaseMediaManager * b_manager, GstPad * pad)
{
  return FALSE;
}

static void
gst_media_manager_update_fragment_name (GstBaseMediaManager * manager,
    gchar * pad_name, GstFragment * fragment)
{
  fragment->name = g_strdup_printf ("%s_%s_%s_%d%s", manager->title,
      pad_name, manager->fragment_prefix, fragment->index,
      ISOFF_FRAGMENT_EXTENSION);
}

static void
gst_media_manager_update_headers_name (GstBaseMediaManager * manager,
    gchar * pad_name, GstFragment * fragment)
{
  fragment->name = g_strdup_printf ("%s_%s%s", manager->title,
      pad_name, ISOFF_HEADERS_EXTENSION);
}

static gboolean
gst_media_manager_add_headers (GstBaseMediaManager * b_manager,
    GstPad * pad, GstFragment * fragment)
{
  GstMediaManager *manager;
  gchar *pad_name;

  manager = GST_MEDIA_MANAGER (b_manager);

  pad_name = gst_pad_get_name (pad);
  gst_media_manager_update_headers_name (b_manager, pad_name, fragment);

  return gst_media_presentation_set_init_segment (manager->mdp, pad_name,
      fragment->name, fragment->offset, fragment->size);
}

static gboolean
gst_media_manager_add_fragment (GstBaseMediaManager * b_manager,
    GstPad * pad, GstFragment * fragment, GList ** removed_fragments)
{
  GstMediaManager *manager;
  gchar *pad_name;

  manager = GST_MEDIA_MANAGER (b_manager);
  pad_name = gst_pad_get_name (pad);
  gst_media_manager_update_fragment_name (b_manager, pad_name, fragment);

  /* FIXME: handled rotating windows */
  *removed_fragments = NULL;

  return gst_media_presentation_add_media_segment (manager->mdp,
      pad_name, fragment->name, fragment->index, fragment->start_ts,
      fragment->stop_ts - fragment->start_ts, fragment->offset, fragment->size);
}

static GstMediaManagerFile *
gst_media_manager_render (GstBaseMediaManager * b_manager, GstPad * pad)
{
  GstMediaManager *manager;
  GstMediaManagerFile *file;

  manager = GST_MEDIA_MANAGER (b_manager);

  file = g_new0 (GstMediaManagerFile, 1);
  file->content = gst_media_presentation_render (manager->mdp);
  file->filename = g_strdup_printf ("%s%s", b_manager->title, MDP_EXTENSION);

  return file;
}
