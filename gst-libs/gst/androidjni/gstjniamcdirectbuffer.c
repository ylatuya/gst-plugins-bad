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

#include "gstjniamcdirectbuffer.h"

GstJniAmcDirectBuffer *
gst_jni_amc_direct_buffer_new (GstJniSurfaceTexture * texture,
    jobject media_codec, jmethodID release_output_buffer, guint idx)
{
  GstJniAmcDirectBuffer *buf;

  buf = g_new0 (GstJniAmcDirectBuffer, 1);
  buf->texture = g_object_ref (texture);
  buf->media_codec = media_codec;
  buf->release_output_buffer = release_output_buffer;
  buf->idx = idx;
  buf->released = FALSE;

  return buf;
}

GstJniAmcDirectBuffer *
gst_jni_amc_direct_buffer_from_gst_buffer (GstBuffer * buffer)
{
  return (GstJniAmcDirectBuffer *) GST_BUFFER_DATA (buffer);
}

GstBuffer *
gst_jni_amc_direct_buffer_get_gst_buffer (GstJniAmcDirectBuffer * drbuf)
{
  GstBuffer *buf;

  g_return_val_if_fail (drbuf != NULL, NULL);

  buf = gst_buffer_new ();
  GST_BUFFER_SIZE (buf) = sizeof (GstJniAmcDirectBuffer *);
  GST_BUFFER_DATA (buf) = (guint8 *) drbuf;
  GST_BUFFER_MALLOCDATA (buf) = (guint8 *) drbuf;
  GST_BUFFER_FREE_FUNC (buf) = (GFreeFunc) gst_jni_amc_direct_buffer_free;

  return buf;
}

gboolean
gst_jni_amc_direct_buffer_render (GstJniAmcDirectBuffer * buf)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (buf != NULL, FALSE);

  if (!buf->released) {
    JNIEnv *env;

    env = gst_jni_get_env ();
    ret = gst_jni_call_void_method (env, buf->media_codec,
        buf->release_output_buffer, buf->idx, TRUE);
    if (ret)
      buf->released = TRUE;
  }

  return ret;
}

void
gst_jni_amc_direct_buffer_free (GstJniAmcDirectBuffer * buf)
{
  JNIEnv *env;

  g_return_if_fail (buf != NULL);

  env = gst_jni_get_env ();

  if (buf->texture != NULL) {
    g_object_unref (buf->texture);
    buf->texture = NULL;
  }

  if (!buf->released) {
    gst_jni_call_void_method (env, buf->media_codec,
        buf->release_output_buffer, buf->idx, FALSE);
  }
  g_free (buf);
}
