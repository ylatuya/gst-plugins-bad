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

#include "gstjniutils.h"
#include "gstamcsurface.h"

G_DEFINE_TYPE (GstAmcSurface, gst_amc_surface, G_TYPE_OBJECT);

static gpointer parent_class = NULL;
static void gst_amc_surface_dispose (GObject * object);

static gboolean
_cache_java_class (GstAmcSurfaceClass * klass)
{
  JNIEnv *env;

  gst_amc_jni_initialize ();

  env = gst_amc_jni_get_env ();

  klass->jklass = gst_amc_jni_get_class (env, "android/view/Surface");
  if (!klass->jklass)
    return FALSE;

  klass->constructor = gst_amc_jni_get_method (env, klass->jklass, "<init>",
      "(Landroid/graphics/SurfaceTexture;)V");
  klass->is_valid =
      gst_amc_jni_get_method (env, klass->jklass, "isValid", "()Z");
  klass->release =
      gst_amc_jni_get_method (env, klass->jklass, "release", "()V");
  klass->describe_contents =
      gst_amc_jni_get_method (env, klass->jklass, "describeContents", "()I");
  if (!klass->constructor || !klass->is_valid || !klass->release
      || !klass->describe_contents) {
    return FALSE;
  }

  return TRUE;
}


static void
gst_amc_surface_class_init (GstAmcSurfaceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  gobject_class->dispose = gst_amc_surface_dispose;

  if (!_cache_java_class (klass)) {
    g_critical ("Could not cache java class android/view/Surface");
  }
}

static void
gst_amc_surface_init (GstAmcSurface * self)
{
  /* initialize the object */
}

static void
gst_amc_surface_dispose (GObject * object)
{
  GstAmcSurface *self;
  JNIEnv *env;

  self = GST_AMC_SURFACE (object);
  env = gst_amc_jni_get_env ();

  gst_amc_surface_release (self);

  if (self->jobject) {
    gst_amc_jni_object_unref (env, self->jobject);
  }

  if (self->texture != NULL) {
    g_object_unref (self->texture);
    self->texture = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

GstAmcSurface *
gst_amc_surface_new (GstAmcSurfaceTexture * texture)
{
  GstAmcSurface *surface;
  GstAmcSurfaceClass *klass;
  JNIEnv *env;

  surface = g_object_new (GST_TYPE_AMC_SURFACE, NULL);
  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (surface);

  surface->jobject = gst_amc_jni_new_object (env, klass->jklass,
      klass->constructor, texture->jobject);
  if (surface->jobject == NULL) {
    g_object_unref (surface);
    return NULL;
  }

  surface->texture = texture;
  return surface;
}

gboolean
gst_amc_surface_is_valid (GstAmcSurface * self)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  return (*env)->CallBooleanMethod (env, self->jobject, klass->is_valid);
}

void
gst_amc_surface_release (GstAmcSurface * self)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  (*env)->CallVoidMethod (env, self->jobject, klass->release);
}

gint
gst_amc_surface_describe_contents (GstAmcSurface * self)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  return (*env)->CallIntMethod (env, self->jobject, klass->describe_contents);
}
