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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <gst/androidjni/gstjniutils.h>
#include "gstjnisurfacetexture.h"

G_DEFINE_TYPE (GstJniSurfaceTexture, gst_jni_surface_texture, G_TYPE_OBJECT);

static gpointer parent_class = NULL;
static void gst_jni_surface_texture_dispose (GObject * object);

static gboolean
_cache_java_class (GstJniSurfaceTextureClass * klass)
{
  JNIEnv *env;

  gst_jni_initialize ();
  env = gst_jni_get_env ();

  klass->jklass = gst_jni_get_class (env, "android/graphics/SurfaceTexture");
  if (!klass->jklass)
    return FALSE;

  klass->constructor =
      gst_jni_get_method (env, klass->jklass, "<init>", "(I)V");
  klass->set_on_frame_available_listener =
      gst_jni_get_method (env, klass->jklass, "setOnFrameAvailableListener",
      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V");
  klass->set_default_buffer_size =
      gst_jni_get_method (env, klass->jklass, "setDefaultBufferSize", "(II)V");
  klass->update_tex_image =
      gst_jni_get_method (env, klass->jklass, "updateTexImage", "()V");
  klass->detach_from_gl_context =
      gst_jni_get_method (env, klass->jklass, "detachFromGLContext", "()V");
  klass->attach_to_gl_context =
      gst_jni_get_method (env, klass->jklass, "attachToGLContext", "(I)V");
  klass->get_transform_matrix =
      gst_jni_get_method (env, klass->jklass, "getTransformMatrix", "([F)V");
  klass->get_timestamp =
      gst_jni_get_method (env, klass->jklass, "getTimestamp", "()J");
  klass->release = gst_jni_get_method (env, klass->jklass, "release", "()V");

  if (!klass->constructor || !klass->set_on_frame_available_listener ||
      !klass->set_default_buffer_size || !klass->update_tex_image ||
      !klass->detach_from_gl_context || !klass->attach_to_gl_context ||
      !klass->get_transform_matrix || !klass->get_timestamp
      || !klass->release) {
    return FALSE;
  }

  return TRUE;
}

static void
gst_jni_surface_texture_init (GstJniSurfaceTexture * self)
{
}

static void
gst_jni_surface_texture_class_init (GstJniSurfaceTextureClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  gobject_class->dispose = gst_jni_surface_texture_dispose;

  if (!_cache_java_class (klass)) {
    g_critical ("Could not cache java class android/graphics/SurfaceTexture");
  }
}

static void
gst_jni_surface_texture_dispose (GObject * object)
{
  GstJniSurfaceTexture *self;
  JNIEnv *env;

  self = GST_JNI_SURFACE_TEXTURE (object);
  env = gst_jni_get_env ();

  gst_jni_surface_texture_release (self);

  /* initialize the object */
  if (self->jobject) {
    gst_jni_object_unref (env, self->jobject);
  }
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

GstJniSurfaceTexture *
gst_jni_surface_texture_new (void)
{
  GstJniSurfaceTexture *texture = NULL;
  GstJniSurfaceTextureClass *klass;
  JNIEnv *env;

  texture = g_object_new (GST_TYPE_JNI_SURFACE_TEXTURE, NULL);
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (texture);
  env = gst_jni_get_env ();

  texture->texture_id = 0;

  texture->jobject = gst_jni_new_object (env, klass->jklass,
      klass->constructor, texture->texture_id);
  if (texture->jobject == NULL) {
    goto error;
  }
  gst_jni_surface_texture_detach_from_gl_context (texture);

done:
  return texture;

error:
  if (texture)
    g_object_unref (texture);
  texture = NULL;
  goto done;
}

void
gst_jni_surface_texture_set_default_buffer_size (GstJniSurfaceTexture * self,
    gint width, gint height)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->set_default_buffer_size,
      width, height);
}

void
gst_jni_surface_texture_update_tex_image (GstJniSurfaceTexture * self)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->update_tex_image);
}

void
gst_jni_surface_texture_detach_from_gl_context (GstJniSurfaceTexture * self)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->detach_from_gl_context);
  self->texture_id = 0;
}

void
gst_jni_surface_texture_attach_to_gl_context (GstJniSurfaceTexture * self,
    gint texture_id)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->attach_to_gl_context,
      texture_id);
  self->texture_id = texture_id;
}

void
gst_jni_surface_texture_get_transform_matrix (GstJniSurfaceTexture * self,
    const gfloat * matrix)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;
  /* 4x4 Matrix */
  jsize size = 16;
  jfloatArray floatarray;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  floatarray = (*env)->NewFloatArray (env, size);
  (*env)->CallVoidMethod (env, self->jobject, klass->get_transform_matrix,
      floatarray);
  (*env)->GetFloatArrayRegion (env, floatarray, 0, size, (jfloat *) matrix);
  (*env)->DeleteLocalRef (env, floatarray);
}

gint64
gst_jni_surface_texture_get_timestamp (GstJniSurfaceTexture * self)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  return (gint64) (*env)->CallLongMethod (env, self->jobject,
      klass->get_timestamp);
}

void
gst_jni_surface_texture_release (GstJniSurfaceTexture * self)
{
  JNIEnv *env;
  GstJniSurfaceTextureClass *klass;

  env = gst_jni_get_env ();
  klass = GST_JNI_SURFACE_TEXTURE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->release);
}
