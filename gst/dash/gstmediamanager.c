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

static void gst_media_manager_finalize (GObject * gobject);

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
static void gst_media_manager_clear (GstBaseMediaManager * manager);

G_DEFINE_TYPE (GstMediaManager, gst_media_manager, GST_TYPE_BASE_MEDIA_MANAGER);

static void
gst_media_manager_class_init (GstMediaManagerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseMediaManagerClass *manager_class =
      GST_BASE_MEDIA_MANAGER_CLASS (klass);

  manager_class->add_stream = gst_media_manager_add_stream;
  manager_class->remove_stream = gst_media_manager_remove_stream;
  manager_class->add_fragment = gst_media_manager_add_fragment;
  manager_class->add_headers = gst_media_manager_add_headers;
  manager_class->render = gst_media_manager_render;
  manager_class->clear = gst_media_manager_clear;

  gobject_class->finalize = gst_media_manager_finalize;
}

static void
gst_media_manager_init (GstMediaManager * manager)
{
  GstBaseMediaManager *base_manager;

  base_manager = GST_BASE_MEDIA_MANAGER (manager);
  gst_base_media_manager_set_base_url (base_manager, g_strdup ("./"));
  gst_base_media_manager_set_title (base_manager, g_strdup ("dash"));
  gst_base_media_manager_set_fragment_prefix (base_manager, g_strdup
      ("segment"));
  base_manager->chunked = TRUE;
  base_manager->is_live = FALSE;
  base_manager->finished = FALSE;
  base_manager->window_size = GST_CLOCK_TIME_NONE;

  manager->mdp = gst_media_presentation_new (MEDIA_PRESENTATION_TYPE_ONDEMAND);
}

static void
gst_media_manager_finalize (GObject * gobject)
{
  GstMediaManager *manager = GST_MEDIA_MANAGER (gobject);

  if (manager->mdp != NULL) {
    gst_media_presentation_free (manager->mdp);
    manager->mdp = NULL;
  }

  G_OBJECT_CLASS (gst_media_manager_parent_class)->finalize (gobject);
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
  gint width = 0, height = 0, par_n = 0, par_d = 0, samplerate = 0;
  gint bitrate = 0;
  gdouble framerate = 0;
  gchar *pad_name;
  gboolean ret;


  manager = GST_MEDIA_MANAGER (b_manager);

  if (g_list_length (substreams_caps) != 1) {
    GST_ERROR ("The stream contains %d muxed streams instead of 1",
        g_list_length (substreams_caps));
    return FALSE;
  }

  s = gst_caps_get_structure (GST_PAD_CAPS (pad), 0);
  pad_mime = gst_structure_get_name (s);

  /* Extract substream metadata from the caps */
  caps = (GstCaps *) g_list_first (substreams_caps)->data;
  s = gst_caps_get_structure (caps, 0);
  mime = gst_structure_get_name (s);

  /* Video metadata */
  gst_structure_get_int (s, "height", &height);
  gst_structure_get_int (s, "width", &width);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);
  if (gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d))
    framerate = ((gdouble) fps_n) / fps_d;

  /* Audio metadata */
  gst_structure_get_int (s, "samplerate", &samplerate);

  /* Stream bitrate */
  gst_structure_get_int (s, "bitrate", &bitrate);

  /* Stream type */
  if (g_str_has_prefix (mime, "audio"))
    type = STREAM_TYPE_AUDIO;
  else if (g_str_has_prefix (mime, "video"))
    type = STREAM_TYPE_VIDEO;
  else {
    GST_ERROR ("The stream doesn't contain audio or video.");
    return FALSE;
  }

  /* FIXME: Add audio channels */
  pad_name = gst_pad_get_name (pad);
  ret = gst_media_presentation_add_stream (manager->mdp, type,
      pad_name, pad_mime, width, height, par_n, par_d, framerate,
      NULL, samplerate, bitrate);
  g_free (pad_name);
  return ret;
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
  gchar *name;

  name = g_strdup_printf ("%s_%s_%s_%d%s", manager->title,
      pad_name, manager->fragment_prefix, fragment->index,
      ISOFF_FRAGMENT_EXTENSION);
  gst_fragment_set_name (fragment, name);
}

static void
gst_media_manager_update_headers_name (GstBaseMediaManager * manager,
    gchar * pad_name, GstFragment * fragment)
{
  gchar *name;

  name = g_strdup_printf ("%s_%s%s", manager->title,
      pad_name, ISOFF_HEADERS_EXTENSION);
  gst_fragment_set_name (fragment, name);
}

static gboolean
gst_media_manager_add_headers (GstBaseMediaManager * b_manager,
    GstPad * pad, GstFragment * fragment)
{
  GstMediaManager *manager;
  gchar *pad_name;
  gboolean ret;

  manager = GST_MEDIA_MANAGER (b_manager);

  pad_name = gst_pad_get_name (pad);
  gst_media_manager_update_headers_name (b_manager, pad_name, fragment);

  ret = gst_media_presentation_set_init_segment (manager->mdp, pad_name,
      fragment->name, fragment->offset, fragment->size);
  g_free (pad_name);

  return ret;
}

static gboolean
gst_media_manager_add_fragment (GstBaseMediaManager * b_manager,
    GstPad * pad, GstFragment * fragment, GList ** removed_fragments)
{
  GstMediaManager *manager;
  gchar *pad_name;
  gboolean ret;

  manager = GST_MEDIA_MANAGER (b_manager);
  pad_name = gst_pad_get_name (pad);
  gst_media_manager_update_fragment_name (b_manager, pad_name, fragment);

  /* FIXME: handled rotating windows */
  *removed_fragments = NULL;

  ret = gst_media_presentation_add_media_segment (manager->mdp,
      pad_name, fragment->name, fragment->index, fragment->start_ts,
      gst_fragment_get_duration (fragment), fragment->offset, fragment->size);
  g_free (pad_name);

  return ret;
}

static void
gst_media_manager_clear (GstBaseMediaManager * b_manager)
{
  GstMediaManager *manager;

  manager = GST_MEDIA_MANAGER (b_manager);
  if (manager->mdp != NULL) {
    gst_media_presentation_free (manager->mdp);
  }

  manager->mdp = gst_media_presentation_new (MEDIA_PRESENTATION_TYPE_ONDEMAND);
}

static GstMediaManagerFile *
gst_media_manager_render (GstBaseMediaManager * b_manager, GstPad * pad)
{
  GstMediaManager *manager;
  GstMediaManagerFile *file;
  GList *base_urls = NULL;

  manager = GST_MEDIA_MANAGER (b_manager);

  /* Update base url */
  base_urls = g_list_append (base_urls, g_strdup (b_manager->base_url));
  gst_media_presentation_set_base_urls (manager->mdp, base_urls);

  file = g_new0 (GstMediaManagerFile, 1);
  file->content = gst_media_presentation_render (manager->mdp);
  file->filename = g_strdup_printf ("%s%s", b_manager->title, MDP_EXTENSION);

  return file;
}
