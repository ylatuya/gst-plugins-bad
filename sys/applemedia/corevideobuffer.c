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

#include <gst/video/video.h>
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

static GstVideoFormat
_get_video_format (OSType format)
{
  switch (format) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return GST_VIDEO_FORMAT_NV12;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
      return GST_VIDEO_FORMAT_YUY2;
    case kCVPixelFormatType_422YpCbCr8:
      return GST_VIDEO_FORMAT_UYVY;
    case kCVPixelFormatType_32BGRA:
      return GST_VIDEO_FORMAT_BGRA;
    default:
      GST_WARNING ("Unknown OSType format: %d", (gint) format);
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

GstBuffer *
gst_core_video_buffer_new (CVBufferRef cvbuf)
{
  guint8 *data;
  size_t size;
  CVPixelBufferRef pixbuf = NULL;
  GstCoreVideoBuffer *buf;
  size_t width, height, bytes_per_row;
  OSType format = 0;
  gboolean needs_copy = FALSE;

  if (CFGetTypeID (cvbuf) == CVPixelBufferGetTypeID ()) {
    pixbuf = (CVPixelBufferRef) cvbuf;

    if (CVPixelBufferLockBaseAddress (pixbuf,
            kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
      goto error;
    }

    format = CVPixelBufferGetPixelFormatType (pixbuf);
    width = CVPixelBufferGetWidth(pixbuf);
    height = CVPixelBufferGetHeight(pixbuf);
    bytes_per_row = CVPixelBufferGetBytesPerRow (pixbuf);
    size = gst_video_format_get_size (
        _get_video_format (format), width, height);

    if (height * bytes_per_row != size) {
      needs_copy = TRUE;
    }

    if (CVPixelBufferIsPlanar (pixbuf)) {
      data = CVPixelBufferGetBaseAddressOfPlane (pixbuf, 0);
    }else {
      data = CVPixelBufferGetBaseAddress (pixbuf);
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
    guint8 *out_data;
    gint plane_count, plane_idx;

    out_data = g_malloc0(size);
    ptr = out_data;
    GST_BUFFER_MALLOCDATA(buf) = out_data;
    plane_count = CVPixelBufferGetPlaneCount (pixbuf);

    for (plane_idx = 0; plane_idx < plane_count; plane_idx++) {
      gint row;
      gint row_size = gst_video_format_get_row_stride (_get_video_format (format), plane_idx, width);
      gint height = CVPixelBufferGetHeightOfPlane (pixbuf, plane_idx);
      gint bytes_per_row = CVPixelBufferGetBytesPerRowOfPlane (pixbuf, plane_idx);
      data = CVPixelBufferGetBaseAddressOfPlane (pixbuf, plane_idx);

      for (row = 0; row < height; row++) {
        memcpy (ptr, data, row_size);
        ptr += row_size;
        data += bytes_per_row;
      }
    }
    data = out_data;
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
