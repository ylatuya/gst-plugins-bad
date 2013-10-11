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

#ifndef __GST_AMC_JNI_DIRECT_BUFFER_H__
#define __GST_AMC_JNI_DIRECT_BUFFER_H__

#include <gst/gst.h>
#include "gstjniutils.h"
#include "gstjnisurfacetexture.h"

G_BEGIN_DECLS

typedef struct _GstJniAmcDirectBuffer GstJniAmcDirectBuffer;

struct _GstJniAmcDirectBuffer {
  GstJniSurfaceTexture *texture;
  jobject media_codec;
  jmethodID release_output_buffer;
  guint idx;
  gboolean released;
};

GstJniAmcDirectBuffer * gst_jni_amc_direct_buffer_new (
    GstJniSurfaceTexture *texture, jobject media_codec,
    jmethodID release_output_buffer, guint idx);

GstJniAmcDirectBuffer * gst_jni_amc_direct_buffer_from_gst_buffer (GstBuffer *buf);

GstBuffer * gst_jni_amc_direct_buffer_get_gst_buffer (GstJniAmcDirectBuffer * drbuf);

void gst_jni_amc_direct_buffer_free (GstJniAmcDirectBuffer *buf);

gboolean gst_jni_amc_direct_buffer_render (GstJniAmcDirectBuffer *buf);

G_END_DECLS
#endif
