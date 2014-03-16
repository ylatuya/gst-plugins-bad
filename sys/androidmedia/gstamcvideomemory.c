/*
 *  gstamcvideometa.h - GStreamer/Android video meta (GLTextureUpload)
 *
 *  Copyright (C) 2014 Andoni Morales <ylatuya@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "gstamcvideomemory.h"

struct _GstAmcVideoMemory
{
  GstMemory parent;
  GstAmcDirectBuffer *direct_buffer;
  GstAmcSurfaceTexture *surface_texture;
};

GstMemory *
gst_amc_video_memory_new (GstAmcSurfaceTexture * surface_texture,
    GstAmcDirectBuffer * direct_buffer)
{
  GstAmcVideoMemory *mem;

  gst_memory_init (&mem->parent, GST_MEMORY_FLAG_NOT_MAPPABLE, NULL, NULL,
      0, 0, 0, 0);

  mem = g_slice_new (GstAmcVideoMemory);
  if (!mem)
    return NULL;

  mem->direct_buffer = direct_buffer;
  mem->surface_texture = surface_texture;
  return mem;
}


void
gst_buffer_set_amc_video_meta (GstBuffer * buffer, GstAmcVideoMeta * meta)
{
  GstMeta *m;

  g_return_if_fail (GST_IS_BUFFER (buffer));
  g_return_if_fail (GST_AMC_IS_VIDEO_META (meta));

  m = gst_buffer_add_meta (buffer, GST_AMC_VIDEO_META_INFO, NULL);
  if (m)
    GST_AMC_VIDEO_META_HOLDER (m)->meta = gst_amc_video_meta_ref (meta);
}

static void
gst_amc_texure_upload_free (gpointer data)
{
  GstAmcDirectBuffer *const buf = data;

  if (buf) {
    gst_amc_direct_buffer_free (buf);
  }
}

static gboolean
gst_amc_texture_upload (GstVideoGLTextureUploadMeta * meta, guint texture_id[4])
{
  GstAmcVideoMeta *const vmeta = gst_buffer_get_amc_video_memory (meta->buffer);
  GstAmcSurfaceTexture *texture = vmeta->surface_texture;
  GstAmcDirectBuffer *drbuf = vmeta->direct_buffer;

  gst_amc_surface_texture_attach_to_gl_context (texture, texture_id[0]);

  if (!gst_amc_direct_buffer_render (drbuf)) {
    return FALSE;
  }
  gst_amc_surface_texture_update_tex_image (texture);
  return TRUE;
}

gboolean
gst_buffer_add_texture_upload_meta (GstBuffer * buffer)
{
  GstVideoGLTextureUploadMeta *meta = NULL;
  GstVideoGLTextureType tex_type[] = { GST_VIDEO_GL_TEXTURE_TYPE_OES };

  if (!buffer)
    return FALSE;

  meta = gst_buffer_add_video_gl_texture_upload_meta (buffer,
      GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_FLIP,
      1, tex_type, gst_amc_texture_upload,
      NULL, NULL, gst_amc_texure_upload_free);
  return meta != NULL;
}

GstBuffer *
gst_amc_video_meta_buffer_new (GstAmcSurfaceTexture * surface_texture,
    GstAmcDirectBuffer * direct_buffer)
{
  GstBuffer *buffer;
  GstAmcVideoMeta *video_meta;

  buffer = gst_buffer_new ();
  video_meta = gst_amc_video_meta_new (surface_texture, direct_buffer);
  gst_buffer_add_texture_upload_meta (buffer);
  gst_buffer_set_amc_video_meta (buffer, video_meta);
  return buffer;
}
