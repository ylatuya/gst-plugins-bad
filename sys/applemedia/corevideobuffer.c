/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oravnas@cisco.com>
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

#include "corevideobuffer.h"

G_DEFINE_TYPE (GstCoreVideoBuffer, gst_core_video_buffer, GST_TYPE_BUFFER);

static void
gst_core_video_buffer_init (GstCoreVideoBuffer * self)
{
  GST_BUFFER_FLAG_SET (self, GST_BUFFER_FLAG_READONLY);
}

static void
gst_core_video_buffer_finalize (GstMiniObject * mini_object)
{
  GstCoreVideoBuffer *self = GST_CORE_VIDEO_BUFFER_CAST (mini_object);

  if (self->pixbuf != NULL) {
    CVPixelBufferUnlockBaseAddress (self->pixbuf, kCVPixelBufferLock_ReadOnly);
  }

  CVBufferRelease (self->cvbuf);

  GST_MINI_OBJECT_CLASS (gst_core_video_buffer_parent_class)->finalize
      (mini_object);
}

GstBuffer *
gst_core_video_buffer_new (CVBufferRef cvbuf)
{
  guint8 *data;
  size_t size;
  CVPixelBufferRef pixbuf = NULL;
  GstCoreVideoBuffer *buf;
  gboolean needs_copy = FALSE;

  if (CFGetTypeID (cvbuf) == CVPixelBufferGetTypeID ()) {
    pixbuf = (CVPixelBufferRef) cvbuf;

    if (CVPixelBufferLockBaseAddress (pixbuf,
            kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
      goto error;
    }
    if (CVPixelBufferIsPlanar (pixbuf)) {
      gint plane_count, plane_idx;

      data = CVPixelBufferGetBaseAddressOfPlane (pixbuf, 0);

      size = 0;
      plane_count = CVPixelBufferGetPlaneCount (pixbuf);
      for (plane_idx = 0; plane_idx != plane_count; plane_idx++) {
        size_t plane_size =
            CVPixelBufferGetBytesPerRowOfPlane (pixbuf, plane_idx) *
            CVPixelBufferGetHeightOfPlane (pixbuf, plane_idx);
        if (plane_idx == 1 && !needs_copy) {
          long plane_stride =
              (guint8 *) CVPixelBufferGetBaseAddressOfPlane (pixbuf, plane_idx) - data;
          if (plane_stride != size) {
            needs_copy = TRUE;
          }
        }
        size += plane_size;
      }
    }else {
      data = CVPixelBufferGetBaseAddress (pixbuf);
      size = CVPixelBufferGetBytesPerRow (pixbuf) *
          CVPixelBufferGetHeight (pixbuf);
    }
  } else {
    /* TODO: Do we need to handle other buffer types? */
    goto error;
  }

  buf = GST_CORE_VIDEO_BUFFER_CAST (gst_mini_object_new
      (GST_TYPE_CORE_VIDEO_BUFFER));
  buf->cvbuf = CVBufferRetain (cvbuf);
  buf->pixbuf = pixbuf;

  if (needs_copy) {
    guint8 *ptr;
    gint plane_count, plane_idx;

    data = g_malloc0(size);
    ptr = data;
    GST_BUFFER_MALLOCDATA(buf) = data;
    plane_count = CVPixelBufferGetPlaneCount (pixbuf);
    for (plane_idx = 0; plane_idx != plane_count; plane_idx++) {
      size_t plane_size =
          CVPixelBufferGetBytesPerRowOfPlane (pixbuf, plane_idx) *
          CVPixelBufferGetHeightOfPlane (pixbuf, plane_idx);
      memcpy(ptr, CVPixelBufferGetBaseAddressOfPlane (pixbuf, plane_idx),
          plane_size);
      ptr += plane_size;
    }
  }

  GST_BUFFER_DATA (buf) = data;
  GST_BUFFER_SIZE (buf) = size;

  return GST_BUFFER_CAST (buf);

error:
  return NULL;
}

static void
gst_core_video_buffer_class_init (GstCoreVideoBufferClass * klass)
{
  GstMiniObjectClass *miniobject_class = GST_MINI_OBJECT_CLASS (klass);

  miniobject_class->finalize = gst_core_video_buffer_finalize;
}
