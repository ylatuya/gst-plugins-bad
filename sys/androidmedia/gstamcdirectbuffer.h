/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_amc_jni_DIRECT_BUFFER_H__
#define __GST_amc_jni_DIRECT_BUFFER_H__

#include <gst/gst.h>
#include "gstjniutils.h"
#include "gstamcsurfacetexture.h"

G_BEGIN_DECLS

typedef struct _GstAmcDirectBuffer GstAmcDirectBuffer;

struct _GstAmcDirectBuffer {
  GstAmcSurfaceTexture *texture;
  jobject media_codec;
  jmethodID release_output_buffer;
  guint idx;
  gboolean released;
};

GstAmcDirectBuffer * gst_amc_direct_buffer_new (
    GstAmcSurfaceTexture *texture, jobject media_codec,
    jmethodID release_output_buffer, guint idx);

GstAmcDirectBuffer * gst_amc_direct_buffer_from_gst_buffer (GstBuffer *buf);

GstBuffer * gst_amc_direct_buffer_get_gst_buffer (GstAmcDirectBuffer * drbuf);

void gst_amc_direct_buffer_free (GstAmcDirectBuffer *buf);

gboolean gst_amc_direct_buffer_render (GstAmcDirectBuffer *buf);

G_END_DECLS
#endif
