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

#include "gstamcdirectbuffer.h"

GstAmcDirectBuffer *
gst_amc_direct_buffer_new (GstAmcSurfaceTexture * texture,
    jobject media_codec, jmethodID release_output_buffer, guint idx)
{
  GstAmcDirectBuffer *buf;

  buf = g_new0 (GstAmcDirectBuffer, 1);
  buf->texture = g_object_ref (texture);
  buf->media_codec = media_codec;
  buf->release_output_buffer = release_output_buffer;
  buf->idx = idx;
  buf->released = FALSE;

  return buf;
}

gboolean
gst_amc_direct_buffer_render (GstAmcDirectBuffer * buf)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (buf != NULL, FALSE);

  if (!buf->released) {
    JNIEnv *env;

    env = gst_amc_jni_get_env ();
    ret = gst_amc_jni_call_void_method (env, buf->media_codec,
        buf->release_output_buffer, buf->idx, TRUE);
    if (ret)
      buf->released = TRUE;
  }

  return ret;
}

void
gst_amc_direct_buffer_free (GstAmcDirectBuffer * buf)
{
  JNIEnv *env;

  g_return_if_fail (buf != NULL);

  env = gst_amc_jni_get_env ();

  if (buf->texture != NULL) {
    g_object_unref (buf->texture);
    buf->texture = NULL;
  }

  if (!buf->released) {
    gst_amc_jni_call_void_method (env, buf->media_codec,
        buf->release_output_buffer, buf->idx, FALSE);
  }
  g_free (buf);
}
