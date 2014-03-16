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

#ifndef GST_AMC_VIDEO_META_H
#define GST_AMC_VIDEO_META_H

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include "gstamcsurfacetexture.h"
#include "gstamcdirectbuffer.h"

G_BEGIN_DECLS

typedef struct _GstAmcVideoMemory                GstAmcVideoMemory;

#define GST_AMC_VIDEO_MEMORY_CAST (mem)          ((GstAmcVideoMemory *)(mem))
#define GST_AMC_VIDEO_MEMORY_NAME                "GstAmcVideoMemory"
#define GST_CAPS_FEATURE_MEMORY_AMC_SURFACE      "memory:AMCBuffer"


GstMemory * gst_amc_video_memory_new             (GstAmcSurfaceTexture *surface_texture,
                                                  GstAmcDirectBuffer *direct_buffer);

GstBuffer * gst_amc_video_buffer_new             (GstAmcSurfaceTexture *surface_texture,
                                                  GstAmcDirectBuffer *direct_buffer);

G_END_DECLS
#endif
