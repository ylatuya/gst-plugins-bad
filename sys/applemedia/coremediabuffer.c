/*
 * Copyright (C) 2009 Ole André Vadla Ravnås <oravnas@cisco.com>
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

#include "coremediabuffer.h"

G_DEFINE_TYPE (GstCoreMediaBuffer, gst_core_media_buffer, GST_TYPE_BUFFER);

static void
gst_core_media_buffer_init (GstCoreMediaBuffer * self)
{
  GST_BUFFER_FLAG_SET (self, GST_BUFFER_FLAG_READONLY);
}

static void
gst_core_media_buffer_finalize (GstMiniObject * mini_object)
{
  GstCoreMediaBuffer *self = GST_CORE_MEDIA_BUFFER_CAST (mini_object);

  if (self->image_buf != NULL) {
    CVPixelBufferUnlockBaseAddress (self->image_buf,
        kCVPixelBufferLock_ReadOnly);
  }
  CFRelease (self->sample_buf);

  GST_MINI_OBJECT_CLASS (gst_core_media_buffer_parent_class)->finalize
      (mini_object);
}

GstBuffer *
gst_core_media_buffer_new (CMSampleBufferRef sample_buf)
{
  CVImageBufferRef image_buf;
  CVPixelBufferRef pixel_buf;
  CMBlockBufferRef block_buf;
  char *data = NULL;
  UInt32 size;
  OSStatus status;
  GstCoreMediaBuffer *buf;

  image_buf = CMSampleBufferGetImageBuffer (sample_buf);
  pixel_buf = NULL;
  block_buf = CMSampleBufferGetDataBuffer (sample_buf);

  if (image_buf != NULL &&
      CFGetTypeID (image_buf) == CVPixelBufferGetTypeID ()) {
    pixel_buf = (CVPixelBufferRef) image_buf;

    if (CVPixelBufferLockBaseAddress (pixel_buf,
            kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
      goto error;
    }

    if (CVPixelBufferIsPlanar (pixel_buf)) {
      gint plane_count, plane_idx;

      data = CVPixelBufferGetBaseAddressOfPlane (pixel_buf, 0);

      size = 0;
      plane_count = CVPixelBufferGetPlaneCount (pixel_buf);
      for (plane_idx = 0; plane_idx != plane_count; plane_idx++) {
        size += CVPixelBufferGetBytesPerRowOfPlane (pixel_buf, plane_idx) *
            CVPixelBufferGetHeightOfPlane (pixel_buf, plane_idx);
      }
    } else {
      data = CVPixelBufferGetBaseAddress (pixel_buf);
      size = CVPixelBufferGetBytesPerRow (pixel_buf) *
          CVPixelBufferGetHeight (pixel_buf);
    }
  } else if (block_buf != NULL) {
    status = CMBlockBufferGetDataPointer (block_buf, 0, 0, 0, &data);
    if (status != noErr)
      goto error;
    size = CMBlockBufferGetDataLength (block_buf);
  } else {
    goto error;
  }

  buf =
      GST_CORE_MEDIA_BUFFER (gst_mini_object_new (GST_TYPE_CORE_MEDIA_BUFFER));
  buf->sample_buf = CFRetain (sample_buf);
  buf->image_buf = image_buf;
  buf->pixel_buf = pixel_buf;
  buf->block_buf = block_buf;

  GST_BUFFER_DATA (buf) = data;
  GST_BUFFER_SIZE (buf) = size;

  return GST_BUFFER_CAST (buf);

error:
  return NULL;
}

CVPixelBufferRef
gst_core_media_buffer_get_pixel_buffer (GstCoreMediaBuffer * buf)
{
  return CVPixelBufferRetain (buf->pixel_buf);
}

static void
gst_core_media_buffer_class_init (GstCoreMediaBufferClass * klass)
{
  GstMiniObjectClass *miniobject_class = GST_MINI_OBJECT_CLASS (klass);

  miniobject_class->finalize = gst_core_media_buffer_finalize;
}
